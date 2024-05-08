# Microsoft Developer Studio Project File - Name="ninjamcast" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ninjamcast - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ninjamcast.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ninjamcast.mak" CFG="ninjamcast - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ninjamcast - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "ninjamcast - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ninjamcast - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../sdks/oggvorbis-win32sdk-1.0.1/include" /I "." /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "NJCLIENT_NO_XMIT_SUPPORT" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib setupapi.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "ninjamcast - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../sdks/oggvorbis-win32sdk-1.0.1/include" /I "." /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "NJCLIENT_NO_XMIT_SUPPORT" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib setupapi.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "ninjamcast - Win32 Release"
# Name "ninjamcast - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Group "wdl src"

# PROP Default_Filter ""
# Begin Group "jnetlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\WDL\jnetlib\asyncdns.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\connection.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\httpget.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\listen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\util.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\WDL\lameencdec.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\rng.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\sha.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\mpb.cpp
# End Source File
# Begin Source File

SOURCE=..\netmsg.cpp
# End Source File
# Begin Source File

SOURCE=..\njclient.cpp
# End Source File
# Begin Source File

SOURCE=..\njclient.h
# End Source File
# Begin Source File

SOURCE="..\..\sdks\oggvorbis-win32sdk-1.0.1\lib\vorbisenc_static.lib"
# End Source File
# Begin Source File

SOURCE="..\..\sdks\oggvorbis-win32sdk-1.0.1\lib\ogg_static.lib"
# End Source File
# Begin Source File

SOURCE="..\..\sdks\oggvorbis-win32sdk-1.0.1\lib\vorbis_static.lib"
# End Source File
# End Group
# Begin Source File

SOURCE=.\ninjamcast.cpp
# End Source File
# Begin Source File

SOURCE=.\njcast.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
