// COMPAT LAYER: Chilkat HTTP, gzip and string types.
//
// Replaces:  Chilkat HTTP + Chilkat Zip (commercial, paid).
// Backed by: WinHTTP for the transport, the zlib already vendored inside Eternity
//            (src/Eternity/Source/ZLib) for gzip, and a local base64.
// Status:    FUNCTIONAL. Requests are really issued and really answered.
//
// Implementation: ../../compat/ChilkatHttp.cpp. Declarations here are clean-room,
// derived from the call sites in Sources/Backend/, Sources/UI/FrontEndWarZ.cpp,
// Sources/GameCode/UserProfile.cpp, server SupervisorServer/LogUploader.cpp and
// server WO_GameServer/ServerMain.cpp. No code originates from the Chilkat SDK.
//
// WHY WinHTTP RATHER THAN libcurl (which DEPENDENCIES.md named)
//
// The target is Windows-only, winhttp.dll is present on every supported version, it
// speaks TLS through schannel with no certificate bundle to ship, and it adds nothing
// to the licence audit. libcurl would be the right answer the moment this port grows
// a second platform; today it would be a vendored dependency earning nothing.
//
// SELF-SIGNED CERTIFICATES
//
// The backend is reached over HTTPS (gDomainUseSSL, port 443) and a private server's
// certificate will not chain to a public root. Set the environment variable
// WARZ_HTTP_INSECURE=1 to skip certificate validation. It is off by default, and when
// a request fails validation the log says both what happened and that this switch
// exists.
//
// WHAT IS STILL MISSING
//
//   - CkHttp::quickGetStr is a convenience one-liner with no caller here; it is
//     implemented, but only for simple absolute URLs.
//   - Chilkat's cookie jar, authentication helpers and its own proxy configuration
//     have no callers and are not declared.

#pragma once

#include <cstring>
#include <string>
#include <utility>
#include <vector>

//////////////////////////////////////////////////////////////////////////
// CkByteData -- a growable byte buffer
//////////////////////////////////////////////////////////////////////////

class CkByteData
{
public:
    CkByteData() = default;

    const unsigned char* getBytes() const { return m_data.empty() ? nullptr
                                                  : (const unsigned char*)m_data.data(); }
    unsigned long getSize() const { return (unsigned long)m_data.size(); }
    void          clear()         { m_data.clear(); }
    void          append(const void* p, unsigned long n)
    { m_data.append((const char*)p, n); }
    // Chilkat's second append overload -- same behaviour, different name. Used by
    // SteamHelper.cpp to stage the Steam auth ticket.
    void          append2(const void* p, unsigned long n)
    { m_data.append((const char*)p, n); }
    void          appendChar(char c)          { m_data.push_back(c); }
    unsigned char getByte(unsigned long i) const
    { return i < m_data.size() ? (unsigned char)m_data[i] : 0; }
    // Chilkat's writable view of the buffer, used by WOBackendAPI to patch the
    // response in place before parsing.
    char*         getData()                   { return m_data.empty() ? nullptr : m_data.data(); }
    void          removeChunk(unsigned long offset, unsigned long n)
    {
        if (offset < m_data.size())
            m_data.erase(offset, n);
    }

    // Whole-file read/write. LogUploader.cpp:29 stages crash logs through loadFile.
    bool          loadFile(const char* path);
    bool          saveFile(const char* path) const;

private:
    std::string m_data;
};

//////////////////////////////////////////////////////////////////////////
// CkString -- a string with in-place base64
//
// Both base64 entry points transform the string IN PLACE, which is what the call
// sites expect: FrontEndWarZ::DecodeAuthParams decodes then XORs the result through
// getAnsi(), and CClientUserProfile::GenerateSessionKey XORs then encodes and reads
// the result through getUtf8(). ServerMain.cpp:290 and SupervisorServer.cpp:152 both
// decode the game name out of argv, so this is on the server's startup path.
//////////////////////////////////////////////////////////////////////////

class CkString
{
public:
    CkString() = default;

    const char* getString() const { return m_s.c_str(); }
    void        setString(const char* s) { m_s = s ? s : ""; }

    // Assigned from a char buffer at FrontEndWarZ.cpp:354.
    CkString& operator=(const char* s) { m_s = s ? s : ""; return *this; }

    // The charset argument is Chilkat's; both callers pass "utf-8", which is what the
    // decoded bytes already are, so it is accepted and ignored.
    bool base64Decode(const char* charset);
    bool base64Encode(const char* charset);

    // Chilkat returned an internally-owned buffer and FrontEndWarZ XORs it in place --
    // so these hand back the string's own writable storage (data() is non-const since
    // C++17), not c_str().
    char* getAnsi() { return m_s.empty() ? const_cast<char*>("") : m_s.data(); }
    char* getUtf8() { return getAnsi(); }

private:
    std::string m_s;
};

//////////////////////////////////////////////////////////////////////////
// CkHttpProgress -- transfer callbacks
//
// HttpDownload (Sources/Backend/HttpDownload.h) derives from this and declares all
// five. They are declared here so they actually override something; previously the
// base had only a destructor, so HttpDownload's methods were new virtuals that
// nothing ever called.
//////////////////////////////////////////////////////////////////////////

class CkHttpProgress
{
public:
    virtual ~CkHttpProgress() = default;

    virtual void PercentDone(int /*pctDone*/, bool* /*abort*/) {}
    virtual void ProgressInfo(const char* /*name*/, const char* /*value*/) {}
    virtual void HttpBeginReceive() {}
    virtual void HttpEndReceive(bool /*success*/) {}
    virtual void ReceiveRate(unsigned long /*byteCount*/, unsigned long /*bytesPerSec*/) {}
};

//////////////////////////////////////////////////////////////////////////
// CkHttpResponse
//
// Heap-allocated by CkHttp::SynchronousRequest and DELETED by the caller --
// WOBackendAPI uses SAFE_DELETE and HttpDownload a plain `delete`, so this is an
// ordinary object with a virtual destructor, not a COM-style refcounted one.
//////////////////////////////////////////////////////////////////////////

class CkHttpResponse
{
public:
    virtual ~CkHttpResponse() = default;

    int           get_StatusCode() const     { return m_status; }
    const char*   bodyStr() const            { return m_body.c_str(); }
    void          get_Body(CkByteData& out) const;
    // Case-insensitive, as HTTP headers are. Returns null when absent, which is what
    // HttpDownload::Get tests for after a 301.
    const char*   getHeaderField(const char* name) const;
    unsigned long get_ContentLength() const  { return (unsigned long)m_body.size(); }

    // Filled by CkHttp::SynchronousRequest.
    int         m_status = 0;
    std::string m_body;
    std::vector<std::pair<std::string, std::string> > m_headers;
};

//////////////////////////////////////////////////////////////////////////
// CkHttpRequest -- an accumulator
//
// Public state: CkHttp consumes it directly. This is a compat layer over a C++ API
// that was itself a thin builder, and hiding the fields behind accessors only CkHttp
// would call would be ceremony.
//////////////////////////////////////////////////////////////////////////

class CkHttpRequest
{
public:
    typedef std::pair<std::string, std::string> Pair;

    struct UploadPart
    {
        std::string name;
        std::string fileName;
        std::string contentType;
        std::string data;
    };

    void UsePost() { m_verb = "POST"; }
    void UseGet()  { m_verb = "GET";  }
    void UseUpload();

    void put_Path(const char* path) { m_path = path ? path : "/"; }
    void AddParam(const char* name, const char* value);
    void AddHeader(const char* name, const char* value);
    void AddStringForUpload(const char* name, const char* fileName,
                            const char* data, const char* charset);
    void AddBytesForUpload(const char* name, const char* fileName, const CkByteData& data);

    // Splits an absolute URL and keeps the path+query. The host and port are passed
    // to SynchronousRequest separately by every caller, so they are recorded but not
    // required.
    bool SetFromUrl(const char* url);

    void put_Charset(const char* cs)  { m_charset = cs ? cs : ""; }
    void put_SendCharset(bool on)     { m_sendCharset = on; }
    void put_Utf8(bool on)            { m_utf8 = on; }

    std::string m_verb        = "GET";   // Chilkat's default; WOBackendAPI opts into POST
    std::string m_path        = "/";
    std::string m_charset;
    std::string m_host;                  // from SetFromUrl, informational
    bool        m_sendCharset = false;
    bool        m_utf8        = false;
    bool        m_upload      = false;

    std::vector<Pair>       m_params;
    std::vector<Pair>       m_headers;
    std::vector<UploadPart> m_uploads;
};

//////////////////////////////////////////////////////////////////////////
// CkHttp -- the transport
//////////////////////////////////////////////////////////////////////////

class CkHttp
{
public:
    CkHttp() = default;

    // Chilkat's licence gate. There is no licence to check, so this always succeeds;
    // IsUnlocked() agrees with it rather than contradicting it as the no-op shim did.
    int  UnlockComponent(const char*)   { m_unlocked = true; return 1; }
    bool IsUnlocked() const             { return m_unlocked; }

    // Seconds, as Chilkat counted them.
    void put_ConnectTimeout(int s)      { m_connectTimeout = s; }
    void put_ReadTimeout(int s)         { m_readTimeout = s; }

    // Issues the request and returns a response the CALLER OWNS AND DELETES, or null
    // if the exchange never completed. Null is the failure signal every call site
    // already tests for.
    CkHttpResponse* SynchronousRequest(const char* host, int port, bool useSsl,
                                       CkHttpRequest& req);

    // One-shot GET of an absolute URL. The returned pointer is owned by this CkHttp
    // and stays valid until the next call.
    const char* quickGetStr(const char* url);

    // Extracts the host from an absolute URL. Also owned by this CkHttp and valid
    // until the next call -- HttpDownload::Get relies on exactly that lifetime,
    // re-calling it for the relocated URL after the first result is done with.
    const char* getDomain(const char* url);

    void put_EventCallbackObject(CkHttpProgress* p) { m_progress = p; }
    void put_UseBgThread(bool)                      {}   // always synchronous here
    void put_KeepEventLog(bool)                     {}
    void put_FollowRedirects(bool on)               { m_followRedirects = on; }
    bool put_SessionLogFilename(const char* path)   { m_sessionLog = path ? path : ""; return true; }

    const char* lastErrorText() const { return m_lastError.c_str(); }

private:
    bool            m_unlocked       = false;
    int             m_connectTimeout = 30;
    int             m_readTimeout    = 60;
    bool            m_followRedirects = true;
    CkHttpProgress* m_progress       = nullptr;
    std::string     m_sessionLog;
    std::string     m_lastError;
    std::string     m_domainScratch;
    std::string     m_quickGetScratch;
};

//////////////////////////////////////////////////////////////////////////
// CkGzip
//
// UncompressMemory is on the live path: WOBackendAPI::ParseResult gzip-decompresses
// any response the backend marks with a leading "$1".
//////////////////////////////////////////////////////////////////////////

class CkGzip
{
public:
    int  UnlockComponent(const char*) { return 1; }

    bool UncompressMemory(CkByteData& in, CkByteData& out);
    bool CompressMemory(CkByteData& in, CkByteData& out);
};

// Chilkat spells the class CkGzip and the header CkGzip.h; CkGZip is kept as an alias
// because this shim originally used that spelling.
typedef CkGzip CkGZip;
