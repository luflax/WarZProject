//=========================================================================
//  Chilkat compat layer -- WinHTTP transport, zlib gzip, local base64.
//
//  Implements the API declared in ../ChilKat/Include/CkShimCommon.h. See that header
//  for the choice of WinHTTP over libcurl and for the WARZ_HTTP_INSECURE switch.
//
//  This is the single highest-leverage piece of the shim layer, because it is on both
//  sides of the product:
//
//    client   CWOBackendReq -- login, character load, shop, every menu action
//    server   Async_ServerState / Async_ServerObjects / AsyncFuncs -- every
//             persistence call the GameServer makes (api_Srv*.aspx)
//    server   SupervisorServer's LogUploader -- multipart crash-log upload
//    both     CkString::base64Decode, on the startup path: ServerMain.cpp:290 and
//             SupervisorServer.cpp:152 decode the game name out of argv
//
//  Clean-room. No code originates from the Chilkat SDK.
//=========================================================================

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "CkHttp.h"
#include "CkHttpRequest.h"
#include "CkHttpResponse.h"
#include "CkGzip.h"
#include "CkString.h"
#include "CkByteData.h"

#include "zlib.h"

#include "WarzCompat.h"

namespace {

//////////////////////////////////////////////////////////////////////////
// Small helpers
//////////////////////////////////////////////////////////////////////////

std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return std::wstring();

    // The request path and header values may carry UTF-8 (put_Utf8(true) is set by
    // WOBackendAPI), so decode as UTF-8 and fall back to the ANSI code page if the
    // bytes are not valid UTF-8.
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), (int)s.size(),
                                nullptr, 0);
    UINT cp = CP_UTF8;

    if (n <= 0)
    {
        cp = CP_ACP;
        n = MultiByteToWideChar(cp, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (n <= 0)
            return std::wstring();
    }

    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                        s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

std::string Narrow(const std::wstring& s)
{
    if (s.empty())
        return std::string();

    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                      nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return std::string();

    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

bool IEquals(const std::string& a, const char* b)
{
    return _stricmp(a.c_str(), b) == 0;
}

// application/x-www-form-urlencoded percent-encoding. Unreserved set per RFC 3986,
// plus the historical '+' for space that form encoding uses.
std::string UrlEncode(const std::string& in)
{
    static const char hex[] = "0123456789ABCDEF";

    std::string out;
    out.reserve(in.size() + in.size() / 4);

    for (size_t i = 0; i < in.size(); ++i)
    {
        const unsigned char c = (unsigned char)in[i];

        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out.push_back((char)c);
        }
        else if (c == ' ')
        {
            out.push_back('+');
        }
        else
        {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }

    return out;
}

bool InsecureTlsRequested()
{
    // Read once. Changing it mid-run would silently change the security posture of
    // later requests, which is worse than requiring a restart.
    static int cached = -1;

    if (cached < 0)
    {
        char buf[8] = "";
        const DWORD n = GetEnvironmentVariableA("WARZ_HTTP_INSECURE", buf, sizeof(buf));
        cached = (n > 0 && n < sizeof(buf) && buf[0] != '0') ? 1 : 0;

        if (cached)
        {
            r3dOutToLog("HTTP: WARZ_HTTP_INSECURE is set -- TLS certificates will NOT "
                        "be validated. Do not use this against a public server.\n");
        }
    }

    return cached != 0;
}

//////////////////////////////////////////////////////////////////////////
// URL splitting
//////////////////////////////////////////////////////////////////////////

struct SplitUrl
{
    std::string scheme;
    std::string host;
    int         port = 80;
    std::string pathAndQuery = "/";
    bool        ok = false;
};

SplitUrl SplitAbsoluteUrl(const char* url)
{
    SplitUrl r;

    if (!url || !*url)
        return r;

    std::string s(url);

    size_t hostStart = 0;
    const size_t sep = s.find("://");
    if (sep != std::string::npos)
    {
        r.scheme = s.substr(0, sep);
        hostStart = sep + 3;
    }

    r.port = IEquals(r.scheme, "https") ? 443 : 80;

    const size_t slash = s.find('/', hostStart);
    std::string authority = (slash == std::string::npos)
                                ? s.substr(hostStart)
                                : s.substr(hostStart, slash - hostStart);

    if (slash != std::string::npos)
        r.pathAndQuery = s.substr(slash);

    // Strip any userinfo, then split off an explicit port. Guard against an IPv6
    // literal, whose colons are inside brackets.
    const size_t at = authority.find('@');
    if (at != std::string::npos)
        authority = authority.substr(at + 1);

    if (!authority.empty() && authority[0] == '[')
    {
        const size_t close = authority.find(']');
        if (close != std::string::npos)
        {
            r.host = authority.substr(1, close - 1);
            if (close + 1 < authority.size() && authority[close + 1] == ':')
                r.port = atoi(authority.c_str() + close + 2);
        }
    }
    else
    {
        const size_t colon = authority.find(':');
        if (colon != std::string::npos)
        {
            r.host = authority.substr(0, colon);
            r.port = atoi(authority.c_str() + colon + 1);
        }
        else
        {
            r.host = authority;
        }
    }

    r.ok = !r.host.empty();
    return r;
}

//////////////////////////////////////////////////////////////////////////
// Request body construction
//////////////////////////////////////////////////////////////////////////

std::string FormBody(const CkHttpRequest& req)
{
    std::string body;

    for (size_t i = 0; i < req.m_params.size(); ++i)
    {
        if (!body.empty())
            body.push_back('&');

        body += UrlEncode(req.m_params[i].first);
        body.push_back('=');
        body += UrlEncode(req.m_params[i].second);
    }

    return body;
}

std::string MultipartBoundary()
{
    // Uniqueness only has to hold within one process; the boundary must simply not
    // occur in the payload, and a counter plus the tick count is enough for that.
    static LONG counter = 0;

    char buf[64];
    _snprintf(buf, sizeof(buf) - 1, "----WarZBoundary%08lX%08lX",
              GetTickCount(), (unsigned long)InterlockedIncrement(&counter));
    buf[sizeof(buf) - 1] = 0;

    return std::string(buf);
}

std::string MultipartBody(const CkHttpRequest& req, const std::string& boundary)
{
    std::string body;

    for (size_t i = 0; i < req.m_params.size(); ++i)
    {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + req.m_params[i].first + "\"\r\n\r\n";
        body += req.m_params[i].second;
        body += "\r\n";
    }

    for (size_t i = 0; i < req.m_uploads.size(); ++i)
    {
        const CkHttpRequest::UploadPart& p = req.m_uploads[i];

        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + p.name +
                "\"; filename=\"" + p.fileName + "\"\r\n";
        body += "Content-Type: " +
                (p.contentType.empty() ? std::string("application/octet-stream") : p.contentType) +
                "\r\n\r\n";
        body += p.data;
        body += "\r\n";
    }

    body += "--" + boundary + "--\r\n";
    return body;
}

//////////////////////////////////////////////////////////////////////////
// Header parsing
//////////////////////////////////////////////////////////////////////////

void ParseRawHeaders(const std::wstring& raw,
                     std::vector<std::pair<std::string, std::string> >& out)
{
    const std::string all = Narrow(raw);

    size_t pos = 0;
    while (pos < all.size())
    {
        size_t eol = all.find("\r\n", pos);
        if (eol == std::string::npos)
            eol = all.size();

        const std::string line = all.substr(pos, eol - pos);
        pos = eol + 2;

        // The first line is the status line, and there is no colon-separated name in
        // it -- skipping it falls out of the find() below returning npos.
        const size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string name  = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
            value.erase(0, 1);

        out.push_back(std::make_pair(name, value));
    }
}

//////////////////////////////////////////////////////////////////////////
// base64
//////////////////////////////////////////////////////////////////////////

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int B64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+')             return 62;
    if (c == '/')             return 63;
    return -1;
}

std::string Base64Encode(const std::string& in)
{
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < in.size())
    {
        const unsigned v = ((unsigned char)in[i] << 16) |
                           ((unsigned char)in[i + 1] << 8) |
                            (unsigned char)in[i + 2];
        out.push_back(kB64[(v >> 18) & 0x3F]);
        out.push_back(kB64[(v >> 12) & 0x3F]);
        out.push_back(kB64[(v >>  6) & 0x3F]);
        out.push_back(kB64[ v        & 0x3F]);
        i += 3;
    }

    const size_t rest = in.size() - i;
    if (rest == 1)
    {
        const unsigned v = (unsigned char)in[i] << 16;
        out.push_back(kB64[(v >> 18) & 0x3F]);
        out.push_back(kB64[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    }
    else if (rest == 2)
    {
        const unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8);
        out.push_back(kB64[(v >> 18) & 0x3F]);
        out.push_back(kB64[(v >> 12) & 0x3F]);
        out.push_back(kB64[(v >>  6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}

// Tolerant of whitespace and of missing padding, and of the URL-safe alphabet, since
// the encoded game name arrives on a command line.
bool Base64Decode(const std::string& in, std::string& out)
{
    out.clear();
    out.reserve((in.size() / 4) * 3);

    unsigned accum = 0;
    int bits = 0;

    for (size_t i = 0; i < in.size(); ++i)
    {
        char c = in[i];

        if (c == '=' )
            break;
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
            continue;

        if (c == '-') c = '+';      // URL-safe alphabet
        if (c == '_') c = '/';

        const int v = B64Value(c);
        if (v < 0)
            return false;

        accum = (accum << 6) | (unsigned)v;
        bits += 6;

        if (bits >= 8)
        {
            bits -= 8;
            out.push_back((char)((accum >> bits) & 0xFF));
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
// zlib
//////////////////////////////////////////////////////////////////////////

// windowBits selects the framing: 15 is a zlib stream, 15|16 is gzip, -15 is raw
// deflate. The backend's exact framing is not documented anywhere in this tree, so
// all three are tried. Chilkat's CkGzip wrote gzip, so that is first.
bool Inflate(const unsigned char* src, size_t srcLen, std::string& out, int windowBits)
{
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit2(&zs, windowBits) != Z_OK)
        return false;

    zs.next_in  = const_cast<Bytef*>(src);
    zs.avail_in = (uInt)srcLen;

    out.clear();

    char buf[64 * 1024];
    int rc = Z_OK;

    do
    {
        const uInt inBefore = zs.avail_in;

        zs.next_out  = (Bytef*)buf;
        zs.avail_out = sizeof(buf);

        rc = inflate(&zs, Z_NO_FLUSH);

        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR)
        {
            inflateEnd(&zs);
            return false;
        }

        const size_t produced = sizeof(buf) - zs.avail_out;
        out.append(buf, produced);

        // Bail on a round that neither consumed input nor produced output. This
        // matters because the input here came off the network: a truncated stream
        // leaves inflate asking for more data it will never get, and without this the
        // loop spins forever rather than reporting a corrupt response. The rc alone
        // is not a reliable signal -- zlib may answer Z_OK or Z_BUF_ERROR for the
        // same "needs more input" condition.
        if (produced == 0 && zs.avail_in == inBefore)
        {
            inflateEnd(&zs);
            return false;
        }
    }
    while (rc != Z_STREAM_END);

    inflateEnd(&zs);
    return true;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// CkByteData
//////////////////////////////////////////////////////////////////////////

bool CkByteData::loadFile(const char* path)
{
    clear();

    if (!path)
        return false;

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 0x7FFFFFFF)
    {
        CloseHandle(h);
        return false;
    }

    std::vector<char> buf((size_t)size.QuadPart);

    bool ok = true;
    size_t done = 0;

    while (done < buf.size())
    {
        DWORD read = 0;
        const DWORD chunk = (DWORD)((buf.size() - done > 0x00100000) ? 0x00100000
                                                                     : buf.size() - done);
        if (!ReadFile(h, &buf[done], chunk, &read, nullptr) || read == 0)
        {
            ok = false;
            break;
        }
        done += read;
    }

    CloseHandle(h);

    if (ok && !buf.empty())
        append(&buf[0], (unsigned long)buf.size());

    return ok;
}

bool CkByteData::saveFile(const char* path) const
{
    if (!path)
        return false;

    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    bool ok = true;
    const unsigned long size = getSize();

    if (size)
    {
        DWORD written = 0;
        ok = WriteFile(h, getBytes(), size, &written, nullptr) && written == size;
    }

    CloseHandle(h);
    return ok;
}

//////////////////////////////////////////////////////////////////////////
// CkString
//////////////////////////////////////////////////////////////////////////

bool CkString::base64Decode(const char*)
{
    std::string decoded;
    if (!Base64Decode(m_s, decoded))
    {
        r3dOutToLog("CkString::base64Decode: input is not valid base64 (%u bytes)\n",
                    (unsigned)m_s.size());
        return false;
    }

    m_s.swap(decoded);
    return true;
}

bool CkString::base64Encode(const char*)
{
    m_s = Base64Encode(m_s);
    return true;
}

//////////////////////////////////////////////////////////////////////////
// CkHttpResponse
//////////////////////////////////////////////////////////////////////////

void CkHttpResponse::get_Body(CkByteData& out) const
{
    out.clear();
    if (!m_body.empty())
        out.append(m_body.data(), (unsigned long)m_body.size());
}

const char* CkHttpResponse::getHeaderField(const char* name) const
{
    if (!name)
        return nullptr;

    for (size_t i = 0; i < m_headers.size(); ++i)
    {
        if (_stricmp(m_headers[i].first.c_str(), name) == 0)
            return m_headers[i].second.c_str();
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// CkHttpRequest
//////////////////////////////////////////////////////////////////////////

void CkHttpRequest::UseUpload()
{
    m_upload = true;
    m_verb   = "POST";   // multipart is only meaningful on a POST
}

void CkHttpRequest::AddParam(const char* name, const char* value)
{
    if (!name)
        return;

    m_params.push_back(Pair(name, value ? value : ""));
}

void CkHttpRequest::AddHeader(const char* name, const char* value)
{
    if (!name)
        return;

    m_headers.push_back(Pair(name, value ? value : ""));
}

void CkHttpRequest::AddStringForUpload(const char* name, const char* fileName,
                                       const char* data, const char* charset)
{
    UploadPart p;
    p.name        = name     ? name     : "";
    p.fileName    = fileName ? fileName : "";
    p.data        = data     ? data     : "";
    p.contentType = charset && *charset
                        ? std::string("text/plain; charset=") + charset
                        : std::string("text/plain");

    m_uploads.push_back(p);
    m_upload = true;
}

void CkHttpRequest::AddBytesForUpload(const char* name, const char* fileName,
                                      const CkByteData& data)
{
    UploadPart p;
    p.name        = name     ? name     : "";
    p.fileName    = fileName ? fileName : "";
    p.contentType = "application/octet-stream";

    if (data.getSize())
        p.data.assign((const char*)data.getBytes(), data.getSize());

    m_uploads.push_back(p);
    m_upload = true;
}

bool CkHttpRequest::SetFromUrl(const char* url)
{
    const SplitUrl s = SplitAbsoluteUrl(url);
    if (!s.ok)
        return false;

    m_host = s.host;
    m_path = s.pathAndQuery;
    return true;
}

//////////////////////////////////////////////////////////////////////////
// CkHttp
//////////////////////////////////////////////////////////////////////////

CkHttpResponse* CkHttp::SynchronousRequest(const char* host, int port, bool useSsl,
                                           CkHttpRequest& req)
{
    m_lastError.clear();

    if (!host || !*host)
    {
        m_lastError = "no host";
        return nullptr;
    }

    // ---- body and content type -------------------------------------------------

    std::string body;
    std::string contentType;
    std::string path = req.m_path;

    if (req.m_upload)
    {
        const std::string boundary = MultipartBoundary();
        body        = MultipartBody(req, boundary);
        contentType = "multipart/form-data; boundary=" + boundary;
    }
    else if (IEquals(req.m_verb, "POST"))
    {
        body        = FormBody(req);
        contentType = "application/x-www-form-urlencoded";
        if (req.m_sendCharset && !req.m_charset.empty())
            contentType += "; charset=" + req.m_charset;
    }
    else if (!req.m_params.empty())
    {
        // GET: parameters belong in the query string, appended to whatever query the
        // path already carries.
        path += (path.find('?') == std::string::npos) ? '?' : '&';
        path += FormBody(req);
    }

    // ---- session ---------------------------------------------------------------

    HINTERNET session = WinHttpOpen(L"WarZ/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        m_lastError = "WinHttpOpen failed";
        r3dOutToLog("HTTP: WinHttpOpen failed (error %lu)\n", GetLastError());
        return nullptr;
    }

    // Chilkat counted these in seconds; WinHTTP wants milliseconds. Resolve gets the
    // connect budget too, since a DNS stall is indistinguishable from a connect stall
    // from the caller's point of view.
    WinHttpSetTimeouts(session,
                       m_connectTimeout * 1000,
                       m_connectTimeout * 1000,
                       m_readTimeout    * 1000,
                       m_readTimeout    * 1000);

    HINTERNET connection = WinHttpConnect(session, Widen(host).c_str(),
                                          (INTERNET_PORT)port, 0);
    if (!connection)
    {
        m_lastError = "WinHttpConnect failed";
        r3dOutToLog("HTTP: cannot connect to %s:%d (error %lu)\n", host, port, GetLastError());
        WinHttpCloseHandle(session);
        return nullptr;
    }

    DWORD openFlags = useSsl ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET request = WinHttpOpenRequest(connection,
                                           Widen(req.m_verb).c_str(),
                                           Widen(path).c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, openFlags);
    if (!request)
    {
        m_lastError = "WinHttpOpenRequest failed";
        r3dOutToLog("HTTP: WinHttpOpenRequest failed for %s (error %lu)\n",
                    path.c_str(), GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return nullptr;
    }

    if (!m_followRedirects)
    {
        DWORD option = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof(option));
    }

    // Chilkat decompressed a Content-Encoding'd response transparently, and callers
    // depend on that: WOBackendAPI::ParseResult inspects the body's first two bytes
    // for its OWN "$1" gzip marker, which it would never find under an HTTP-level
    // encoding it did not expect. WinHTTP only does this when asked. The option is
    // Windows 8.1 and later, so the failure is ignored -- on an older system the
    // request simply goes out without Accept-Encoding and the server sends plain text.
    {
        DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(request, WINHTTP_OPTION_DECOMPRESSION,
                         &decompress, sizeof(decompress));
    }

    if (useSsl && InsecureTlsRequested())
    {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                      SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    }

    // ---- headers ---------------------------------------------------------------

    std::string headers;

    if (!contentType.empty())
        headers += "Content-Type: " + contentType + "\r\n";

    for (size_t i = 0; i < req.m_headers.size(); ++i)
        headers += req.m_headers[i].first + ": " + req.m_headers[i].second + "\r\n";

    const std::wstring headersW = Widen(headers);

    // ---- send ------------------------------------------------------------------

    BOOL ok = WinHttpSendRequest(request,
                                 headersW.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headersW.c_str(),
                                 headersW.empty() ? 0 : (DWORD)-1,
                                 body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                 (DWORD)body.size(),
                                 (DWORD)body.size(),
                                 0);

    if (ok)
        ok = WinHttpReceiveResponse(request, nullptr);

    if (!ok)
    {
        const DWORD err = GetLastError();

        char buf[256];
        _snprintf(buf, sizeof(buf) - 1, "WinHTTP error %lu", err);
        buf[sizeof(buf) - 1] = 0;
        m_lastError = buf;

        if (err == ERROR_WINHTTP_SECURE_FAILURE)
        {
            r3dOutToLog("HTTP: TLS validation failed for https://%s:%d%s -- if this is a "
                        "private server with a self-signed certificate, set "
                        "WARZ_HTTP_INSECURE=1\n", host, port, path.c_str());
        }
        else
        {
            r3dOutToLog("HTTP: %s %s:%d%s failed (error %lu)\n",
                        req.m_verb.c_str(), host, port, path.c_str(), err);
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return nullptr;
    }

    // ---- response --------------------------------------------------------------

    CkHttpResponse* resp = new CkHttpResponse();

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    resp->m_status = (int)status;

    DWORD rawSize = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &rawSize,
                        WINHTTP_NO_HEADER_INDEX);

    if (rawSize > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        std::wstring raw(rawSize / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, &raw[0], &rawSize,
                                WINHTTP_NO_HEADER_INDEX))
        {
            ParseRawHeaders(raw, resp->m_headers);
        }
    }

    // Content-Length is advisory -- a chunked response has none -- so it is only used
    // to drive progress reporting, never to size the read.
    unsigned long expected = 0;
    if (const char* cl = resp->getHeaderField("Content-Length"))
        expected = (unsigned long)strtoul(cl, nullptr, 10);

    if (m_progress)
        m_progress->HttpBeginReceive();

    const DWORD startTick = GetTickCount();
    bool aborted = false;

    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            r3dOutToLog("HTTP: read failed after %u bytes (error %lu)\n",
                        (unsigned)resp->m_body.size(), GetLastError());
            break;
        }

        if (available == 0)
            break;

        const size_t offset = resp->m_body.size();
        resp->m_body.resize(offset + available);

        DWORD read = 0;
        if (!WinHttpReadData(request, &resp->m_body[offset], available, &read))
        {
            resp->m_body.resize(offset);
            r3dOutToLog("HTTP: WinHttpReadData failed (error %lu)\n", GetLastError());
            break;
        }

        resp->m_body.resize(offset + read);

        if (read == 0)
            break;

        if (m_progress)
        {
            const DWORD elapsed = GetTickCount() - startTick;
            m_progress->ReceiveRate((unsigned long)resp->m_body.size(),
                                    elapsed ? (unsigned long)(resp->m_body.size() * 1000 / elapsed)
                                            : 0);

            if (expected)
            {
                int pct = (int)((resp->m_body.size() * 100) / expected);
                if (pct > 100)
                    pct = 100;

                bool abortFlag = false;
                m_progress->PercentDone(pct, &abortFlag);

                if (abortFlag)
                {
                    aborted = true;
                    break;
                }
            }
        }
    }

    if (m_progress)
        m_progress->HttpEndReceive(!aborted);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (aborted)
    {
        delete resp;
        m_lastError = "transfer aborted by progress callback";
        return nullptr;
    }

    return resp;
}

const char* CkHttp::getDomain(const char* url)
{
    const SplitUrl s = SplitAbsoluteUrl(url);
    if (!s.ok)
    {
        m_lastError = "cannot parse URL";
        return nullptr;
    }

    m_domainScratch = s.host;
    return m_domainScratch.c_str();
}

const char* CkHttp::quickGetStr(const char* url)
{
    const SplitUrl s = SplitAbsoluteUrl(url);
    if (!s.ok)
    {
        m_lastError = "cannot parse URL";
        return nullptr;
    }

    CkHttpRequest req;
    req.UseGet();
    req.put_Path(s.pathAndQuery.c_str());

    CkHttpResponse* resp = SynchronousRequest(s.host.c_str(), s.port,
                                              IEquals(s.scheme, "https"), req);
    if (!resp)
        return nullptr;

    m_quickGetScratch = resp->m_body;
    delete resp;

    return m_quickGetScratch.c_str();
}

//////////////////////////////////////////////////////////////////////////
// CkGzip
//////////////////////////////////////////////////////////////////////////

bool CkGzip::UncompressMemory(CkByteData& in, CkByteData& out)
{
    out.clear();

    if (in.getSize() == 0)
        return false;

    // gzip first (what Chilkat's CkGzip produced), then a zlib stream, then raw
    // deflate. The backend's framing is not recorded anywhere in this tree and the
    // cost of being wrong once is a failed login, so all three are tried.
    static const int kWindowBits[] = { 15 + 16, 15, -15 };

    for (size_t i = 0; i < sizeof(kWindowBits) / sizeof(kWindowBits[0]); ++i)
    {
        std::string result;
        if (Inflate(in.getBytes(), in.getSize(), result, kWindowBits[i]))
        {
            if (!result.empty())
                out.append(result.data(), (unsigned long)result.size());
            return true;
        }
    }

    r3dOutToLog("CkGzip: %u bytes decompressed as neither gzip, zlib nor raw deflate\n",
                (unsigned)in.getSize());
    return false;
}

bool CkGzip::CompressMemory(CkByteData& in, CkByteData& out)
{
    out.clear();

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    // 15|16 selects a gzip wrapper, matching what UncompressMemory tries first.
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
    {
        return false;
    }

    zs.next_in  = const_cast<Bytef*>(in.getBytes());
    zs.avail_in = (uInt)in.getSize();

    char buf[64 * 1024];
    int rc = Z_OK;

    do
    {
        zs.next_out  = (Bytef*)buf;
        zs.avail_out = sizeof(buf);

        rc = deflate(&zs, Z_FINISH);

        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR)
        {
            deflateEnd(&zs);
            out.clear();
            return false;
        }

        const size_t produced = sizeof(buf) - zs.avail_out;
        if (produced)
            out.append(buf, (unsigned long)produced);

        // Same no-progress guard as Inflate. deflate under Z_FINISH should always
        // advance, but a stalled round here would hang the caller just as hard.
        if (produced == 0 && rc != Z_STREAM_END)
        {
            deflateEnd(&zs);
            out.clear();
            return false;
        }
    }
    while (rc != Z_STREAM_END);

    deflateEnd(&zs);
    return true;
}
