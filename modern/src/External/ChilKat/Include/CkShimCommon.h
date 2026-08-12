// SHIM: shared Chilkat type surface. See any Ck*.h in this directory.
#pragma once

#include <cstring>
#include <string>

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
private:
    std::string m_data;
};

class CkString
{
public:
    CkString() = default;

    const char* getString() const { return m_s.c_str(); }
    void        setString(const char* s) { m_s = s ? s : ""; }

    // Assigned from a char buffer at FrontEndWarZ.cpp:354.
    CkString& operator=(const char* s) { m_s = s ? s : ""; return *this; }

    // In Chilkat these decode/encode the string IN PLACE. Left as no-ops: the only
    // caller (FrontEndWarZ, unpacking the launcher's auth token) then XORs the result
    // and parses it, and it already handles a malformed token. Reimplementing base64
    // here would be inventing behaviour the shim cannot honestly promise -- the real
    // replacement is libcurl plus a base64 routine, per ../../../../DEPENDENCIES.md.
    bool base64Decode(const char* /*charset*/) { return false; }
    bool base64Encode(const char* /*charset*/) { return false; }

    // Chilkat returns an internally-owned buffer, and FrontEndWarZ XORs it in place --
    // so this hands back the string's own writable storage (data() is non-const since
    // C++17), not c_str().
    char* getAnsi() { return m_s.data(); }
    char* getUtf8() { return m_s.data(); }

private:
    std::string m_s;
};

class CkHttpProgress { public: virtual ~CkHttpProgress() = default; };

class CkHttpResponse
{
public:
    virtual ~CkHttpResponse() = default;
    int         get_StatusCode() const           { return 0; }
    const char* bodyStr() const                  { return ""; }
    void        get_Body(CkByteData& out) const  { out.clear(); }
    // No response was ever received, so there are no headers and no content.
    const char* getHeaderField(const char*) const { return nullptr; }
    unsigned long get_ContentLength() const       { return 0; }
};

class CkHttpRequest
{
public:
    void UsePost()                                  {}
    void UseGet()                                   {}
    void put_Path(const char*)                      {}
    void AddParam(const char*, const char*)         {}
    void AddHeader(const char*, const char*)        {}
    void AddStringForUpload(const char*, const char*, const char*, const char*) {}
    // Chilkat parsed a full URL into host/path/query. Nothing is sent, so this only
    // has to be callable.
    bool SetFromUrl(const char*)                    { return false; }
    void put_Charset(const char*)                   {}
    void put_SendCharset(bool)                      {}
    void put_Utf8(bool)                             {}
};

class CkHttp
{
public:
    int  UnlockComponent(const char*)               { return 1; }
    void put_ConnectTimeout(int)                    {}
    void put_ReadTimeout(int)                       {}
    // Returns null so callers take their failure path rather than proceeding with
    // an empty-but-successful response.
    CkHttpResponse* SynchronousRequest(const char*, int, bool, CkHttpRequest&) { return nullptr; }
    const char*     quickGetStr(const char*)        { return nullptr; }
    bool            put_SessionLogFilename(const char*) { return true; }

    // Progress reporting and transfer options -- all inert; nothing ever transfers.
    void put_EventCallbackObject(CkHttpProgress*)   {}
    void put_UseBgThread(bool)                      {}
    void put_KeepEventLog(bool)                     {}
    void put_FollowRedirects(bool)                  {}

    // Chilkat parsed the host out of a URL. Returning null makes HttpDownload log
    // "failed to parse url" and abort, which is the honest outcome.
    const char* getDomain(const char*)              { return nullptr; }
};

// Chilkat spells the class CkGzip and the header CkGzip.h; CkGZip is kept as an alias
// because this shim originally used that spelling.
class CkGzip
{
public:
    int  UnlockComponent(const char*)                        { return 1; }
    bool UncompressMemory(CkByteData&, CkByteData& out)      { out.clear(); return false; }
    bool CompressMemory(CkByteData&, CkByteData& out)        { out.clear(); return false; }
};

typedef CkGzip CkGZip;
