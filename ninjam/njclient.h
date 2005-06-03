#ifndef _NJCLIENT_H_
#define _NJCLIENT_H_

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "../WDL/string.h"
#include "../WDL/ptrlist.h"
#include "../WDL/jnetlib/jnetlib.h"
#include "../WDL/sha.h"
#include "../WDL/rng.h"

#include "../netmsg.h"
#include "vorbisencdec.h"




#define MAX_USER_CHANNELS 32
class RemoteUser_Channel
{
  public:
    RemoteUser_Channel();
    ~RemoteUser_Channel();


    int active;
    float volume, pan;
    bool muted;

    unsigned char cur_guid[16];
    WDL_String name;


    // decode/mixer state, used by mixer
    FILE *decode_fp;
    VorbisDecoder *decode_codec;
    int dump_samples;
    unsigned char decode_last_guid[16];

};

class RemoteUser
{
public:
  RemoteUser() { }
  ~RemoteUser() { }

  WDL_String name;
  RemoteUser_Channel channels[MAX_USER_CHANNELS];
};


#define DOWNLOAD_TIMEOUT 8
class RemoteDownload
{
public:
  RemoteDownload();
  ~RemoteDownload();

  void Close();
  void Open();
  void Write(void *buf, int len);

  time_t last_time;
  unsigned char guid[16];

private:
  FILE *fp;
};




class NJClient
{
public:
  NJClient();
  ~NJClient();

  void Connect(char *host, char *user, char *pass);

  int Run();// returns nonzero if sleep is OK

  void AudioProc(float *buf, int len, int nch, int srate); // len is number of sample pairs or samples




  int   config_send;            // basic configuration
  int   config_recv;
  int   config_savelocalaudio;
  float config_monitor;
  float config_metronome;
  int   config_debug_level; 



#define NJC_STATUS_CANTCONNECT -1
#define NJC_STATUS_INVALIDAUTH -2
#define NJC_STATUS_DISCONNECTED -3
#define NJC_STATUS_PRECONNECT 1
#define NJC_STATUS_OK 0
  int GetStatus();

  int GetBPM() { return m_active_bpm; }
  int GetBPI() { return m_active_bpi; }


private:
  void updateBPMinfo(int bpm, int bpi);
  void process_samples(float *buf, int len, int nch, int srate);
  void on_new_interval(int nch, int srate);

  int m_status;

  WDL_String m_user, m_pass;

  int m_bpm,m_bpi;
  int m_beatinfo_updated;
  int m_audio_enable;

  int m_active_bpm, m_active_bpi;
  int m_interval_length;
  int m_interval_pos, m_metronome_pos, m_metronome_state, m_metronome_tmp,m_metronome_interval;


// per-channel encoding stuff
  VorbisEncoder *m_vorbisenc;
  RemoteDownload *m_curwritefile;
  Net_Message *m_vorbisenc_header_needsend;

  CRITICAL_SECTION m_net_cs;
  Net_Connection *m_netcon;
  WDL_PtrList<RemoteUser> m_remoteusers;
  WDL_PtrList<RemoteDownload> m_downloads;
};




extern unsigned char zero_guid[16];;
char *guidtostr_tmp(unsigned char *guid);
void guidtostr(unsigned char *guid, char *str);
void makeFilenameFromGuid(WDL_String *s, unsigned char *guid);




#endif//_NJCLIENT_H_