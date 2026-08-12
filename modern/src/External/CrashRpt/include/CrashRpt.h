// SHIM: CrashRpt
//
// Replaces:  CrashRpt (crash reporting)
// Status:    NO-OP. No crash reports are generated.
// Later:     Crashpad (Apache-2.0) or Sentry Native (MIT) -- see DEPENDENCIES.md.
//
// Clean-room declarations derived from call sites in src/Eternity/Source/r3dDebug.cpp.

#pragma once
#include <windows.h>

#define CR_INST_ALL_POSSIBLE_HANDLERS   0x1FFF
#define CR_INST_APP_RESTART             0x8000

typedef struct tagCR_INSTALL_INFOA
{
    WORD   cb;
    LPCSTR pszAppName;
    LPCSTR pszAppVersion;
    LPCSTR pszEmailTo;
    LPCSTR pszEmailSubject;
    LPCSTR pszUrl;
    LPCSTR pszCrashSenderPath;
    LPVOID pfnCrashCallback;
    UINT   uPriorities[3];
    DWORD  dwFlags;
    LPCSTR pszPrivacyPolicyURL;
    LPCSTR pszDebugHelpDLL;
    UINT   uMiniDumpType;
    LPCSTR pszErrorReportSaveDir;
    LPCSTR pszRestartCmdLine;
} CR_INSTALL_INFOA, *PCR_INSTALL_INFOA;
typedef CR_INSTALL_INFOA CR_INSTALL_INFO;

inline int crInstallA(PCR_INSTALL_INFOA)                 { return 0; }
inline int crUninstall()                                 { return 0; }
inline int crInstallToCurrentThread2(DWORD)              { return 0; }
inline int crUninstallFromCurrentThread()                { return 0; }
inline int crAddFile2A(LPCSTR, LPCSTR, LPCSTR, DWORD)    { return 0; }
inline int crAddScreenshot2(DWORD, int)                  { return 0; }
inline int crGenerateErrorReport(LPVOID)                 { return 0; }
inline int crGetLastErrorMsgA(LPSTR, UINT)               { return 0; }

#define crInstall           crInstallA
#define crAddFile2          crAddFile2A
#define crGetLastErrorMsg   crGetLastErrorMsgA
