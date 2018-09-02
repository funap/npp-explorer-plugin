//this file is part of notepad++
//Copyright (C)2003 Don HO ( donho@altern.org )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "winVersion.h"


typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);

winVer getWindowsVersion()
{
	OSVERSIONINFOEXNEXT	osvi;
	SYSTEM_INFO			si;
	PGNSI				pGNSI;
	BOOL				bOsVersionInfoEx;

	ZeroMemory(&si, sizeof(SYSTEM_INFO));
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXNEXT));

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXNEXT);

	if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
	{
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) ) 
			return WV_UNKNOWN;
	}

	pGNSI = (PGNSI) GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetNativeSystemInfo");
	if(pGNSI != NULL)
		pGNSI(&si);
	else
		GetSystemInfo(&si);

   switch (osvi.dwPlatformId)
   {
		case VER_PLATFORM_WIN32_NT:
		{
			if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 )
			{
				return WV_VISTA;
			}

			if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
			{
				if (osvi.wProductType == VER_NT_WORKSTATION &&
					   si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
				{
					return WV_XPX64;
				}
				return WV_S2003;
			}

			if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
				return WV_XP;

			if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
				return WV_W2K;

			if ( osvi.dwMajorVersion <= 4 )
				return WV_NT;
		}
		break;

		// Test for the Windows Me/98/95.
		case VER_PLATFORM_WIN32_WINDOWS:
		{
			if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
			{
				return WV_95;
			} 

			if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
			{
				return WV_98;
			} 

			if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
			{
				return WV_ME;
			}
		}
		break;

      case VER_PLATFORM_WIN32s:
		return WV_WIN32S;
      
	  default :
		return WV_UNKNOWN;
   }
   return WV_UNKNOWN; 
}