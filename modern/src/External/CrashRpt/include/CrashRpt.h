// SHIM: CrashRpt
//
// Replaces:  CrashRpt (crash reporting)
// Status:    NO-OP. No crash reports are generated.
// Later:     Crashpad (Apache-2.0) or Sentry Native (MIT) -- see DEPENDENCIES.md.
//
// Clean-room declarations derived from call sites in src/Eternity/Source/r3dDebug.cpp.

#pragma once
#include <windows.h>

typedef BOOL (CALLBACK *LPGETLOGFILE)(LPVOID lpvState);

#define CR_INST_ALL_POSSIBLE_HANDLERS     0x1FFF
#define CR_INST_ALL_EXCEPTION_HANDLERS    0x1FFF
#define CR_INST_APP_RESTART               0x8000
#define CR_INST_HTTP_BINARY_ENCODING      0x10000
#define CR_INST_SEND_QUEUED_REPORTS       0x40000

// Delivery transports, in priority order.
#define CR_HTTP                 0
#define CR_SMTP                 1
#define CR_SMAPI                2
#define CR_NEGATIVE_PRIORITY    ((UINT)-1)

// crAddFile2 flags
#define CR_AF_TAKE_ORIGINAL_FILE  0
#define CR_AF_MAKE_FILE_COPY      1
#define CR_AF_FILE_MUST_EXIST     0
#define CR_AF_MISSING_FILE_OK     2

// Exception types for crEmulateCrash
#define CR_CPP_NEW_OPERATOR_ERROR 5

typedef struct tagCR_INSTALL_INFOA
{
    WORD   cb;
    LPCSTR pszAppName;
    LPCSTR pszAppVersion;
    LPCSTR pszEmailTo;
    LPCSTR pszEmailSubject;
    LPCSTR pszUrl;
    LPCSTR pszCrashSenderPath;
    LPGETLOGFILE pfnCrashCallback;
    UINT   uPriorities[3];
    DWORD  dwFlags;
    LPCSTR pszPrivacyPolicyURL;
    LPCSTR pszDebugHelpDLL;
    UINT   uMiniDumpType;
    LPCSTR pszErrorReportSaveDir;
    LPCSTR pszRestartCmdLine;
    LPCSTR pszLangFilePath;
    LPCSTR pszEmailText;
    LPCSTR pszSmtpProxy;
    LPCSTR pszCustomSenderIcon;
} CR_INSTALL_INFOA, *PCR_INSTALL_INFOA;
typedef CR_INSTALL_INFOA CR_INSTALL_INFO;

typedef struct tagCR_INSTALL_INFOW
{
    WORD    cb;
    LPCWSTR pszAppName;
    LPCWSTR pszAppVersion;
    LPCWSTR pszEmailTo;
    LPCWSTR pszEmailSubject;
    LPCWSTR pszUrl;
    LPCWSTR pszCrashSenderPath;
    LPGETLOGFILE pfnCrashCallback;
    UINT    uPriorities[3];
    DWORD   dwFlags;
    LPCWSTR pszPrivacyPolicyURL;
    LPCWSTR pszDebugHelpDLL;
    UINT    uMiniDumpType;
    LPCWSTR pszErrorReportSaveDir;
    LPCWSTR pszRestartCmdLine;
    LPCWSTR pszLangFilePath;
    LPCWSTR pszEmailText;
    LPCWSTR pszSmtpProxy;
    LPCWSTR pszCustomSenderIcon;
} CR_INSTALL_INFOW, *PCR_INSTALL_INFOW;

inline int crInstallA(PCR_INSTALL_INFOA)                 { return 0; }
inline int crInstallW(PCR_INSTALL_INFOW)                 { return 0; }
inline int crAddFile2W(LPCWSTR, LPCWSTR, LPCWSTR, DWORD) { return 0; }
inline int crGetLastErrorMsgW(LPWSTR, UINT)              { return 0; }
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
