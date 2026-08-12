//=========================================================================
//  CrashRpt compat layer -- local minidump crash reporting.
//
//  Implements the API declared in ../CrashRpt/include/CrashRpt.h against dbghelp's
//  MiniDumpWriteDump, which Eternity already links for StackWalker.cpp.
//
//  WHAT THIS DOES
//    - installs an unhandled-exception filter, a terminate handler and a SIGABRT
//      handler at crInstall
//    - on a crash, creates <save-dir>/CrashReports/<app>_<stamp>_<pid>/ containing
//      crashdump.dmp, report.txt, and a copy of every file registered with crAddFile2
//    - runs the pfnCrashCallback FIRST, because r3dDebug.cpp uses it to close and
//      flush r3dlog.txt -- which is one of the files it then asks to have copied
//
//  WHAT THIS DOES NOT DO
//    Upload. Real CrashRpt spawned CrashSender.exe, which posted the report to
//    info.pszUrl. There is no uploader here and info.pszUrl is recorded in report.txt
//    rather than contacted. Nothing leaves the machine.
//
//  Clean-room: written against the call sites in src/Eternity/Source/r3dDebug.cpp.
//  No code originates from the CrashRpt SDK.
//=========================================================================

#include <windows.h>
#include <dbghelp.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "CrashRpt/include/CrashRpt.h"
#include "WarzCompat.h"

namespace {

//////////////////////////////////////////////////////////////////////////
// Installed state.
//
// A crash handler runs in a process that has already lost its footing, so everything
// it needs is captured up front at crInstall time and nothing is allocated on the
// crash path that can be avoided. The std::wstrings below are built during install;
// the handler only reads them.
//////////////////////////////////////////////////////////////////////////

struct AttachedFile
{
    std::wstring path;          // source path, as registered
    std::wstring destName;      // name to give the copy; empty means "use the source's"
    std::wstring description;
    DWORD        flags;
};

struct InstallState
{
    bool          installed        = false;
    std::wstring  appName          = L"Application";
    std::wstring  appVersion       = L"0.0";
    std::wstring  url;                              // recorded, never contacted
    std::wstring  saveDir;                          // empty means "next to the exe"
    LPGETLOGFILE  callback         = nullptr;
    UINT          miniDumpType     = 0;             // 0 -> our default, see DumpType()
    std::vector<AttachedFile> files;

    LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
    std::terminate_handler       previousTerminate = nullptr;
    void (*previousAbort)(int)   = nullptr;

    // Set once, by whichever thread reaches the handler first. A crash inside the
    // crash handler must not recurse.
    LONG          handling         = 0;

    std::wstring  lastError        = L"";
};

InstallState& State()
{
    static InstallState s;
    return s;
}

//////////////////////////////////////////////////////////////////////////
// Narrow/wide conversion.
//
// The public API is doubled A/W and r3dDebug.cpp uses both halves -- crInstallW for
// the install (CrashRpt required a full path in pszLangFilePath, which is why the
// original code chose the wide form) and crAddFile2A/crGetLastErrorMsgA for the rest.
// Wide is treated as canonical and the narrow entry points convert.
//////////////////////////////////////////////////////////////////////////

std::wstring Widen(const char* s)
{
    if (!s || !*s)
        return std::wstring();

    const int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (n <= 0)
        return std::wstring();

    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, &out[0], n);
    return out;
}

std::string Narrow(const std::wstring& s)
{
    if (s.empty())
        return std::string();

    const int n = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return std::string();

    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, &out[0], n, nullptr, nullptr);
    return out;
}

void SetLastError(const wchar_t* msg)
{
    State().lastError = msg ? msg : L"";
}

//////////////////////////////////////////////////////////////////////////
// Dump configuration
//////////////////////////////////////////////////////////////////////////

MINIDUMP_TYPE DumpType()
{
    // Honour the caller's request when it made one. r3dDebug.cpp leaves uMiniDumpType
    // at zero, so the default below is what this build actually uses -- and it matches
    // the one r3dDebug.cpp's own DISABLE_CRASHRPT path picks, deliberately, so that
    // dumps are comparable whichever writer produced them.
    if (State().miniDumpType != 0)
        return static_cast<MINIDUMP_TYPE>(State().miniDumpType);

    return static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                                      MiniDumpScanMemory);
}

BOOL CALLBACK DumpFilterCallback(PVOID, const PMINIDUMP_CALLBACK_INPUT pInput,
                                 PMINIDUMP_CALLBACK_OUTPUT pOutput)
{
    if (!pInput || !pOutput)
        return FALSE;

    switch (pInput->CallbackType)
    {
    case IncludeModuleCallback:
    case IncludeThreadCallback:
    case ThreadCallback:
        return TRUE;

    case ModuleCallback:
        // Drop modules nothing in the dump actually points at. On a 22 MB statically
        // linked binary with the system DLLs loaded this is the difference between a
        // dump worth mailing and one that is mostly ntdll.
        if (!(pOutput->ModuleWriteFlags & ModuleReferencedByMemory))
            pOutput->ModuleWriteFlags &= ~ModuleWriteModule;
        return TRUE;

    default:
        return FALSE;
    }
}

//////////////////////////////////////////////////////////////////////////
// Report directory
//////////////////////////////////////////////////////////////////////////

std::wstring ExeDirectory()
{
    wchar_t buf[MAX_PATH] = L"";
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return L".";

    std::wstring path(buf);
    const size_t slash = path.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? std::wstring(L".") : path.substr(0, slash);
}

// Creates <base>\CrashReports\<app>_<yyyymmdd-hhmmss>_<pid> and returns it, or an
// empty string if it could not be created.
std::wstring CreateReportDirectory()
{
    const InstallState& st = State();

    std::wstring base = st.saveDir.empty() ? ExeDirectory() : st.saveDir;

    std::wstring reports = base + L"\\CrashReports";
    CreateDirectoryW(reports.c_str(), nullptr);   // may already exist; checked below

    SYSTEMTIME t;
    GetLocalTime(&t);

    wchar_t leaf[256];
    swprintf(leaf, 256, L"%s_%04u%02u%02u-%02u%02u%02u_%lu",
             st.appName.c_str(),
             t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
             GetCurrentProcessId());

    std::wstring dir = reports + L"\\" + leaf;

    if (!CreateDirectoryW(dir.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return std::wstring();
    }

    return dir;
}

const wchar_t* LeafName(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    return path.c_str() + (slash == std::wstring::npos ? 0 : slash + 1);
}

//////////////////////////////////////////////////////////////////////////
// report.txt -- what a human opens first
//////////////////////////////////////////////////////////////////////////

void WriteReportText(const std::wstring& dir, EXCEPTION_POINTERS* pep, const char* reason)
{
    const InstallState& st = State();

    const std::wstring path = dir + L"\\report.txt";

    FILE* f = _wfopen(path.c_str(), L"wt");
    if (!f)
        return;

    SYSTEMTIME t;
    GetLocalTime(&t);

    fprintf(f, "%s %s crash report\n", Narrow(st.appName).c_str(),
                                       Narrow(st.appVersion).c_str());
    fprintf(f, "-------------------------------------------------------------\n");
    fprintf(f, "when       : %04u-%02u-%02u %02u:%02u:%02u\n",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    fprintf(f, "process    : %lu\n", GetCurrentProcessId());
    fprintf(f, "thread     : %lu\n", GetCurrentThreadId());
    fprintf(f, "reason     : %s\n", reason ? reason : "unhandled exception");

    if (pep && pep->ExceptionRecord)
    {
        fprintf(f, "code       : 0x%08lX\n", pep->ExceptionRecord->ExceptionCode);
        fprintf(f, "address    : 0x%p\n",    pep->ExceptionRecord->ExceptionAddress);

        // For access violations the record carries what was touched and how, and that
        // pair answers most "what happened" questions without opening the dump at all.
        if (pep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            pep->ExceptionRecord->NumberParameters >= 2)
        {
            const ULONG_PTR op   = pep->ExceptionRecord->ExceptionInformation[0];
            const ULONG_PTR addr = pep->ExceptionRecord->ExceptionInformation[1];
            fprintf(f, "access     : %s at 0x%p\n",
                    op == 0 ? "read" : (op == 1 ? "write" : "execute"),
                    reinterpret_cast<void*>(addr));
        }
    }

    if (!st.url.empty())
    {
        fprintf(f, "\n");
        fprintf(f, "This report was NOT uploaded. The configured endpoint was:\n");
        fprintf(f, "  %s\n", Narrow(st.url).c_str());
        fprintf(f, "CrashRpt's uploader (CrashSender.exe) is not part of this build.\n");
    }

    fprintf(f, "\nattached files\n");
    for (size_t i = 0; i < st.files.size(); ++i)
    {
        fprintf(f, "  %-24s %s\n",
                Narrow(LeafName(st.files[i].path)).c_str(),
                Narrow(st.files[i].description).c_str());
    }

    fclose(f);
}

//////////////////////////////////////////////////////////////////////////
// The dump itself
//////////////////////////////////////////////////////////////////////////

bool WriteDump(const std::wstring& dir, EXCEPTION_POINTERS* pep)
{
    const std::wstring path = dir + L"\\crashdump.dmp";

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId          = GetCurrentThreadId();
    mdei.ExceptionPointers = pep;
    mdei.ClientPointers    = FALSE;

    MINIDUMP_CALLBACK_INFORMATION mci;
    mci.CallbackRoutine = reinterpret_cast<MINIDUMP_CALLBACK_ROUTINE>(DumpFilterCallback);
    mci.CallbackParam   = nullptr;

    const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                      hFile, DumpType(),
                                      pep ? &mdei : nullptr, nullptr, &mci);

    CloseHandle(hFile);
    return ok != FALSE;
}

void CopyAttachedFiles(const std::wstring& dir)
{
    const InstallState& st = State();

    for (size_t i = 0; i < st.files.size(); ++i)
    {
        const AttachedFile& af = st.files[i];

        const std::wstring dest =
            dir + L"\\" + (af.destName.empty() ? std::wstring(LeafName(af.path))
                                               : af.destName);

        if (!CopyFileW(af.path.c_str(), dest.c_str(), FALSE))
        {
            // CR_AF_MISSING_FILE_OK is the normal case for gameSettings.ini and
            // GPU.txt on a first run -- absent is not an error, so it is not reported
            // as one. Everything else is worth a line in the log.
            if (!(af.flags & CR_AF_MISSING_FILE_OK))
            {
                r3dOutToLog("CrashRpt: could not attach '%s' (error %lu)\n",
                            Narrow(af.path).c_str(), GetLastError());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// The handler
//////////////////////////////////////////////////////////////////////////

void GenerateReport(EXCEPTION_POINTERS* pep, const char* reason, bool interactive)
{
    InstallState& st = State();

    // One report per process. A fault inside this function -- writing the dump of a
    // process whose heap may already be corrupt is not risk-free -- must not re-enter.
    if (InterlockedCompareExchange(&st.handling, 1, 0) != 0)
        return;

    // Before anything else: r3dDebug.cpp's callback closes r3dlog.txt. It has to run
    // before CopyAttachedFiles or the copied log is missing its last -- most
    // interesting -- buffered lines.
    if (st.callback)
        st.callback(nullptr);

    const std::wstring dir = CreateReportDirectory();
    if (dir.empty())
    {
        r3dOutToLog("CrashRpt: could not create a report directory (error %lu)\n",
                    GetLastError());
        return;
    }

    const bool dumped = WriteDump(dir, pep);

    WriteReportText(dir, pep, reason);
    CopyAttachedFiles(dir);

    const std::string dirNarrow = Narrow(dir);

    if (dumped)
        r3dOutToLog("CrashRpt: report written to %s\n", dirNarrow.c_str());
    else
        r3dOutToLog("CrashRpt: MiniDumpWriteDump failed (error %lu); wrote %s without a dump\n",
                    GetLastError(), dirNarrow.c_str());

    if (interactive)
    {
        char msg[1024];
        _snprintf(msg, sizeof(msg) - 1,
                  "%s stopped unexpectedly.\n\n"
                  "A crash report was written to:\n%s\n\n"
                  "Please send that folder to the developers along with a description "
                  "of what you were doing.",
                  Narrow(State().appName).c_str(), dirNarrow.c_str());
        msg[sizeof(msg) - 1] = 0;

        // Hide the game window first: a message box behind an exclusive-fullscreen
        // swap chain is a hang, not a dialog.
        HWND active = GetActiveWindow();
        if (active)
            ShowWindow(active, SW_FORCEMINIMIZE);

        MessageBoxA(nullptr, msg, "Crash", MB_OK | MB_ICONERROR);
    }
}

LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* pep)
{
    GenerateReport(pep, "unhandled exception", true);

    // Terminate rather than hand the exception on. Continuing the search reaches
    // Windows Error Reporting, which would produce a second dialog over the one just
    // shown; the report is already on disk by this point.
    return EXCEPTION_EXECUTE_HANDLER;
}

void TerminateHandler()
{
    GenerateReport(nullptr, "std::terminate", true);
    _exit(3);
}

void AbortHandler(int)
{
    GenerateReport(nullptr, "abort()", true);
    _exit(3);
}

// Shared tail of crInstallA/crInstallW, taking the fields already widened.
int InstallCommon(const std::wstring& appName, const std::wstring& appVersion,
                  const std::wstring& url, const std::wstring& saveDir,
                  LPGETLOGFILE callback, UINT miniDumpType)
{
    InstallState& st = State();

    if (st.installed)
    {
        SetLastError(L"crInstall called twice without an intervening crUninstall");
        return 1;
    }

    st.appName      = appName.empty()    ? std::wstring(L"Application") : appName;
    st.appVersion   = appVersion.empty() ? std::wstring(L"0.0")         : appVersion;
    st.url          = url;
    st.saveDir      = saveDir;
    st.callback     = callback;
    st.miniDumpType = miniDumpType;

    // The app name becomes a directory name, so it cannot carry separators.
    for (size_t i = 0; i < st.appName.size(); ++i)
    {
        const wchar_t c = st.appName[i];
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' ||
            c == L'"'  || c == L'<' || c == L'>' || c == L'|')
        {
            st.appName[i] = L'_';
        }
    }

    st.previousFilter    = SetUnhandledExceptionFilter(ExceptionFilter);
    st.previousTerminate = std::set_terminate(TerminateHandler);
    st.previousAbort     = signal(SIGABRT, AbortHandler);

    st.installed = true;
    SetLastError(L"");

    r3dOutToLog("CrashRpt: installed; reports go to %s\\CrashReports\n",
                Narrow(st.saveDir.empty() ? ExeDirectory() : st.saveDir).c_str());

    return 0;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Public API
//////////////////////////////////////////////////////////////////////////

int crInstallW(PCR_INSTALL_INFOW pInfo)
{
    if (!pInfo)
    {
        SetLastError(L"crInstallW: null CR_INSTALL_INFOW");
        return 1;
    }

    return InstallCommon(pInfo->pszAppName    ? pInfo->pszAppName    : L"",
                         pInfo->pszAppVersion ? pInfo->pszAppVersion : L"",
                         pInfo->pszUrl        ? pInfo->pszUrl        : L"",
                         pInfo->pszErrorReportSaveDir ? pInfo->pszErrorReportSaveDir : L"",
                         pInfo->pfnCrashCallback,
                         pInfo->uMiniDumpType);
}

int crInstallA(PCR_INSTALL_INFOA pInfo)
{
    if (!pInfo)
    {
        SetLastError(L"crInstallA: null CR_INSTALL_INFOA");
        return 1;
    }

    return InstallCommon(Widen(pInfo->pszAppName),
                         Widen(pInfo->pszAppVersion),
                         Widen(pInfo->pszUrl),
                         Widen(pInfo->pszErrorReportSaveDir),
                         pInfo->pfnCrashCallback,
                         pInfo->uMiniDumpType);
}

int crUninstall()
{
    InstallState& st = State();

    if (!st.installed)
        return 0;

    SetUnhandledExceptionFilter(st.previousFilter);
    std::set_terminate(st.previousTerminate);
    signal(SIGABRT, st.previousAbort ? st.previousAbort : SIG_DFL);

    st.previousFilter    = nullptr;
    st.previousTerminate = nullptr;
    st.previousAbort     = nullptr;
    st.callback          = nullptr;
    st.files.clear();
    st.installed         = false;

    return 0;
}

int crAddFile2W(LPCWSTR pszFile, LPCWSTR pszDestFile, LPCWSTR pszDesc, DWORD dwFlags)
{
    InstallState& st = State();

    if (!st.installed)
    {
        SetLastError(L"crAddFile2 called before crInstall");
        return 1;
    }

    if (!pszFile || !*pszFile)
    {
        SetLastError(L"crAddFile2: empty path");
        return 1;
    }

    // CR_AF_FILE_MUST_EXIST is zero, so "must exist" is the absence of
    // CR_AF_MISSING_FILE_OK rather than a bit of its own.
    if (!(dwFlags & CR_AF_MISSING_FILE_OK) &&
        GetFileAttributesW(pszFile) == INVALID_FILE_ATTRIBUTES)
    {
        SetLastError(L"crAddFile2: file does not exist and CR_AF_MISSING_FILE_OK was not set");
        return 1;
    }

    AttachedFile af;
    af.path        = pszFile;
    af.destName    = pszDestFile ? pszDestFile : L"";
    af.description = pszDesc     ? pszDesc     : L"";
    af.flags       = dwFlags;

    st.files.push_back(af);

    SetLastError(L"");
    return 0;
}

int crAddFile2A(LPCSTR pszFile, LPCSTR pszDestFile, LPCSTR pszDesc, DWORD dwFlags)
{
    const std::wstring file = Widen(pszFile);
    const std::wstring dest = Widen(pszDestFile);
    const std::wstring desc = Widen(pszDesc);

    return crAddFile2W(file.empty() ? nullptr : file.c_str(),
                       dest.empty() ? nullptr : dest.c_str(),
                       desc.empty() ? nullptr : desc.c_str(),
                       dwFlags);
}

int crGenerateErrorReport(LPVOID pExceptionInfo)
{
    if (!State().installed)
    {
        SetLastError(L"crGenerateErrorReport called before crInstall");
        return 1;
    }

    // Real CrashRpt takes a CR_EXCEPTION_INFO here. Nothing in this codebase calls it
    // with one -- the only reference is a commented-out crEmulateCrash -- so the
    // parameter is treated as an optional EXCEPTION_POINTERS and the report is
    // generated non-interactively, since this path is a deliberate request rather
    // than a fault.
    GenerateReport(static_cast<EXCEPTION_POINTERS*>(pExceptionInfo),
                   "crGenerateErrorReport", false);
    return 0;
}

int crInstallToCurrentThread2(DWORD)
{
    // The handlers this layer installs are all process-wide: SetUnhandledExceptionFilter
    // covers every thread, and libstdc++'s terminate handler is a single global. There
    // is genuinely nothing per-thread left to do, so this succeeds rather than
    // pretending to install something.
    //
    // (Real CrashRpt needed this because it also hooked the per-thread CRT handlers --
    // _set_invalid_parameter_handler and friends -- which MinGW's CRT does not provide.)
    return State().installed ? 0 : 1;
}

int crUninstallFromCurrentThread()
{
    return 0;
}

int crAddScreenshot2(DWORD, int)
{
    // Real CrashRpt grabbed the desktop through GDI. Not implemented: a screenshot of
    // a D3D9 exclusive-fullscreen window comes out black anyway, and nothing in this
    // codebase calls it.
    SetLastError(L"crAddScreenshot2 is not implemented in this build");
    return 1;
}

int crGetLastErrorMsgW(LPWSTR pszBuffer, UINT uBuffSize)
{
    if (!pszBuffer || uBuffSize == 0)
        return -1;

    const std::wstring& msg = State().lastError;
    const size_t n = (msg.size() < uBuffSize - 1) ? msg.size() : uBuffSize - 1;

    wmemcpy(pszBuffer, msg.c_str(), n);
    pszBuffer[n] = L'\0';

    return static_cast<int>(n);
}

int crGetLastErrorMsgA(LPSTR pszBuffer, UINT uBuffSize)
{
    if (!pszBuffer || uBuffSize == 0)
        return -1;

    const std::string msg = Narrow(State().lastError);
    const size_t n = (msg.size() < uBuffSize - 1) ? msg.size() : uBuffSize - 1;

    memcpy(pszBuffer, msg.c_str(), n);
    pszBuffer[n] = '\0';

    return static_cast<int>(n);
}
