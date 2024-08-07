/*
    Synthseeker forked NINJAM Server - ninjamsrv.cpp
	
	Forked modification Copyright (C) 2024 Jeff Hopkins
	Original file Copyright (C) 2005 and onward Cockos Incorporated

    NINJAM is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    NINJAM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NINJAM; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*

  This file does the setup and configuration file management for the server. 
  Note that the kernel of the server is basically in usercon.cpp/.h, which 
  includes a User_Connection class (manages a user) and a User_Group class 
  (manages a jam).

*/



#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif
#include <signal.h>
#include <stdarg.h>

#include "../../WDL/jnetlib/jnetlib.h"
#include "../../WDL/jnetlib/httpget.h"
#include "../netmsg.h"
#include "../mpb.h"
#include "usercon.h"

#include "../../WDL/rng.h"
#include "../../WDL/sha.h"
#include "../../WDL/lineparse.h"
#include "../../WDL/ptrlist.h"
#include "../../WDL/wdlstring.h"
#include "../../WDL/wdlcstring.h"
#include "../../WDL/assocarray.h"

#define VERSION "v0.082"


const char *startupmessage="Synthseeker Fork of the NINJAM Server " VERSION " built on " __DATE__ " at " __TIME__ " starting up...\n";

int g_set_uid=-1;
int g_default_bpm,g_default_bpi;
FILE *g_logfp;
WDL_String g_pidfilename;
WDL_String g_logfilename;
WDL_String g_status_pass,g_status_user;
WDL_String g_server_pass;

User_Group *m_group; // used normally, but in privategroup mode, this is a lobby
static void delGroup(User_Group *g) { delete g; }
WDL_StringKeyedArray<User_Group *> g_private_groups(false,delGroup);

JNL_Listen *m_listener;
void onConfigChange(int argc, char **argv);
void logText(const char *s, ...);

class UserPassEntry
{
public:
  UserPassEntry() {priv_flag=0;} 
  ~UserPassEntry() {} 
  WDL_String name, pass;
  unsigned int priv_flag;
};


#define ACL_FLAG_DENY 1
#define ACL_FLAG_RESERVE 2
typedef struct
{
  unsigned int addr;
  unsigned int mask;
  int flags;
} ACLEntry;


WDL_HeapBuf g_acllist;
void aclAdd(unsigned int addr, unsigned int mask, int flags)
{
  addr=ntohl(addr);
//  printf("adding acl entry for %08x + %08x\n",addr,mask);
  ACLEntry f={addr,mask,flags};
  int os=g_acllist.GetSize();
  g_acllist.Resize(os+sizeof(f));
  memcpy((char *)g_acllist.Get()+os,&f,sizeof(f));
}

int aclGet(unsigned int addr)
{
  addr=ntohl(addr);

  ACLEntry *p=(ACLEntry *)g_acllist.Get();
  int x=g_acllist.GetSize()/sizeof(ACLEntry);
  while (x--)
  {
  //  printf("comparing %08x to %08x\n",addr,p->addr);
    if ((addr & p->mask) == p->addr) return p->flags;
    p++;
  }
  return 0;
}


int g_config_private_maxsz; // if >0, in private multigroup mode (do not use m_group!)
int g_config_private_maxlobbysz;
int g_config_private_allowchat;
WDL_PtrList<UserPassEntry> g_userlist;
int g_config_allow_anonchat;
int g_config_port;
bool g_config_allowanonymous;
bool g_config_allowanonymous_multi;
bool g_config_anonymous_mask_ip;
int g_config_maxch_anon;
int g_config_maxch_user;
WDL_String g_config_logpath;
int g_config_log_sessionlen;

int g_config_max_users; // these all must be copied to User_Group
int g_config_keepalive;
WDL_FastString g_config_motdfile, g_config_private_lobby_motdfile;
WDL_FastString g_config_default_topic;
WDL_FastString g_config_private_publicprefix;
int g_config_voting_threshold, g_config_voting_timeout;
bool g_config_allow_hidden_users;

static void copyConfigToGroup(User_Group *group)
{
  if (group == m_group && g_config_private_maxsz > 0)
  {
    group->m_is_lobby_mode = 1;
    if (g_config_private_allowchat) group->m_is_lobby_mode |= LOBBY_ALLOW_CHAT;
    group->m_max_users = g_config_private_maxlobbysz;
    group->m_motdfile.Set(g_config_private_lobby_motdfile.Get());
  }
  else
  {
    group->m_max_users = g_config_max_users;
    group->m_motdfile.Set(g_config_motdfile.Get());
  }
  group->m_keepalive = g_config_keepalive;
  if (!group->m_topictext.GetLength()) group->m_topictext.Set(g_config_default_topic.Get());
  group->m_allow_hidden_users = g_config_allow_hidden_users;
  group->m_voting_threshold = g_config_voting_threshold;
  group->m_voting_timeout = g_config_voting_timeout;
}

time_t next_session_update_time;

WDL_String g_config_license;

class localUserInfoLookup : public IUserInfoLookup
{
public:
  localUserInfoLookup(char *name)
  {
    username.Set(name);
  }
  ~localUserInfoLookup()
  {
  }

  int Run()
  {
	logText("received login request for '%s'\n",username.Get());
	user_valid=1;
	reqpass=1;
	WDL_SHA1 shatmp;
	shatmp.add(username.Get(),strlen(username.Get()));
	shatmp.add(":",1);
	shatmp.add(g_server_pass.Get(),g_server_pass.GetLength());
	shatmp.result(sha1buf_user);
	privs=~0 -64; 
	max_channels=g_config_maxch_user;
	return 1;
  }

};


static IUserInfoLookup *myCreateUserLookup(char *username)
{
  return new localUserInfoLookup(username);
}

static int ConfigOnToken(LineParser *lp, bool is_init)
{
  const char *t=lp->gettoken_str(0);
  if (!stricmp(t,"Port"))
  {
    if (lp->getnumtokens() != 2) return -1;
    int p=lp->gettoken_int(1);
    if (!p) return -2;
    g_config_port=p;
  }
  else if (!stricmp(t,"StatusUserPass"))
  {
    if (lp->getnumtokens() != 3) return -1;
    g_status_user.Set(lp->gettoken_str(1));
    g_status_pass.Set(lp->gettoken_str(2));
  }
  else if (!stricmp(t,"MaxUsers"))
  {
    if (lp->getnumtokens() != 2) return -1;
    int p=lp->gettoken_int(1);
    g_config_max_users = p;
  }  
  else if (!stricmp(t,"PIDFile"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_pidfilename.Set(lp->gettoken_str(1));    
  }
  else if (!stricmp(t,"LogFile"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_logfilename.Set(lp->gettoken_str(1));    
  }
  else if (!stricmp(t,"MOTDFile"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_motdfile.Set(lp->gettoken_str(1));
  }
  else if (!stricmp(t,"SessionArchive"))
  {
    if (lp->getnumtokens() != 3) return -1;
    g_config_logpath.Set(lp->gettoken_str(1));    
    g_config_log_sessionlen = lp->gettoken_int(2);
  }
  else if (!stricmp(t,"SetUID"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_set_uid = lp->gettoken_int(1);
  }
  else if (!stricmp(t,"DefaultBPI"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_default_bpi=lp->gettoken_int(1);
    if (g_default_bpi<MIN_BPI) g_default_bpi=MIN_BPI;
    else if (g_default_bpi > MAX_BPI) g_default_bpi=MAX_BPI;
  }
  else if (!stricmp(t,"DefaultBPM"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_default_bpm=lp->gettoken_int(1);
    if (g_default_bpm<MIN_BPM) g_default_bpm=MIN_BPM;
    else if (g_default_bpm > MAX_BPM) g_default_bpm=MAX_BPM;
  }
  else if (!stricmp(t,"DefaultTopic"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_default_topic.Set(lp->gettoken_str(1));
  }
  else if (!stricmp(t,"ServerPassword"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_server_pass.Set(lp->gettoken_str(1));
  }
  else if (!stricmp(t,"MaxChannels"))
  {
    if (lp->getnumtokens() != 2 && lp->getnumtokens() != 3) return -1;
    
    g_config_maxch_user=lp->gettoken_int(1);
    g_config_maxch_anon=lp->gettoken_int(lp->getnumtokens()>2?2:1);
  }
  else if (!stricmp(t,"SetKeepAlive"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_keepalive=lp->gettoken_int(1);
    if (g_config_keepalive < 0 || g_config_keepalive > 255) g_config_keepalive=0;
  }
  else if (!stricmp(t,"SetVotingThreshold"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_voting_threshold = lp->gettoken_int(1);
  }
  else if (!stricmp(t,"SetVotingVoteTimeout"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_voting_timeout = lp->gettoken_int(1);
  }
  else if (!stricmp(t,"ServerLicense"))
  {
    if (lp->getnumtokens() != 2) return -1;
    FILE *fp=fopen(lp->gettoken_str(1),"rt");
    if (!fp) 
    {
      printf("Error opening license file %s\n",lp->gettoken_str(1));
      if (g_logfp)
        logText("Error opening license file %s\n",lp->gettoken_str(1));
      return -2;
    }
    g_config_license.Set("");
    for (;;)
    {
      char buf[1024];
      if (!fgets(buf,sizeof(buf),fp)) break;
      WDL_remove_trailing_crlf(buf);
      g_config_license.Append(buf);
      g_config_license.Append("\n");
    }

    fclose(fp);
    
  }
  else if (!stricmp(t,"ACL"))
  {
    if (lp->getnumtokens() != 3) return -1;
    int suc=0;
    const char *v=lp->gettoken_str(1);
    char buf[256];
    size_t vlen = strlen(v)+1;
    memcpy(buf,v,vlen < sizeof(buf) ? vlen : sizeof(buf));
    buf[sizeof(buf)-1]=0;
    char *t=strstr(buf,"/");
    if (t)
    {
      *t++=0;
      unsigned int addr=JNL::ipstr_to_addr(buf);
      if (addr != INADDR_NONE)
      {
        int maskbits=atoi(t);
        if (maskbits >= 0 && maskbits <= 32)
        {
          int flag=lp->gettoken_enum(2,"allow\0deny\0reserve\0");
          if (flag >= 0)
          {
            suc=1;
            unsigned int mask=~(0xffffffff>>maskbits);
            aclAdd(addr,mask,flag);
          }
        }
      }
    }

    if (!suc)
    {
      if (g_logfp)
        logText("Usage: ACL xx.xx.xx.xx/X [ban|allow|reserve]\n");
      printf("Usage: ACL xx.xx.xx.xx/X [ban|allow|reserve]\n");
      return -2;
    }
  }
  else if (!stricmp(t,"User"))
  {
    if (lp->getnumtokens() != 3 && lp->getnumtokens() != 4) return -1;
    UserPassEntry *p=new UserPassEntry;
    p->name.Set(lp->gettoken_str(1));
    p->pass.Set(lp->gettoken_str(2));
    if (lp->getnumtokens()>3)
    {
      const char *ptr=lp->gettoken_str(3);
      while (*ptr)
      {
        if (*ptr == '*') p->priv_flag|=~PRIV_HIDDEN; // everything but hidden if * used
        else if (*ptr == 'T' || *ptr == 't') p->priv_flag |= PRIV_TOPIC;
        else if (*ptr == 'B' || *ptr == 'b') p->priv_flag |= PRIV_BPM;
        else if (*ptr == 'C' || *ptr == 'c') p->priv_flag |= PRIV_CHATSEND;
        else if (*ptr == 'K' || *ptr == 'k') p->priv_flag |= PRIV_KICK;        
        else if (*ptr == 'R' || *ptr == 'r') p->priv_flag |= PRIV_RESERVE;        
        else if (*ptr == 'M' || *ptr == 'm') p->priv_flag |= PRIV_ALLOWMULTI;
        else if (*ptr == 'H' || *ptr == 'h') p->priv_flag |= PRIV_HIDDEN;       
        else if (*ptr == 'V' || *ptr == 'v') p->priv_flag |= PRIV_VOTE;               
        else if (*ptr == 'P' || *ptr == 'p') p->priv_flag |= PRIV_SHOW_PRIVATE;
        else 
        {
          if (g_logfp)
            logText("Warning: Unknown user privilege flag '%c'\n",*ptr);
          printf("Warning: Unknown user privilege flag '%c'\n",*ptr);
        }
        ptr++;
      }
    }
    else p->priv_flag=PRIV_CHATSEND|PRIV_VOTE;// default privs
    g_userlist.Add(p);
  }
  else if (!stricmp(t,"AllowHiddenUsers"))
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    g_config_allow_hidden_users = !!x;
  }
  else if (!stricmp(t,"AnonymousUsers"))
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0multi\0");
    if (x <0)
    {
      return -2;
    }
    g_config_allowanonymous=!!x;
    g_config_allowanonymous_multi=x==2;
  }
  else if (!stricmp(t,"AnonymousMaskIP"))
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    g_config_anonymous_mask_ip=!!x;
  }
  else if (!stricmp(t,"AnonymousUsers"))
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    g_config_allowanonymous=!!x;
  }  
  else if (!stricmp(t,"AnonymousUsersCanChat"))
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    g_config_allow_anonchat=!!x;
  }  
  else if (!stricmp(t,"PrivateGroupMode"))
  {
    if (lp->getnumtokens() != 2) return -1;
    if (is_init || ((g_config_private_maxsz>0) == (lp->gettoken_int(1)>0)))
    {
      g_config_private_maxsz = lp->gettoken_int(1);
      if (g_config_private_maxsz < 1) return -2;
    }
  }
  else if (!stricmp(t,"PrivateGroupPublicPrefix"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_private_publicprefix.Set(lp->gettoken_str(1));
  }
  else if (!stricmp(t,"PrivateGroupLobbySize"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_private_maxlobbysz = lp->gettoken_int(1);
    if (g_config_private_maxlobbysz < 0) return -2;
  }
  else if (!stricmp(t,"PrivateGroupAllowChat"))
  {
    if (lp->getnumtokens() != 2) return -1;

    int x=lp->gettoken_enum(1,"no\0yes\0");
    if (x <0)
    {
      return -2;
    }
    g_config_private_allowchat = x;
  }
  else if (!stricmp(t,"PrivateGroupLobbyMOTDFile"))
  {
    if (lp->getnumtokens() != 2) return -1;
    g_config_private_lobby_motdfile.Set(lp->gettoken_str(1));
  }
  else return -3;
  return 0;

};


static int ReadConfig(char *configfile, bool is_init=false)
{
  int linecnt=0;
  WDL_String linebuild;
  if (g_logfp) logText("[config] reloading configuration file\n");
  FILE *fp=strcmp(configfile,"-")?fopen(configfile,"rt"):stdin; 
  if (!fp)
  {
    printf("[config] error opening configfile '%s'\n",configfile);
    if (g_logfp) logText("[config] error opening config file (console request)\n");
    return -1;
  }

  // clear user list, etc
  g_config_port=2049;
  g_config_allow_anonchat=1;
  g_config_allowanonymous=0;
  g_config_allowanonymous_multi=0;
  g_config_anonymous_mask_ip=0;
  g_config_maxch_anon=2;
  g_config_maxch_user=32;
  g_default_bpi=8;
  g_default_bpm=120;

  g_config_log_sessionlen=10; // ten minute default, tho the user will need to specify the path anyway

  g_config_max_users=0; // unlimited users
  g_config_motdfile.Set("");

  g_acllist.Resize(0);
  g_config_license.Set("");
  int x;
  for(x=0;x<g_userlist.GetSize(); x++)
  {
    delete g_userlist.Get(x);
  }
  g_userlist.Empty();

  for (;;)
  {
    char buf[8192];
    linecnt++;
    if (!fgets(buf,sizeof(buf),fp)) break;
    WDL_remove_trailing_crlf(buf);

    LineParser lp;

    if (buf[0] && buf[strlen(buf)-1]=='\\')
    {
      buf[strlen(buf)-1]=0;
      linebuild.Append(buf);
      continue;
    }
    linebuild.Append(buf);

    int res=lp.parse(linebuild.Get());

    linebuild.Set("");

    if (res)
    {
      if (res==-2) 
      {
        if (g_logfp) logText("[config] warning: unterminated string parsing line %d of %s\n",linecnt,configfile);
        printf("[config] warning: unterminated string parsing line %d of %s\n",linecnt,configfile);
      }
      else 
      {
        if (g_logfp) logText("[config] warning: error parsing line %d of %s\n",linecnt,configfile);
        printf("[config] warning: error parsing line %d of %s\n",linecnt,configfile);
      }
    }
    else
    {
      if (lp.getnumtokens()>0)
      {
        int err=ConfigOnToken(&lp,is_init);
        if (err)
        {
          if (err == -1)
          {
            if (g_logfp) logText("[config] warning: wrong number of tokens on line %d of %s\n",linecnt,configfile);
            printf("[config] warning: wrong number of tokens on line %d of %s\n",linecnt,configfile);
          }
          if (err == -2)
          {
            if (g_logfp) logText("[config] warning: invalid parameter on line %d of %s\n",linecnt,configfile);
            printf("[config] warning: invalid parameter on line %d of %s\n",linecnt,configfile);
          }
          if (err == -3)
          {
            if (g_logfp) logText("[config] warning: invalid config command \"%s\" on line %d of %s\n",lp.gettoken_str(0),linecnt,configfile);
            printf("[config] warning: invalid config command \"%s\" on line %d of %s\n",lp.gettoken_str(0),linecnt,configfile);
          }
        }
      }
    }
  }
  copyConfigToGroup(m_group);

  if (g_logfp) logText("[config] reload complete\n");

  if (fp != stdin) fclose(fp);
  return 0;
}

int g_reloadconfig;
int g_done;


void sighandler(int sig)
{
  if (sig == SIGINT)
  {
    g_done=1;
  }
#ifndef _WIN32
  if (sig == SIGHUP)
  {
    g_reloadconfig=1;
  }
#endif
}

void enforceACL(User_Group *group)
{
  int x;
  int killcnt=0;
  for (x = 0; x < group->m_users.GetSize(); x ++)
  {
    User_Connection *c=group->m_users.Get(x);
    if (aclGet(c->m_netcon.GetConnection()->get_remote()) == ACL_FLAG_DENY)
    {
      c->m_netcon.Kill();
      killcnt++;
    }
  }
  if (killcnt) logText("killed %d users by enforcing ACL\n",killcnt);
}


void usage()
{
    printf("Usage: NINJAMserver config.cfg [options]\n"
           "Options (override config file):\n"
#ifndef _WIN32
           "  -pidfile <filename.pid>\n"
#endif
           "  -logfile <filename.log>\n"
           "  -archive <path_to_archive>\n"
           "  -port <port>\n"
#ifndef _WIN32
           "  -setuid <uid>\n"
#endif
      );
    exit(1);
}

void logText(const char *s, ...)
{
    if (g_logfp) 
    {      
      time_t tv;
      time(&tv);
      struct tm *t=localtime(&tv);
      fprintf(g_logfp,"[%04d/%02d/%02d %02d:%02d:%02d] ",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
    }


    va_list ap;
    va_start(ap,s);

    vfprintf(g_logfp?g_logfp:stdout,s,ap);    

    if (g_logfp) fflush(g_logfp);

    va_end(ap);
}

static void appendGroupUsers(User_Group *group, WDL_FastString &str, int linelen)
{
  for (int x=0;x<group->m_users.GetSize();x++)
  {
    User_Connection *user = group->m_users.Get(x);
    if (WDL_NORMALLY(user) && user->m_auth_state>0)
    {
      const char *n = user->m_username.Get();
      int nlen = (int)strlen(n);
      if (linelen && linelen + nlen + 1>100)
      {
        str.Append("\n    ");
        linelen = 4;
      }
      else
      {
        if (!linelen) str.Append("    ");
        else str.Append(" ");
        linelen += nlen+1;
      }
      str.Append(n);
    }
  }
}

const char *get_privatemode_stats(int privs, const char *req)
{
  if (!g_config_private_maxsz) return "";

  static WDL_FastString str;
  static time_t last_stat_time;
  time_t now = time(NULL);
  if (now > last_stat_time + 4)
  {
    int lobby_cnt = 0,x;
    for (x=0;x<m_group->m_users.GetSize();x++)
    {
      User_Connection *user = m_group->m_users.Get(x);
      if (WDL_NORMALLY(user) && user->m_auth_state>0)
        lobby_cnt++;
    }
    const int maxuserchk = 9;
    int total_cnt = 0;
    int sizes[maxuserchk+1]={0,};
    int public_cnt = 0;
    for (x=0;x<g_private_groups.GetSize();x++)
    {
      const char *nm=NULL;
      User_Group *g = g_private_groups.Enumerate(x,&nm);
      if (WDL_NORMALLY(g))
      {
        if (nm && g_config_private_publicprefix.GetLength() && !strnicmp(nm,g_config_private_publicprefix.Get(),g_config_private_publicprefix.GetLength()))
          public_cnt++;
        total_cnt += g->m_users.GetSize();
        int slot = g->m_users.GetSize();
        if (slot > maxuserchk) slot=maxuserchk;
        sizes[slot]++;
      }
    }
    str.SetFormatted(256,"%d/%d rooms occupied, %d user%s total in rooms, %d user%s in lobby\n",g_private_groups.GetSize(),g_config_private_maxsz,
        total_cnt,total_cnt==1?"":"s", 
        lobby_cnt,lobby_cnt==1?"":"s");
    for (x=maxuserchk;x>=0;x--)
    {
      if (sizes[x]) 
        str.AppendFormatted(256,"%d room%s %d%s user%s\n",
                                sizes[x], sizes[x]==1?" has":"s have", 
                                x, x==maxuserchk?"+":"", x==1?"":"s");
    }
    if (public_cnt > 0)
    {
      str.AppendFormatted(256,"%d room%s public:\n",public_cnt,public_cnt == 1 ? " is" : "s are");
      for (x=0;x<g_private_groups.GetSize();x++)
      {
        const char *nm=NULL;
        User_Group *g = g_private_groups.Enumerate(x,&nm);
        if (WDL_NORMALLY(g))
        {
          if (nm && g_config_private_publicprefix.GetLength() && 
              !strnicmp(nm,g_config_private_publicprefix.Get(),g_config_private_publicprefix.GetLength()))
          {
            str.AppendFormatted(256,"  %s - %d/%d users, %d BPI %d BPM\n",nm,g->m_users.GetSize(),g->m_max_users,g->m_last_bpi,g->m_last_bpm);
          }
        }
      }
    }
    if (lobby_cnt)
    {
      str.Append("Lobby users: ");
      appendGroupUsers(m_group,str,13);
      str.Append("\n");
    }
  }
  if ((privs & PRIV_SHOW_PRIVATE) && req && strstr(req,"full"))
  {
    static WDL_FastString str2;
    str2 = str;
    str2.Append("\nFull room list:\n");
    for (int x=0;x<g_private_groups.GetSize();x++)
    {
      const char *nm=NULL;
      User_Group *g = g_private_groups.Enumerate(x,&nm);
      if (WDL_NORMALLY(g) && nm)
      {
        str2.AppendFormatted(256,"  %s - %d/%d users, %d BPI %d BPM:\n",nm,g->m_users.GetSize(),g->m_max_users,g->m_last_bpi,g->m_last_bpm);
        appendGroupUsers(g,str2,0);
        str2.Append("\n");
      }
    }
    return str2.Get();
  }
  return str.Get();
}


int main(int argc, char **argv)
{

  if (argc < 2)
  {
    usage();
  }

  m_group=new User_Group;

  printf("%s",startupmessage);
  if (ReadConfig(argv[1],true))
  {
    printf("Error loading config file!\n");
    exit(1);
  }
  int p;
  for (p = 2; p < argc; p ++)
  {
      if (!strcmp(argv[p],"-pidfile"))
      {
        if (++p >= argc) usage();
        g_pidfilename.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-logfile"))
      {
        if (++p >= argc) usage();
        g_logfilename.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-archive"))
      {
        if (++p >= argc) usage();
        g_config_logpath.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-setuid"))
      {
        if (++p >= argc) usage();
        g_set_uid=atoi(argv[p]);
      }
      else if (!strcmp(argv[p],"-port"))
      {
        if (++p >= argc) usage();
        g_config_port=atoi(argv[p]);
      }
      else usage();

  }


#ifdef _WIN32
  DWORD v=GetTickCount();
  WDL_RNG_addentropy(&v,sizeof(v));
  v=(DWORD)time(NULL);
  WDL_RNG_addentropy(&v,sizeof(v));
#else

  if (g_set_uid != -1) 
  {
    if (setuid(g_set_uid))
      printf("warning: setuid(%d) failed\n",g_set_uid);
  }

  time_t v=time(NULL);
  WDL_RNG_addentropy(&v,sizeof(v));
  int pid=getpid();
  WDL_RNG_addentropy(&pid,sizeof(pid));

  if (g_pidfilename.Get()[0])
  {
    FILE *fp=fopen(g_pidfilename.Get(),"w");
    if (fp)
    {
      fprintf(fp,"%d\n",pid);
      fclose(fp);
    }
    else printf("Error opening PID file '%s'\n",g_pidfilename.Get());
  }



  signal(SIGPIPE,sighandler);
  signal(SIGHUP,sighandler);
#endif
  signal(SIGINT,sighandler);


  if (g_logfilename.Get()[0])
  {
    g_logfp=fopen(g_logfilename.Get(),"at");
    if (!g_logfp)
      printf("Error opening log file '%s'\n",g_logfilename.Get());
    else
      logText("Opened log. NINJAM Server %s built on %s at %s\n",VERSION,__DATE__,__TIME__);

  }

  logText("Server starting up...\n");

  JNL::open_socketlib();

  {
    logText("Port: %d\n",g_config_port);    
    m_listener = new JNL_Listen(g_config_port);
    if (m_listener->is_error()) 
    {
      logText("Error listening on port %d!\n",g_config_port);
    }

    m_group->CreateUserLookup=myCreateUserLookup;

    logText("Using defaults %d BPM %d BPI\n",g_default_bpm,g_default_bpi);
    m_group->SetConfig(g_default_bpi,g_default_bpm);

    m_group->SetLicenseText(g_config_license.Get());

#ifdef _WIN32
    int needprompt=2;
    int esc_state=0;
#endif
    while (!g_done)
    {
      JNL_IConnection *con=m_listener->get_connect(2*65536,65536);
      if (con) 
      {
        char str[512];
        int flag=aclGet(con->get_remote());
        JNL::addr_to_ipstr(con->get_remote(),str,sizeof(str));
        logText("Incoming connection from %s!\n",str);

        if (flag == ACL_FLAG_DENY)
        {
          logText("Denying connection (via ACL)\n");
          delete con;
        }
        else
        {
          m_group->AddConnection(con,flag == ACL_FLAG_RESERVE);
        }
      }

      int can_idle = m_group->Run();

      // only check one lobby-user per cycle to see if we need to move them to a room.
      // if this is too slow we could do a few per cycle but no need to do all of them
      {
        static int rrchk;
        if (rrchk >= m_group->m_users.GetSize()) rrchk = 0;
        User_Connection *c = m_group->m_users.Get(rrchk);
        if (c && c->m_wants_group_migration.GetLength())
        {
          User_Group *ng = g_private_groups.Get(c->m_wants_group_migration.Get());
          const char *msg = "[lobby] could not join room, unknown error.";
          if (!ng)
          {
            if (g_private_groups.GetSize() < g_config_private_maxsz)
            {
              logText("PrivateMode - creating room '%s' (%d/%d)\n",c->m_wants_group_migration.Get(),g_private_groups.GetSize()+1,g_config_private_maxsz);

              ng = new User_Group;
              copyConfigToGroup(ng);
              ng->m_topictext.SetFormatted(256,"Private Room - %s",c->m_wants_group_migration.Get());
              ng->SetConfig(g_default_bpi,g_default_bpm);
              g_private_groups.Insert(c->m_wants_group_migration.Get(),ng);
            }
            else
            {
              logText("PrivateMode - cannot create '%s' (%d/%d)\n",c->m_wants_group_migration.Get(),g_private_groups.GetSize(),g_config_private_maxsz);
              msg = "[lobby] could not join room, server is at room capacity!";
            }
          }
          else
          {
            if (ng->m_users.GetSize() >= ng->m_max_users)
            {
              logText("PrivateMode - cannot join '%s' (%d/%d)\n",c->m_wants_group_migration.Get(),ng->m_users.GetSize(), ng->m_max_users);
              msg = "[lobby] could not join room, room is at capacity!";
              ng = NULL;
            }
          }
          c->m_wants_group_migration.Set("");

          if (ng)
            msg = "[lobby] joining room!";

          mpb_chat_message newmsg;
          newmsg.parms[0]="MSG";
          newmsg.parms[1]="";
          newmsg.parms[2]=(char *)msg;
          c->Send(newmsg.build());

          if (ng)
          {
            m_group->m_users.Delete(rrchk--);
            ng->m_users.Add(c);

            // notify the lobby we're leaving
            mpb_chat_message newmsg;
            newmsg.parms[0]="PART";
            newmsg.parms[1]=c->m_username.Get();
            m_group->Broadcast(newmsg.build(),NULL);

            // notify existing users we're joining via chat
            {
              mpb_chat_message newmsg;
              newmsg.parms[0]="JOIN";
              newmsg.parms[1]=c->m_username.Get();
              ng->Broadcast(newmsg.build(),c);
            }

            // broadcast our channels to any existing users
            {
              mpb_server_userinfo_change_notify bh;

              int acnt=0;
              for (int channel = 0; channel < c->m_max_channels && channel < MAX_USER_CHANNELS; channel ++)
              {
                if (c->m_channels[channel].active)
                {
                  bh.build_add_rec(1,channel,c->m_channels[channel].volume,c->m_channels[channel].panning,c->m_channels[channel].flags,
                                    c->m_username.Get(),c->m_channels[channel].name.Get());
                  acnt++;
                }
              }
              if (!acnt && !ng->m_allow_hidden_users && c->m_max_channels && !(c->m_auth_privs & PRIV_HIDDEN)) // give users at least one channel
              {
                bh.build_add_rec(1,0,0,0,0,c->m_username.Get(),"");
              }
              ng->Broadcast(bh.build(),c);
            }

            c->SendAuthReply(ng);
            c->SendUserList(ng);
            c->SendMOTDFile(ng);
            c->SendConnectInfo(ng);
          }


        }
        rrchk++;
      }

      for (int x = 0; x < g_private_groups.GetSize(); x ++)
      {
        const char *nm=NULL;
        User_Group *g = g_private_groups.Enumerate(x,&nm);
        if (WDL_NORMALLY(g))
        {
          if (!g->Run())
            can_idle = 0;
          else if (!g->m_users.GetSize())
          {
            logText("PrivateMode - disposing empty group '%s' %d/%d\n",nm,x,g_private_groups.GetSize());
            g_private_groups.DeleteByIndex(x--);
          }
        }
      }

      if (can_idle)
      {
#ifdef _WIN32
        if (needprompt)
        {
          if (needprompt>1) printf("\nKeys:\n"
               "  [S]how user table\n"
               "  [R]eload config file\n"
               "  [K]ill user\n"
               "  [Q]uit server\n");
          printf(": ");
          needprompt=0;
        }
        if (kbhit())
        {
          int c=toupper(getch());
          printf("%c\n",isalpha(c)?c:'?');
          if (esc_state)
          {
            if (c == 'Y') break;
            printf("Exit aborted\n");
            needprompt=2;
            esc_state=0;
          }
          else if (c == 'Q')
          {
            if (!esc_state)
            {
              esc_state++;
              printf("Q pressed -- hit Y to exit, any other key to continue\n");
              needprompt=1;
            }
          }
          else if (c == 'K')
          {
            printf("(be quick, server is paused while you type!!!)\nKill username: ");
            char buf[512];
            fgets(buf,sizeof(buf),stdin);
            WDL_remove_trailing_crlf(buf);
            if (buf[0])
            {
              int x;
              int killcnt=0;
              for (x = 0; x < m_group->m_users.GetSize(); x ++)
              {
                User_Connection *c=m_group->m_users.Get(x);
                if (!strcmp(c->m_username.Get(),buf))
                {
                  char str[512];
                  JNL::addr_to_ipstr(c->m_netcon.GetConnection()->get_remote(),str,sizeof(str));
                  printf("Killing user %s on %s\n",c->m_username.Get(),str);
                  c->m_netcon.Kill();
                  killcnt++;
                }
              }
              if (!killcnt)
              {
                printf("User %s not found!\n",buf);
              }
            }
            else printf("Kill aborted with no input\n");
            needprompt=1;
          }
          else if (c == 'S')
          {
            needprompt=1;
            int x;
            for (x = 0; x < m_group->m_users.GetSize(); x ++)
            {
              User_Connection *c=m_group->m_users.Get(x);
              char str[512];
              JNL::addr_to_ipstr(c->m_netcon.GetConnection()->get_remote(),str,sizeof(str));
              printf("%s:%s\n",c->m_auth_state>0?c->m_username.Get():"<unauthorized>",str);
            }
          }
          else if (c == 'R')
          {
            if (!strcmp(argv[1],"-") || ReadConfig(argv[1]))
            {
              if (g_logfp) logText("Error opening config file\n");
              printf("Error opening config file!\n");
            }
            else
            {
//              printf("Listening on port %d...",g_config_port);    

              onConfigChange(argc,argv);
            }
            needprompt=1;
          }
          else needprompt=2;
         

        }
        Sleep(5);
#else
        struct timespec ts={0,5*1000*1000};
        nanosleep(&ts,NULL);
#endif

        if (g_reloadconfig && strcmp(argv[1],"-"))
        {
          g_reloadconfig=0;

          if (!ReadConfig(argv[1]))
            onConfigChange(argc,argv);
        }
      }

      if (can_idle && !g_config_private_maxsz)
      {
        time_t now;
        time(&now);
        if (now >= next_session_update_time)
        {
          m_group->SetLogDir(NULL);

          int len=30; // check every 30 seconds if we aren't logging       

          if (g_config_logpath.Get()[0])
          {
            int x;
            for (x = 0; x < m_group->m_users.GetSize() && m_group->m_users.Get(x)->m_auth_state < 1; x ++);
           
            if (x < m_group->m_users.GetSize())
            {
              WDL_String tmp;
    
              int cnt=0;
              while (cnt < 16)
              {
                char buf[512];
                struct tm *t=localtime(&now);
                sprintf(buf,"/%04d%02d%02d_%02d%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min);
                if (cnt)
                  sprintf(buf+strlen(buf),"_%d",cnt);
                strcat(buf,".ninjam");

                tmp.Set(g_config_logpath.Get());
                tmp.Append(buf);

                #ifdef _WIN32
                if (CreateDirectory(tmp.Get(),NULL)) break;
                #else
                if (!mkdir(tmp.Get(),0755)) break;
                #endif

                cnt++;
              }
    
              if (cnt < 16 )
              {
                logText("Archiving session '%s'\n",tmp.Get());
                m_group->SetLogDir(tmp.Get());
              }
              else
              {
                logText("Error creating a session archive directory! Gave up after '%s' failed!\n",tmp.Get());
              }
              // if we succeded, don't check until configured time
              len=g_config_log_sessionlen*60;
              if (len < 60) len=30;
            }

          }
          next_session_update_time=now+len;

        }
      }
    }
  }

  logText("Shutting down server\n");

  g_private_groups.DeleteAll();
  delete m_group;
  delete m_listener;

  if (g_logfp)
  {
    fclose(g_logfp);
    g_logfp=0;
  }

  JNL::close_socketlib();
  return 0;
}


void onConfigChange(int argc, char **argv)
{
  logText("reloading config...\n");

  //m_group->SetConfig(g_config_bpi,g_config_bpm);
  enforceACL(m_group);
  m_group->SetLicenseText(g_config_license.Get());

  for (int x = 0; x < g_private_groups.GetSize(); x ++)
  {
    User_Group *g = g_private_groups.Enumerate(x,NULL);
    if (WDL_NORMALLY(g)) enforceACL(g);
  }

  int p;
  for (p = 2; p < argc; p ++)
  {
      if (!strcmp(argv[p],"-pidfile"))
      {
        if (++p >= argc) break;
      //  g_pidfilename.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-logfile"))
      {
        if (++p >= argc) break;
//        g_logfilename.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-archive"))
      {
        if (++p >= argc) break;
        g_config_logpath.Set(argv[p]);
      }
      else if (!strcmp(argv[p],"-setuid"))
      {
        if (++p >= argc) break;
  //      g_set_uid=atoi(argv[p]);
      }
      else if (!strcmp(argv[p],"-port"))
      {
        if (++p >= argc) break;
        g_config_port=atoi(argv[p]);
      }
  }

  delete m_listener;
  m_listener = new JNL_Listen(g_config_port);

}
