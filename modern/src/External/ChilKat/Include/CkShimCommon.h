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
private:
    std::string m_data;
};

class CkString
{
public:
    const char* getString() const { return m_s.c_str(); }
    void        setString(const char* s) { m_s = s ? s : ""; }
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
};

class CkGZip
{
public:
    int  UnlockComponent(const char*)                        { return 1; }
    bool UncompressMemory(CkByteData&, CkByteData& out)      { out.clear(); return false; }
    bool CompressMemory(CkByteData&, CkByteData& out)        { out.clear(); return false; }
};
