//=========================================================================
//  D3DX9 shader compilation, on top of D3DCompile.
//
//  Implements D3DXCompileShader / D3DXCompileShaderFromFile / D3DXDisassembleShader
//  from ../dxsdk/Include/d3dx9.h against d3dcompiler_NN.dll.
//
//  WHY THIS IS A THIN BRIDGE AND NOT A REWRITE
//
//  The engine targets vs_3_0 / ps_3_0 (r3d.h:693-694), which d3dcompiler still
//  supports -- it dropped shader models 1.x, not 2 and 3. And the one part of D3DX9's
//  compiler API with no modern counterpart, ID3DXConstantTable, is not used anywhere
//  in this codebase: VShader.cpp:124 and PShader.cpp both pass NULL for it, and a grep
//  across Eternity, GameEngine and EclipseStudio finds no other reference. So the
//  mapping is mechanical:
//
//      ID3DXBuffer            ->  ID3DBlob        (identical shape; copied, not cast)
//      D3DXMACRO              ->  D3D_SHADER_MACRO
//      ID3DXInclude           ->  ID3DInclude     (IncludeAdapter, below)
//      D3DXSHADER_* flags     ->  D3DCOMPILE_*
//
//  WHY THE DLL IS LOADED DYNAMICALLY
//
//  Linking an import library would pin one specific d3dcompiler_NN.dll into the PE
//  imports, and which one exists depends on the machine: _47 ships with Windows 8 and
//  later, _43 with the legacy DirectX end-user redistributable. Probing at runtime
//  covers both, and turns "missing DLL" from a process that will not start into a
//  logged error on the first shader load.
//
//  Clean-room. No code originates from the DirectX SDK.
//=========================================================================

#include <windows.h>
#include <d3d9.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>

#include <cstring>
#include <new>
#include <vector>

#include "d3dx9.h"
#include "WarzCompat.h"

namespace {

//////////////////////////////////////////////////////////////////////////
// ID3DXBuffer over a byte range.
//
// A reinterpret_cast from ID3DBlob* would work -- both are IUnknown plus
// GetBufferPointer and GetBufferSize, in that order -- but it is a layout coincidence
// rather than a guarantee, and the copy costs one memcpy of a few KB per shader on a
// path that already touches the disk. Owning the bytes outright also means the caller
// can outlive the blob, which VShader.cpp's binary-cache write does.
//////////////////////////////////////////////////////////////////////////

class CompatBuffer final : public ID3DXBuffer
{
public:
    static CompatBuffer* Create(const void* data, SIZE_T size)
    {
        void* mem = ::operator new(sizeof(CompatBuffer), std::nothrow);
        if (!mem)
            return nullptr;

        BYTE* bytes = nullptr;
        if (size)
        {
            bytes = static_cast<BYTE*>(::operator new(size, std::nothrow));
            if (!bytes)
            {
                ::operator delete(mem);
                return nullptr;
            }
            if (data)
                memcpy(bytes, data, size);
            else
                memset(bytes, 0, size);
        }

        return new (mem) CompatBuffer(bytes, size);
    }

    // --- IUnknown ---
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;

        if (IsEqualIID(riid, IID_IUnknown))
        {
            *ppv = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }

        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&m_refs));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG n = InterlockedDecrement(&m_refs);
        if (n == 0)
        {
            ::operator delete(m_data);
            this->~CompatBuffer();
            ::operator delete(static_cast<void*>(this));
        }
        return static_cast<ULONG>(n);
    }

    // --- ID3DXBuffer ---
    LPVOID STDMETHODCALLTYPE GetBufferPointer() override { return m_data; }
    DWORD  STDMETHODCALLTYPE GetBufferSize()    override { return static_cast<DWORD>(m_size); }

private:
    CompatBuffer(BYTE* data, SIZE_T size)
        : m_refs(1), m_data(data), m_size(size) {}

    ~CompatBuffer() = default;

    LONG   m_refs;
    BYTE*  m_data;
    SIZE_T m_size;
};

CompatBuffer* BufferFromBlob(ID3DBlob* blob)
{
    if (!blob)
        return nullptr;

    return CompatBuffer::Create(blob->GetBufferPointer(), blob->GetBufferSize());
}

CompatBuffer* BufferFromString(const char* text)
{
    if (!text)
        return nullptr;

    // Include the terminator: every caller treats GetBufferPointer() as a C string.
    return CompatBuffer::Create(text, strlen(text) + 1);
}

//////////////////////////////////////////////////////////////////////////
// ID3DXInclude -> ID3DInclude
//
// r3dDXInclude (VShader.cpp:60) already resolves shader includes through the .wz
// filesystem and records what it opened, so all this has to do is change the enum
// type on the way through. The types are structurally identical and the two enum
// values -- LOCAL 0, SYSTEM 1 -- agree.
//////////////////////////////////////////////////////////////////////////

class IncludeAdapter final : public ID3DInclude
{
public:
    explicit IncludeAdapter(LPD3DXINCLUDE inner) : m_inner(inner) {}

    HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE type, const char* filename,
                                   const void* parentData, const void** data,
                                   UINT* bytes) override
    {
        if (!m_inner)
            return E_FAIL;

        return m_inner->Open(type == D3D_INCLUDE_SYSTEM ? D3DXINC_SYSTEM : D3DXINC_LOCAL,
                             filename, parentData, const_cast<LPCVOID*>(data), bytes);
    }

    HRESULT STDMETHODCALLTYPE Close(const void* data) override
    {
        return m_inner ? m_inner->Close(data) : E_FAIL;
    }

private:
    LPD3DXINCLUDE m_inner;
};

//////////////////////////////////////////////////////////////////////////
// d3dcompiler, resolved at first use
//////////////////////////////////////////////////////////////////////////

// Newest first. _47 is the one Windows itself ships; the rest come from various
// DirectX SDK vintages and the legacy end-user redistributable.
const char* const kCompilerDlls[] =
{
    "d3dcompiler_47.dll",
    "d3dcompiler_46.dll",
    "d3dcompiler_43.dll",
    "d3dcompiler_42.dll",
};

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
                                         ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
                                         ID3DBlob**, ID3DBlob**);

typedef HRESULT (WINAPI *PFN_D3DDisassemble)(LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob**);

struct Compiler
{
    HMODULE            module      = nullptr;
    PFN_D3DCompile     compile     = nullptr;
    PFN_D3DDisassemble disassemble = nullptr;
    bool               tried       = false;
};

Compiler& GetCompiler()
{
    static Compiler c;

    if (c.tried)
        return c;

    c.tried = true;

    for (size_t i = 0; i < sizeof(kCompilerDlls) / sizeof(kCompilerDlls[0]); ++i)
    {
        c.module = LoadLibraryA(kCompilerDlls[i]);
        if (!c.module)
            continue;

        c.compile = reinterpret_cast<PFN_D3DCompile>(
                        GetProcAddress(c.module, "D3DCompile"));

        if (c.compile)
        {
            // Present from _44 onwards; absent on _43, where the equivalent was
            // D3DDisassemble10. Optional -- only the debug shader dump uses it.
            c.disassemble = reinterpret_cast<PFN_D3DDisassemble>(
                                GetProcAddress(c.module, "D3DDisassemble"));

            r3dOutToLog("D3DX compat: shader compilation via %s\n", kCompilerDlls[i]);
            return c;
        }

        FreeLibrary(c.module);
        c.module = nullptr;
    }

    r3dOutToLog("D3DX compat: no usable d3dcompiler DLL found -- shaders cannot be "
                "compiled. Install the DirectX end-user runtime, or ship a prebuilt "
                "shader cache (see r3dVertexShader::LoadBinaryCache).\n");
    return c;
}

//////////////////////////////////////////////////////////////////////////
// Flag translation
//
// The D3DXSHADER_* values the engine may pass are numerically identical to their
// D3DCOMPILE_* counterparts -- both were minted from the same D3D10 constants -- so
// the flag word passes through unchanged. Only one bit is added.
//////////////////////////////////////////////////////////////////////////

UINT TranslateFlags(DWORD flags)
{
    // The HLSL in Data/Shaders was written for the 2010-era compiler, which was more
    // permissive about implicit truncation and about types in intrinsic calls. This
    // flag is exactly the escape hatch d3dcompiler provides for that, and without it
    // legitimate 2013 shader source is rejected as an error rather than a warning.
    return static_cast<UINT>(flags) | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
}

std::vector<D3D_SHADER_MACRO> TranslateMacros(const D3DXMACRO* defines)
{
    std::vector<D3D_SHADER_MACRO> out;

    if (defines)
    {
        // The array is terminated by an entry with a null Name; the engine builds
        // exactly that in r3dVertexShader::Load.
        for (const D3DXMACRO* m = defines; m->Name; ++m)
        {
            D3D_SHADER_MACRO d;
            d.Name       = m->Name;
            d.Definition = m->Definition;
            out.push_back(d);
        }
    }

    D3D_SHADER_MACRO terminator;
    terminator.Name       = nullptr;
    terminator.Definition = nullptr;
    out.push_back(terminator);

    return out;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Public API
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXCompileShader(LPCSTR pSrcData, UINT srcDataLen, const D3DXMACRO* pDefines,
                          LPD3DXINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile,
                          DWORD flags, LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs,
                          LPD3DXCONSTANTTABLE* ppConstantTable)
{
    if (ppShader)         *ppShader         = nullptr;
    if (ppErrorMsgs)      *ppErrorMsgs      = nullptr;

    // Not implemented, and nothing asks for it -- see the header comment. Reporting
    // null rather than silently ignoring the parameter means a future caller that does
    // want reflection gets a null dereference at its own call site instead of a
    // mysteriously empty constant table.
    if (ppConstantTable)  *ppConstantTable  = nullptr;

    Compiler& c = GetCompiler();
    if (!c.compile)
    {
        if (ppErrorMsgs)
            *ppErrorMsgs = BufferFromString("no d3dcompiler DLL is available");
        return E_NOTIMPL;
    }

    if (!pSrcData || srcDataLen == 0)
        return D3DERR_INVALIDCALL;

    const std::vector<D3D_SHADER_MACRO> macros = TranslateMacros(pDefines);

    IncludeAdapter includeAdapter(pInclude);

    ID3DBlob* code   = nullptr;
    ID3DBlob* errors = nullptr;

    const HRESULT hr = c.compile(pSrcData, srcDataLen,
                                 nullptr,                  // source name, for messages
                                 macros.empty() ? nullptr : &macros[0],
                                 pInclude ? &includeAdapter : nullptr,
                                 pFunctionName, pProfile,
                                 TranslateFlags(flags), 0,
                                 &code, &errors);

    if (ppErrorMsgs && errors)
        *ppErrorMsgs = BufferFromBlob(errors);

    if (errors)
        errors->Release();

    if (SUCCEEDED(hr) && code)
    {
        if (ppShader)
            *ppShader = BufferFromBlob(code);
    }

    if (code)
        code->Release();

    return hr;
}

HRESULT D3DXCompileShaderFromFileA(LPCSTR pSrcFile, const D3DXMACRO* pDefines,
                                   LPD3DXINCLUDE pInclude, LPCSTR pFunctionName,
                                   LPCSTR pProfile, DWORD flags, LPD3DXBUFFER* ppShader,
                                   LPD3DXBUFFER* ppErrorMsgs,
                                   LPD3DXCONSTANTTABLE* ppConstantTable)
{
    if (ppShader)        *ppShader        = nullptr;
    if (ppErrorMsgs)     *ppErrorMsgs     = nullptr;
    if (ppConstantTable) *ppConstantTable = nullptr;

    if (!pSrcFile)
        return D3DERR_INVALIDCALL;

    // Read through the plain CRT rather than r3d_open: this entry point is not on the
    // engine's shader path (r3dCompileShader opens the file itself and calls the
    // in-memory form), so it never needs to see inside a .wz archive.
    HANDLE h = CreateFileA(pSrcFile, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(GetLastError());

    const DWORD size = GetFileSize(h, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0)
    {
        CloseHandle(h);
        return E_FAIL;
    }

    std::vector<char> text(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(h, &text[0], size, &read, nullptr);
    CloseHandle(h);

    if (!ok || read != size)
        return E_FAIL;

    return D3DXCompileShader(&text[0], size, pDefines, pInclude, pFunctionName, pProfile,
                             flags, ppShader, ppErrorMsgs, ppConstantTable);
}

HRESULT D3DXDisassembleShader(const DWORD* pShader, BOOL enableColorCode,
                              LPCSTR pComments, LPD3DXBUFFER* ppDisassembly)
{
    if (ppDisassembly)
        *ppDisassembly = nullptr;

    Compiler& c = GetCompiler();
    if (!c.disassemble || !pShader)
        return E_NOTIMPL;

    // The bytecode's own length is not passed in by D3DX's signature. Shader Model 3
    // token streams end with the END token (0x0000FFFF), which is how the runtime
    // itself finds the extent, so walk to it. Bound the scan so a malformed stream
    // cannot run off the end of the address space.
    const DWORD kMaxTokens = 1u << 20;
    SIZE_T tokens = 0;
    while (tokens < kMaxTokens && pShader[tokens] != 0x0000FFFF)
        ++tokens;

    if (tokens >= kMaxTokens)
        return E_FAIL;

    ++tokens;   // include the END token itself

    ID3DBlob* text = nullptr;
    const HRESULT hr = c.disassemble(pShader, tokens * sizeof(DWORD),
                                     enableColorCode ? D3D_DISASM_ENABLE_COLOR_CODE : 0,
                                     pComments, &text);

    if (SUCCEEDED(hr) && text && ppDisassembly)
        *ppDisassembly = BufferFromBlob(text);

    if (text)
        text->Release();

    return hr;
}
