#ifndef _npp_exec_msgs_h_
#define _npp_exec_msgs_h_
//---------------------------------------------------------------------------
#include <windows.h>

/*    

Reference to "Notepad_plus_msgs.h":
 
  #define NPPM_MSGTOPLUGIN (NPPMSG + 47)
  //BOOL NPPM_MSGTOPLUGIN(TCHAR *destModuleName, CommunicationInfo *info)
  // return value is TRUE when the message arrive to the destination plugins.
  // if destModule or info is NULL, then return value is FALSE
  struct CommunicationInfo {
    long internalMsg;
    const TCHAR * srcModuleName;
    void * info; // defined by plugin
  };
 
The following messages can be used as internalMsg parameter.

 */


#define  NPEM_GETVERDWORD       0x0201  // message
  /*
  Returns plugin's version as DWORD e.g. 0x02B4:
    0x02B4 - means "0.2 beta 4" (B - Beta);
    0x02C1 - means "0.2 RC1" (C - release Candidate);
    0x02F0 - means "0.2 final" (F - Final);
    0x02F1 - means "0.2.1" (patched Final version);
    0x03A1 - means "0.3 alpha 1" (A - Alpha).

  Example:

  const TCHAR* cszMyPlugin = _T("my_plugin");
  DWORD dwVersion = 0;
  CommunicationInfo ci = { NPEM_GETVERDWORD, 
                           cszMyPlugin, 
                           (void *) &dwVersion };
  ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
      (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

  Possible results:
  1) dwVersion remains 0 - the plugin is not active or does not support
     external messages;
  2) dwVersion >= 0x02F5 - the plugin is active and other messages can
     be used.

  if ( dwVersion >= 0x02F5 )
  {
     // other messages are accessible
  }
  else
  {
    // plugin is not active or does not support external messages
  }
  */


#define  NPEM_GETVERSTR         0x0202  // message
  #define  NPE_MAXVERSTR            32
  /*
  Returns plugin's version as string e.g. "0.2 beta 4".

  Example:

  const TCHAR* cszMyPlugin = _T("my_plugin");
  TCHAR szVersion[NPE_MAXVERSTR] = { 0 };
  CommunicationInfo ci = { NPEM_GETVERSTR, 
                           cszMyPlugin, 
                           (void *) szVersion };
  ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
      (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );
  */


#define  NPEM_GETSTATE          0x0301  // message
  #define  NPE_STATEREADY         0x01
  #define  NPE_STATEBUSY          0x10
  #define  NPE_STATECHILDPROC     (NPE_STATEBUSY | 0x20)
  /*
  Returns plugin's state as DWORD:
    NPE_STATEREADY     - the plugin is "ready";
    NPE_STATEBUSY      - the plugin is "busy" (script is executed);
    NPE_STATECHILDPROC - the plugin is "busy" (child process is run).

  Example:

  const TCHAR* cszMyPlugin = _T("my_plugin");
  DWORD dwState = 0;
  CommunicationInfo ci = { NPEM_GETSTATE, 
                           cszMyPlugin, 
                           (void *) &dwState };
  ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
      (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

  if ( dwState == NPE_STATEREADY )
  {
    // the plugin is "ready"
  }
  else if ( dwState & NPE_STATEBUSY )
  {
    // the plugin is "busy"
  }
  else
  {
    // unknown state: maybe the plugin does not support external messages
  }
  */


#define  NPEM_PRINT             0x0401  // message
  /*
  Prints (shows) given text in NppExec's Console window. 
  You can separate text lines using _T('\n') or _T("\r\n").
  This text can be highlighted if NppExec's Console Highlight Filters are used.

  If plugin's state is "busy", this message is ignored.

  Example:

  const TCHAR* cszMyPlugin = _T("my_plugin");
  DWORD dwState = 0;
  CommunicationInfo ci = { NPEM_GETSTATE, 
                           cszMyPlugin, 
                           (void *) &dwState };
  ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
      (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

  if ( dwState == NPE_STATEREADY )
  {
    // the plugin is "ready"
    const TCHAR* szText = _T("Hello from my plugin!\n(test message)")
    CommunicationInfo ci = { NPEM_PRINT,
                             cszMyPlugin, 
                             (void *) szText };
    ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
        (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );
  }
  */


#define  NPEM_EXECUTE           0x0101  // message
  #define  NPE_EXECUTE_OK       NPE_STATEREADY
  #define  NPE_EXECUTE_FAILED   NPE_STATEBUSY
  typedef struct sNpeExecuteParam {
      const TCHAR* szScriptBody; // text of the script (i.e. set of commands)
      DWORD        dwResult;     // [out] NPE_EXECUTE_OK - OK; otherwise failed
  } NpeExecuteParam;
  /*
  Executes given commands. 
  
  If plugin's state is "busy", then nothing happens (the message is ignored).
  If szScriptBody is NULL or empty, then nothing happens also.
  In this case [out] dwResult will contain NPE_EXECUTE_FAILED.

  !!! NOTE !!!
  Set [in] dwResult to 1 if you want NppExec to send a message (notification) 
  to your plugin after the script will be executed.
  Otherwise set [in] dwResult to 0.

  !!! NOTE !!!
  The szScriptBody parameter can contain several text lines (commands).
  You can separate text lines using _T('\n') or _T("\r\n").

  Example:

  const TCHAR* cszMyPlugin = _T("my_plugin");
  DWORD dwState = 0;
  CommunicationInfo ci = { NPEM_GETSTATE, 
                           cszMyPlugin, 
                           (void *) &dwState };
  ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
      (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

  if ( dwState == NPE_STATEREADY )
  {
    // the plugin is "ready"
    NpeExecuteParam nep;
    nep.szScriptBody = _T("cls \n npp_open *.txt \n npp_switch $(#2)");
    nep.dwResult = 0; // don't send a message after the script will be executed
    CommunicationInfo ci = { NPEM_EXECUTE, 
                             cszMyPlugin, 
                             (void *) &nep };
    ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
        (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

    if ( nep.dwResult == NPE_EXECUTE_OK )
    {
      // OK, the script is executed
    }
    else
    {
      // failed, the plugin is "busy"
    }
  }
  else
  {
    // the plugin is "busy", NPEM_EXECUTE is not possible
  }
  */


#define  NPEM_NPPEXEC           0x0102  // message
  #define  NPE_NPPEXEC_OK       NPE_STATEREADY
  #define  NPE_NPPEXEC_FAILED   NPE_STATEBUSY
  typedef struct sNpeNppExecParam {
      const TCHAR* szScriptName;      // name of existing script/file
      const TCHAR* szScriptArguments; // arguments, can be NULL (i.e. none)
      DWORD        dwResult;          // [out] NPE_NPPEXEC_OK - OK; otherwise failed
  } NpeNppExecParam;
  /*
  1.a. Substitutes given arguments instead of $(ARGV[n]) inside the script.
  1.b. Executes specified script (internal NppExec's script, if it exists).
  2.a. Substitutes given arguments instead of $(ARGV[n]) inside the script.
  2.b. Executes commands from specified file.
  
  If plugin's state is "busy", then nothing happens (the message is ignored).
  If szScriptName is NULL or empty, then nothing happens also.
  In this case [out] dwResult will contain NPE_NPPEXEC_FAILED.

  !!! NOTE !!!
  Set [in] dwResult to 1 if you want NppExec to send a message (notification) 
  to your plugin after the script will be executed.
  Otherwise set [in] dwResult to 0.

  Example:

  const TCHAR* cszMyPlugin = _T("my_plugin");
  DWORD dwState = 0;
  CommunicationInfo ci = { NPEM_GETSTATE, 
                           cszMyPlugin, 
                           (void *) &dwState };
  ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
      (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

  if ( dwState == NPE_STATEREADY )
  {
    // the plugin is "ready"
    NpeNppExecParam npep;
    npep.szScriptName = _T("C:\\Program Files\\My NppExec Scripts\\test script.txt");
    npep.szScriptArguments = _T("\"arg 1\" \"arg 2\""); // [1] = "arg 1", [2] = "arg 2"
    npep.dwResult = 1; // send a message after the script will be executed
    CommunicationInfo ci = { NPEM_EXECUTE, 
                             cszMyPlugin, 
                             (void *) &npep };
    ::SendMessage( hNppWnd, NPPM_MSGTOPLUGIN,
        (WPARAM) _T("NppExec.dll"), (LPARAM) &ci );

    if ( npep.dwResult == NPE_NPPEXEC_OK )
    {
      // OK, the script is executed
      // you'll get NPEN_RESULT when the script will be executed completely
    }
    else
    {
      // failed, the plugin is "busy"
    }
  }
  else
  {
    // the plugin is "busy", NPEM_EXECUTE is not possible
  }
  */

#define  NPEN_RESULT           0x0109  // notification
  #define  NPE_RESULT_OK       NPE_STATEREADY
  #define  NPE_RESULT_FAILED   NPE_STATEBUSY
  /*
  This notification is sent to your plugin as a result of complete execution 
  of previous message NPEM_EXECUTE or NPEM_NPPEXEC, if the parameter dwResult 
  of this message was set to 1.

  Parameters of the CommunicationInfo struct:
  {
    internalMsg = NPEN_RESULT,
    srcModuleName = _T("NppExec.dll"), 
    info = (DWORD *) pdwResult // contains NPE_RESULT_OK or NPE_RESULT_FAILED
  };

  !!! NOTE !!! 
  NppExec dll name may be different, so srcModuleName may contain different
  string (e.g. "NppExec1.dll" or "MyNppExec.dll" according to the dll file name).

  Example:

  extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
  {
    ...
    if ( Message == NPPM_MSGTOPLUGIN )
    {
      CommunicationInfo* pci = (CommunicationInfo *) lParam;
      if ( pci )
      {
        if ( pci->internalMsg == NPEN_RESULT )
        { 
          // NPEN_RESULT notification...
          if ( lstrcmpi(pci->srcModuleName, szNppExecModuleName) == 0 )
          {
            // ...from NppExec plugin
            DWORD dwResult = *((DWORD *) pci->info);
            if ( dwResult == NPE_RESULT_OK )
            {
              // OK, the script has been executed
            }
            else
            {
              // failed, maybe internal error in NppExec
            }
          }
        }
      }
    }
    ...
  }
  */

#define  NPEM_INTERNAL         0xFF01  // internal messages, don't use it!
#define  NPEM_SUSPENDEDACTION  (NPEM_INTERNAL + 1)

//---------------------------------------------------------------------------
#endif
