
#ifndef WINVERSION_H
#define WINVERSION_H

#include <windows.h>

#ifndef VER_NT_WORKSTATION
#define VER_NT_WORKSTATION 1
#endif

#ifndef PROCESSOR_ARCHITECTURE_AMD64
#define PROCESSOR_ARCHITECTURE_AMD64 9
#endif


typedef struct _OSVERSIONINFOEXNEXT {
  DWORD dwOSVersionInfoSize;
  DWORD dwMajorVersion;
  DWORD dwMinorVersion;
  DWORD dwBuildNumber;
  DWORD dwPlatformId;
  TCHAR szCSDVersion[128];
  WORD wServicePackMajor;
  WORD wServicePackMinor;
  WORD wSuiteMask;
  BYTE wProductType;
  BYTE wReserved;
} OSVERSIONINFOEXNEXT, 
 *POSVERSIONINFOEXNEXT, 
 *LPOSVERSIONINFOEXNEXT;

enum winVer{WV_UNKNOWN, WV_WIN32S, WV_95, WV_98, WV_ME, WV_NT, WV_W2K, WV_XP, WV_S2003, WV_XPX64, WV_VISTA};
winVer getWindowsVersion();

#endif	/* WINVERSION_H */
