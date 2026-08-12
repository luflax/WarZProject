// HOST-BUILD ONLY. Minimal IUnknown so d3dx9core.h's COM interface declarations parse
// on a Linux host. See d3d9.h in this directory for why the host build exists.
//
// The COM interfaces in d3dx9core.h (ID3DXBuffer, ID3DXInclude, ID3DXConstantTable)
// belong to the shim's IMAGING and SHADER halves, which the host build does not test --
// it only needs them to declare cleanly so that including d3dx9.h reaches the math.
//
// Nothing here is ever instantiated. On the real target these come from the Windows SDK.

#ifndef WARZ_TEST_HOST_UNKNWN_H
#define WARZ_TEST_HOST_UNKNWN_H

#include "d3d9.h"   // HRESULT, DWORD, LPVOID, STDMETHODCALLTYPE

// GUID / REFIID: QueryInterface's signature names them, so they must exist. Layout
// matches the Win32 definition; nothing in the host build reads the fields.
typedef struct _GUID {
    DWORD          Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;

typedef GUID        IID;
typedef const IID&  REFIID;

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual DWORD   STDMETHODCALLTYPE AddRef()  = 0;
    virtual DWORD   STDMETHODCALLTYPE Release() = 0;
    virtual ~IUnknown() = default;
};

#endif // WARZ_TEST_HOST_UNKNWN_H
