# Microsoft Developer Studio Project File - Name="ntpdc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ntpdc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ntpdc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ntpdc.mak" CFG="ntpdc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ntpdc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "ntpdc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/ntp/dev/ports/winnt/ntpdc", SWBAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ntpdc - Win32 Release"

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
# ADD CPP /nologo /MT /W4 /GX /O2 /I "." /I "..\include" /I "..\..\..\include" /D "NDEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /D "SYS_WINNT" /D "HAVE_CONFIG_H" /D _WIN32_WINNT=0x400 /YX"windows.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib ws2_32.lib advapi32.lib /nologo /subsystem:console /machine:I386 /out:"../bin/Release/ntpdc.exe"

!ELSEIF  "$(CFG)" == "ntpdc - Win32 Debug"

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
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "." /I "..\include" /I "..\..\..\include" /D "_DEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /D "SYS_WINNT" /D "HAVE_CONFIG_H" /D _WIN32_WINNT=0x400 /FR /YX"windows.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib ws2_32.lib advapi32.lib /nologo /subsystem:console /debug /machine:I386 /out:"../bin/Debug/ntpdc.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "ntpdc - Win32 Release"
# Name "ntpdc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\ntpdc\ntpdc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\ntpdc\ntpdc_ops.c
# End Source File
# Begin Source File

SOURCE=..\libntp\util_clockstuff.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\ntpdc\ntpdc.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\..\..\configure

!IF  "$(CFG)" == "ntpdc - Win32 Release"

# Begin Custom Build
ProjDir=.
InputPath=..\..\..\configure

"$(ProjDir)\version.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo Using NT Shell Script to generate version.c 
	..\scripts\mkver.bat -P ntpdc 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ntpdc - Win32 Debug"

# Begin Custom Build
ProjDir=.
InputPath=..\..\..\configure

"$(ProjDir)\version.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo Using NT Shell Script to generate version.c 
	..\scripts\mkver.bat -P ntpdc 
	
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
