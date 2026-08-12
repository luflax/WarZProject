// COMPAT LAYER: CrashRpt
//
// Replaces:  CrashRpt (crash reporting)
// Status:    FUNCTIONAL, local only. crInstall installs a real unhandled-exception
//            filter that writes a minidump plus a text report and copies the files
//            registered with crAddFile2 alongside it. What it does NOT do is UPLOAD:
//            CrashRpt shipped a separate CrashSender.exe that posted the report to
//            pszUrl, and there is no such uploader here. Reports accumulate on disk.
//
// Implementation: compat/CrashReport.cpp. Declarations here are clean-room, derived
// from call sites in src/Eternity/Source/r3dDebug.cpp -- no vendor code.
//
// Note that r3dDebug.cpp already carries a complete minidump writer of its own,
// behind #ifdef DISABLE_CRASHRPT. That path is not the one taken by this build (the
// define is not set), and it only covers SetUnhandledExceptionFilter. This layer
// covers the same ground plus terminate/abort, the attached-file list, and the
// on-demand crGenerateErrorReport, so the two do not need to be reconciled.

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

// CrashRpt's convention throughout: 0 is success, non-zero is failure, and the reason
// is retrievable with crGetLastErrorMsg. r3dDebug.cpp relies on exactly that.

int crInstallA(PCR_INSTALL_INFOA pInfo);
int crInstallW(PCR_INSTALL_INFOW pInfo);
int crAddFile2W(LPCWSTR pszFile, LPCWSTR pszDestFile, LPCWSTR pszDesc, DWORD dwFlags);
int crGetLastErrorMsgW(LPWSTR pszBuffer, UINT uBuffSize);
int crUninstall();
int crInstallToCurrentThread2(DWORD dwFlags);
int crUninstallFromCurrentThread();
int crAddFile2A(LPCSTR pszFile, LPCSTR pszDestFile, LPCSTR pszDesc, DWORD dwFlags);
int crAddScreenshot2(DWORD dwFlags, int nJpegQuality);
int crGenerateErrorReport(LPVOID pExceptionInfo);
int crGetLastErrorMsgA(LPSTR pszBuffer, UINT uBuffSize);

#define crInstall           crInstallA
#define crAddFile2          crAddFile2A
#define crGetLastErrorMsg   crGetLastErrorMsgA
