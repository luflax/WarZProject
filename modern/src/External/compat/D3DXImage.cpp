//=========================================================================
//  D3DX9 image loading, reimplemented over DDS.
//
//  Implements the D3DXGetImageInfo* / D3DXCreateTexture* family from
//  ../dxsdk/Include/d3dx9.h. These are what actually get pixels into the engine:
//  every texture in the game funnels through two of them --
//
//      r3dTex.cpp:667            D3DXGetImageInfoFromFileInMemory
//      r3dRender.CPP:5923        D3DXCreateTextureFromFileInMemoryEx
//
//  -- reached from r3dTexture::LoadTextureInternal via r3dDeviceTunnel, plus the cube
//  and volume variants beside them.
//
//  WHY DDS ONLY
//
//  The shipped assets are DDS; the format is a header plus raw surfaces, so loading it
//  is a parse and a memcpy with no decoder and no third-party dependency. stb_image
//  (for PNG/TGA/JPG) was the fallback DEPENDENCIES.md names, but src/External/stb does
//  not exist in this tree, and adding an image library to load formats the game does
//  not ship would be scope for its own sake. Non-DDS input is rejected with a log
//  naming the file signature rather than silently producing a blank texture.
//
//  WHAT IS NOT IMPLEMENTED, AND WHY IT DOES NOT MATTER HERE
//
//  D3DX resampled on load: pass any Width/Height and it would filter the image to fit.
//  This does not. It honours a requested size only when that size is the file's own,
//  or is one of the file's mip levels -- in which case the smaller mips are uploaded
//  and the larger ones skipped, which is exact rather than filtered.
//
//  That covers what the engine asks for. r3dTexture::LoadTextureInternal computes its
//  downscale as a power-of-two mip count (`totalMipDown`, r3dTex.cpp:737) and, in the
//  common path, loads at FULL size with D3DX_FILTER_NONE and downscales afterwards by
//  copying mip levels itself (DoDownTex2D). The filtered path is the fallback taken
//  only when the downscale is non-uniform or breaks DXT block alignment. A request
//  this cannot satisfy exactly fails loudly instead of returning a wrong-sized
//  texture, because the caller records the size it asked for (r3dTex.cpp:843) and
//  every UV computation downstream would inherit the discrepancy.
//
//  Clean-room. Written from the DDS file format and the call sites. No code
//  originates from the DirectX SDK.
//=========================================================================

#include <windows.h>
#include <d3d9.h>

#include <cstring>
#include <vector>

#include "d3dx9.h"
#include "WarzCompat.h"

namespace {

//////////////////////////////////////////////////////////////////////////
// DDS file format
//////////////////////////////////////////////////////////////////////////

const DWORD kDdsMagic = 0x20534444;   // 'DDS '

const DWORD DDSD_MIPMAPCOUNT = 0x00020000;
const DWORD DDSD_DEPTH       = 0x00800000;

const DWORD DDPF_ALPHAPIXELS = 0x00000001;
const DWORD DDPF_ALPHA       = 0x00000002;
const DWORD DDPF_FOURCC      = 0x00000004;
const DWORD DDPF_RGB         = 0x00000040;
const DWORD DDPF_LUMINANCE   = 0x00020000;

const DWORD DDSCAPS2_CUBEMAP = 0x00000200;
const DWORD DDSCAPS2_VOLUME  = 0x00200000;

#pragma pack(push, 1)

struct DdsPixelFormat
{
    DWORD size;
    DWORD flags;
    DWORD fourCC;
    DWORD rgbBitCount;
    DWORD rBitMask;
    DWORD gBitMask;
    DWORD bBitMask;
    DWORD aBitMask;
};

struct DdsHeader
{
    DWORD          size;              // 124
    DWORD          flags;
    DWORD          height;
    DWORD          width;
    DWORD          pitchOrLinearSize;
    DWORD          depth;
    DWORD          mipMapCount;
    DWORD          reserved1[11];
    DdsPixelFormat ddspf;
    DWORD          caps;
    DWORD          caps2;
    DWORD          caps3;
    DWORD          caps4;
    DWORD          reserved2;
};

#pragma pack(pop)

//////////////////////////////////////////////////////////////////////////
// Format description
//////////////////////////////////////////////////////////////////////////

bool IsBlockCompressed(D3DFORMAT fmt)
{
    switch (fmt)
    {
    case D3DFMT_DXT1: case D3DFMT_DXT2: case D3DFMT_DXT3:
    case D3DFMT_DXT4: case D3DFMT_DXT5:
        return true;
    default:
        return false;
    }
}

// Bytes per 4x4 block, for the compressed formats only.
UINT BlockBytes(D3DFORMAT fmt)
{
    return (fmt == D3DFMT_DXT1) ? 8u : 16u;
}

// Bytes per pixel for uncompressed formats; 0 means "not a format this file handles".
UINT BytesPerPixel(D3DFORMAT fmt)
{
    switch (fmt)
    {
    case D3DFMT_A8:
    case D3DFMT_L8:
    case D3DFMT_P8:                                          return 1;

    case D3DFMT_R5G6B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_A8L8:
    case D3DFMT_L16:
    case D3DFMT_R16F:
    case D3DFMT_V8U8:                                        return 2;

    case D3DFMT_R8G8B8:                                      return 3;

    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
    case D3DFMT_A2B10G10R10:
    case D3DFMT_A2R10G10B10:
    case D3DFMT_G16R16:
    case D3DFMT_G16R16F:
    case D3DFMT_R32F:                                        return 4;

    case D3DFMT_A16B16G16R16:
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_G32R32F:                                     return 8;

    case D3DFMT_A32B32G32R32F:                               return 16;

    default:                                                 return 0;
    }
}

// Bytes occupied by one mip surface of the given dimensions.
UINT SurfaceBytes(D3DFORMAT fmt, UINT w, UINT h)
{
    if (IsBlockCompressed(fmt))
    {
        const UINT bw = (w + 3) / 4 ? (w + 3) / 4 : 1;
        const UINT bh = (h + 3) / 4 ? (h + 3) / 4 : 1;
        return bw * bh * BlockBytes(fmt);
    }

    const UINT bpp = BytesPerPixel(fmt);
    return bpp ? w * h * bpp : 0;
}

// Rows to copy, and bytes per row, for a surface. Block formats store one row per
// four pixel rows.
void SurfaceRows(D3DFORMAT fmt, UINT w, UINT h, UINT& rows, UINT& rowBytes)
{
    if (IsBlockCompressed(fmt))
    {
        const UINT bw = (w + 3) / 4 ? (w + 3) / 4 : 1;
        rows     = (h + 3) / 4 ? (h + 3) / 4 : 1;
        rowBytes = bw * BlockBytes(fmt);
    }
    else
    {
        rows     = h;
        rowBytes = w * BytesPerPixel(fmt);
    }
}

UINT NextMip(UINT v)
{
    return (v > 1) ? (v / 2) : 1;
}

//////////////////////////////////////////////////////////////////////////
// DDS pixel format -> D3DFORMAT
//////////////////////////////////////////////////////////////////////////

bool MaskIs(const DdsPixelFormat& pf, DWORD bits, DWORD r, DWORD g, DWORD b, DWORD a)
{
    return pf.rgbBitCount == bits &&
           pf.rBitMask == r && pf.gBitMask == g &&
           pf.bBitMask == b && pf.aBitMask == a;
}

D3DFORMAT FormatFromPixelFormat(const DdsPixelFormat& pf)
{
    if (pf.flags & DDPF_FOURCC)
    {
        switch (pf.fourCC)
        {
        case MAKEFOURCC('D','X','T','1'): return D3DFMT_DXT1;
        case MAKEFOURCC('D','X','T','2'): return D3DFMT_DXT2;
        case MAKEFOURCC('D','X','T','3'): return D3DFMT_DXT3;
        case MAKEFOURCC('D','X','T','4'): return D3DFMT_DXT4;
        case MAKEFOURCC('D','X','T','5'): return D3DFMT_DXT5;
        case MAKEFOURCC('D','X','1','0'): return D3DFMT_UNKNOWN;   // DX10 extension header
        default:
            // The DDS writers of this era stored small D3DFORMAT enum values directly
            // in the FourCC field for the formats that have no mask representation --
            // the float and 16-bit-per-channel ones. Anything under 0x100 is one of
            // those rather than a four-character code.
            if (pf.fourCC < 0x100)
                return static_cast<D3DFORMAT>(pf.fourCC);
            return D3DFMT_UNKNOWN;
        }
    }

    if (pf.flags & DDPF_RGB)
    {
        const bool hasAlpha = (pf.flags & DDPF_ALPHAPIXELS) != 0;

        if (MaskIs(pf, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000))
            return hasAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
        if (MaskIs(pf, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000))
            return D3DFMT_X8R8G8B8;
        if (MaskIs(pf, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000))
            return hasAlpha ? D3DFMT_A8B8G8R8 : D3DFMT_X8B8G8R8;
        if (MaskIs(pf, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000))
            return D3DFMT_X8B8G8R8;
        if (MaskIs(pf, 32, 0x0000FFFF, 0xFFFF0000, 0x00000000, 0x00000000))
            return D3DFMT_G16R16;
        if (MaskIs(pf, 32, 0x000003FF, 0x000FFC00, 0x3FF00000, 0xC0000000))
            return D3DFMT_A2B10G10R10;

        if (MaskIs(pf, 24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000))
            return D3DFMT_R8G8B8;

        if (MaskIs(pf, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000))
            return D3DFMT_R5G6B5;
        if (MaskIs(pf, 16, 0x00007C00, 0x000003E0, 0x0000001F, 0x00008000))
            return D3DFMT_A1R5G5B5;
        if (MaskIs(pf, 16, 0x00007C00, 0x000003E0, 0x0000001F, 0x00000000))
            return D3DFMT_X1R5G5B5;
        if (MaskIs(pf, 16, 0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000))
            return D3DFMT_A4R4G4B4;

        return D3DFMT_UNKNOWN;
    }

    if (pf.flags & DDPF_LUMINANCE)
    {
        if (pf.rgbBitCount == 8  && pf.aBitMask == 0)          return D3DFMT_L8;
        if (pf.rgbBitCount == 16 && pf.aBitMask == 0xFF00)     return D3DFMT_A8L8;
        if (pf.rgbBitCount == 16 && pf.aBitMask == 0)          return D3DFMT_L16;
        return D3DFMT_UNKNOWN;
    }

    if (pf.flags & DDPF_ALPHA)
    {
        if (pf.rgbBitCount == 8)                               return D3DFMT_A8;
        return D3DFMT_UNKNOWN;
    }

    return D3DFMT_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////
// Parsed image
//////////////////////////////////////////////////////////////////////////

struct DdsImage
{
    const BYTE* pixels    = nullptr;   // first surface; the rest follow contiguously
    size_t      pixelSize = 0;         // bytes remaining from `pixels` to end of file
    UINT        width     = 0;
    UINT        height    = 0;
    UINT        depth     = 1;
    UINT        mipLevels = 1;
    D3DFORMAT   format    = D3DFMT_UNKNOWN;
    bool        isCube    = false;
    bool        isVolume  = false;
};

// Describes what went wrong well enough to act on, without needing the filename --
// which these entry points are not given.
void LogSignature(const void* data, UINT size, const char* what)
{
    if (!data || size < 4)
    {
        r3dOutToLog("D3DX compat: %s -- input is empty or shorter than a signature\n", what);
        return;
    }

    const BYTE* b = static_cast<const BYTE*>(data);
    r3dOutToLog("D3DX compat: %s -- leading bytes %02X %02X %02X %02X ('%c%c%c%c'), "
                "%u bytes. Only DDS is supported by this build.\n",
                what, b[0], b[1], b[2], b[3],
                b[0] >= 32 && b[0] < 127 ? b[0] : '.',
                b[1] >= 32 && b[1] < 127 ? b[1] : '.',
                b[2] >= 32 && b[2] < 127 ? b[2] : '.',
                b[3] >= 32 && b[3] < 127 ? b[3] : '.',
                size);
}

bool ParseDds(const void* data, UINT size, DdsImage& out, const char* context)
{
    if (!data || size < sizeof(DWORD) + sizeof(DdsHeader))
    {
        LogSignature(data, size, context);
        return false;
    }

    const BYTE* base = static_cast<const BYTE*>(data);

    DWORD magic;
    memcpy(&magic, base, sizeof(magic));
    if (magic != kDdsMagic)
    {
        LogSignature(data, size, context);
        return false;
    }

    DdsHeader hdr;
    memcpy(&hdr, base + sizeof(DWORD), sizeof(hdr));

    if (hdr.size != sizeof(DdsHeader) || hdr.ddspf.size != sizeof(DdsPixelFormat))
    {
        r3dOutToLog("D3DX compat: %s -- DDS header sizes are %lu/%lu, expected %u/%u\n",
                    context, hdr.size, hdr.ddspf.size,
                    (unsigned)sizeof(DdsHeader), (unsigned)sizeof(DdsPixelFormat));
        return false;
    }

    out.format = FormatFromPixelFormat(hdr.ddspf);
    if (out.format == D3DFMT_UNKNOWN)
    {
        if ((hdr.ddspf.flags & DDPF_FOURCC) &&
            hdr.ddspf.fourCC == MAKEFOURCC('D','X','1','0'))
        {
            r3dOutToLog("D3DX compat: %s -- DDS carries a DX10 extension header, which "
                        "describes DXGI formats a Direct3D 9 device cannot consume\n",
                        context);
        }
        else
        {
            r3dOutToLog("D3DX compat: %s -- unrecognised DDS pixel format "
                        "(flags 0x%08lX, fourCC 0x%08lX, %lu bpp)\n",
                        context, hdr.ddspf.flags, hdr.ddspf.fourCC, hdr.ddspf.rgbBitCount);
        }
        return false;
    }

    out.width     = hdr.width  ? hdr.width  : 1;
    out.height    = hdr.height ? hdr.height : 1;
    out.isCube    = (hdr.caps2 & DDSCAPS2_CUBEMAP) != 0;
    out.isVolume  = (hdr.caps2 & DDSCAPS2_VOLUME) != 0;
    out.depth     = (out.isVolume && (hdr.flags & DDSD_DEPTH) && hdr.depth)
                        ? hdr.depth : 1;
    out.mipLevels = ((hdr.flags & DDSD_MIPMAPCOUNT) && hdr.mipMapCount)
                        ? hdr.mipMapCount : 1;

    out.pixels    = base + sizeof(DWORD) + sizeof(DdsHeader);
    out.pixelSize = size - sizeof(DWORD) - sizeof(DdsHeader);

    return true;
}

// Total bytes of one full mip chain starting at (w,h,d).
size_t MipChainBytes(const DdsImage& img, UINT w, UINT h, UINT d, UINT levels)
{
    size_t total = 0;
    for (UINT i = 0; i < levels; ++i)
    {
        total += static_cast<size_t>(SurfaceBytes(img.format, w, h)) * d;
        w = NextMip(w);
        h = NextMip(h);
        d = NextMip(d);
    }
    return total;
}

//////////////////////////////////////////////////////////////////////////
// Uncompressed format conversion
//
// Only needed when a caller asks for a format the file is not already in. In this
// codebase that is one call site out of ten -- everything else passes D3DFMT_UNKNOWN
// and takes the file's own format -- so this is a correctness backstop, not a hot
// path. Everything routes through B8G8R8A8 as an intermediate.
//////////////////////////////////////////////////////////////////////////

struct Bgra { BYTE b, g, r, a; };

BYTE Expand(DWORD value, DWORD mask)
{
    if (!mask)
        return 255;

    // Shift the field down, then scale it to 0..255 by replication rather than a
    // shift -- 5-bit 31 must become 255, not 248.
    DWORD shift = 0;
    while (!((mask >> shift) & 1))
        ++shift;

    DWORD width = 0;
    while (((mask >> shift) >> width) & 1)
        ++width;

    const DWORD field = (value & mask) >> shift;
    const DWORD maxv  = (1u << width) - 1;

    return maxv ? static_cast<BYTE>((field * 255 + maxv / 2) / maxv) : 0;
}

bool DecodeRow(D3DFORMAT fmt, const BYTE* src, Bgra* dst, UINT count)
{
    switch (fmt)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i].b = src[i * 4 + 0];
            dst[i].g = src[i * 4 + 1];
            dst[i].r = src[i * 4 + 2];
            dst[i].a = (fmt == D3DFMT_A8R8G8B8) ? src[i * 4 + 3] : 255;
        }
        return true;

    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i].r = src[i * 4 + 0];
            dst[i].g = src[i * 4 + 1];
            dst[i].b = src[i * 4 + 2];
            dst[i].a = (fmt == D3DFMT_A8B8G8R8) ? src[i * 4 + 3] : 255;
        }
        return true;

    case D3DFMT_R8G8B8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i].b = src[i * 3 + 0];
            dst[i].g = src[i * 3 + 1];
            dst[i].r = src[i * 3 + 2];
            dst[i].a = 255;
        }
        return true;

    case D3DFMT_R5G6B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A4R4G4B4:
        {
            DWORD rm, gm, bm, am;
            switch (fmt)
            {
            case D3DFMT_R5G6B5:   rm=0xF800; gm=0x07E0; bm=0x001F; am=0x0000; break;
            case D3DFMT_A1R5G5B5: rm=0x7C00; gm=0x03E0; bm=0x001F; am=0x8000; break;
            case D3DFMT_X1R5G5B5: rm=0x7C00; gm=0x03E0; bm=0x001F; am=0x0000; break;
            default:              rm=0x0F00; gm=0x00F0; bm=0x000F; am=0xF000; break;
            }

            for (UINT i = 0; i < count; ++i)
            {
                WORD v;
                memcpy(&v, src + i * 2, 2);
                dst[i].r = Expand(v, rm);
                dst[i].g = Expand(v, gm);
                dst[i].b = Expand(v, bm);
                dst[i].a = am ? Expand(v, am) : 255;
            }
        }
        return true;

    case D3DFMT_L8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i].r = dst[i].g = dst[i].b = src[i];
            dst[i].a = 255;
        }
        return true;

    case D3DFMT_A8L8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i].r = dst[i].g = dst[i].b = src[i * 2 + 0];
            dst[i].a = src[i * 2 + 1];
        }
        return true;

    case D3DFMT_A8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i].r = dst[i].g = dst[i].b = 255;
            dst[i].a = src[i];
        }
        return true;

    default:
        return false;
    }
}

bool EncodeRow(D3DFORMAT fmt, const Bgra* src, BYTE* dst, UINT count)
{
    switch (fmt)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i * 4 + 0] = src[i].b;
            dst[i * 4 + 1] = src[i].g;
            dst[i * 4 + 2] = src[i].r;
            dst[i * 4 + 3] = (fmt == D3DFMT_A8R8G8B8) ? src[i].a : BYTE(255);
        }
        return true;

    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i * 4 + 0] = src[i].r;
            dst[i * 4 + 1] = src[i].g;
            dst[i * 4 + 2] = src[i].b;
            dst[i * 4 + 3] = (fmt == D3DFMT_A8B8G8R8) ? src[i].a : BYTE(255);
        }
        return true;

    case D3DFMT_R8G8B8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i * 3 + 0] = src[i].b;
            dst[i * 3 + 1] = src[i].g;
            dst[i * 3 + 2] = src[i].r;
        }
        return true;

    case D3DFMT_R5G6B5:
        for (UINT i = 0; i < count; ++i)
        {
            const WORD v = WORD(((src[i].r >> 3) << 11) | ((src[i].g >> 2) << 5) | (src[i].b >> 3));
            memcpy(dst + i * 2, &v, 2);
        }
        return true;

    case D3DFMT_A1R5G5B5:
    case D3DFMT_X1R5G5B5:
        for (UINT i = 0; i < count; ++i)
        {
            const WORD alphaBit = (fmt == D3DFMT_A1R5G5B5 && src[i].a >= 128) ? 0x8000 : 0;
            const WORD v = WORD(alphaBit | ((src[i].r >> 3) << 10) |
                                           ((src[i].g >> 3) << 5)  | (src[i].b >> 3));
            memcpy(dst + i * 2, &v, 2);
        }
        return true;

    case D3DFMT_A4R4G4B4:
        for (UINT i = 0; i < count; ++i)
        {
            const WORD v = WORD(((src[i].a >> 4) << 12) | ((src[i].r >> 4) << 8) |
                                ((src[i].g >> 4) << 4)  |  (src[i].b >> 4));
            memcpy(dst + i * 2, &v, 2);
        }
        return true;

    case D3DFMT_L8:
        for (UINT i = 0; i < count; ++i)
        {
            // Rec.601 luma, which is what D3DX used.
            dst[i] = BYTE((src[i].r * 77 + src[i].g * 150 + src[i].b * 29) >> 8);
        }
        return true;

    case D3DFMT_A8L8:
        for (UINT i = 0; i < count; ++i)
        {
            dst[i * 2 + 0] = BYTE((src[i].r * 77 + src[i].g * 150 + src[i].b * 29) >> 8);
            dst[i * 2 + 1] = src[i].a;
        }
        return true;

    case D3DFMT_A8:
        for (UINT i = 0; i < count; ++i)
            dst[i] = src[i].a;
        return true;

    default:
        return false;
    }
}

// Copies one surface into a locked destination, converting if the formats differ.
// Returns false only for a conversion pair this file cannot perform.
bool CopySurface(D3DFORMAT srcFmt, const BYTE* src,
                 D3DFORMAT dstFmt, BYTE* dst, int dstPitch,
                 UINT w, UINT h)
{
    UINT rows, rowBytes;
    SurfaceRows(srcFmt, w, h, rows, rowBytes);

    if (srcFmt == dstFmt)
    {
        for (UINT y = 0; y < rows; ++y)
            memcpy(dst + static_cast<size_t>(y) * dstPitch, src + static_cast<size_t>(y) * rowBytes, rowBytes);
        return true;
    }

    // Conversion is only possible between uncompressed formats -- decoding DXT would
    // need a block decompressor, which nothing here asks for.
    if (IsBlockCompressed(srcFmt) || IsBlockCompressed(dstFmt))
        return false;

    const UINT dstBpp = BytesPerPixel(dstFmt);
    if (!dstBpp)
        return false;

    std::vector<Bgra> scratch(w);

    for (UINT y = 0; y < h; ++y)
    {
        if (!DecodeRow(srcFmt, src + static_cast<size_t>(y) * rowBytes, &scratch[0], w))
            return false;
        if (!EncodeRow(dstFmt, &scratch[0], dst + static_cast<size_t>(y) * dstPitch, w))
            return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
// Request resolution
//////////////////////////////////////////////////////////////////////////

bool IsDefaultDim(UINT v)
{
    return v == 0 || v == D3DX_DEFAULT || v == D3DX_DEFAULT_NONPOW2;
}

// Finds the mip index whose dimensions match the requested ones, so a downscale can
// be served by skipping the larger levels. Returns false when no level matches.
bool FindStartMip(const DdsImage& img, UINT reqW, UINT reqH, UINT reqD, UINT& startMip)
{
    UINT w = img.width, h = img.height, d = img.depth;

    for (UINT i = 0; i < img.mipLevels; ++i)
    {
        if (w == reqW && h == reqH && d == reqD)
        {
            startMip = i;
            return true;
        }
        w = NextMip(w);
        h = NextMip(h);
        d = NextMip(d);
    }

    return false;
}

// Advances a surface pointer past `count` mip levels of a 2D/volume chain.
const BYTE* SkipMips(const DdsImage& img, const BYTE* p, UINT& w, UINT& h, UINT& d, UINT count)
{
    for (UINT i = 0; i < count; ++i)
    {
        p += static_cast<size_t>(SurfaceBytes(img.format, w, h)) * d;
        w = NextMip(w);
        h = NextMip(h);
        d = NextMip(d);
    }
    return p;
}

// The common preamble of all three create functions: parse, validate the request,
// and work out where in the file to start and how many levels to upload.
//
// `resolvedFormat` is the format the texture will be created with; it differs from
// img.format only when the caller asked for a specific one.
bool ResolveRequest(const void* data, UINT size, const char* context,
                    UINT reqW, UINT reqH, UINT reqD, UINT reqMips, D3DFORMAT reqFmt,
                    DdsImage& img, UINT& startMip, UINT& levels, D3DFORMAT& resolvedFormat,
                    UINT& outW, UINT& outH, UINT& outD)
{
    if (!ParseDds(data, size, img, context))
        return false;

    outW = IsDefaultDim(reqW) ? img.width  : reqW;
    outH = IsDefaultDim(reqH) ? img.height : reqH;
    outD = IsDefaultDim(reqD) ? img.depth  : reqD;

    if (!FindStartMip(img, outW, outH, outD, startMip))
    {
        r3dOutToLog("D3DX compat: %s -- asked for %ux%ux%u from a %ux%ux%u image with %u "
                    "mip level(s); this build resamples nothing, so only the image's own "
                    "size or one of its mip levels can be served\n",
                    context, outW, outH, outD, img.width, img.height, img.depth,
                    img.mipLevels);
        return false;
    }

    const UINT available = img.mipLevels - startMip;

    levels = (reqMips == 0 || reqMips == D3DX_DEFAULT || reqMips > available)
                 ? available : reqMips;

    resolvedFormat = (reqFmt == D3DFMT_UNKNOWN || reqFmt == static_cast<D3DFORMAT>(D3DX_DEFAULT))
                         ? img.format : reqFmt;

    if (resolvedFormat != img.format &&
        (IsBlockCompressed(resolvedFormat) || IsBlockCompressed(img.format)))
    {
        r3dOutToLog("D3DX compat: %s -- cannot convert between %u and %u; block-compressed "
                    "formats are uploaded as-is only\n",
                    context, (unsigned)img.format, (unsigned)resolvedFormat);
        return false;
    }

    return true;
}

void FillImageInfo(const DdsImage& img, D3DXIMAGE_INFO* info)
{
    if (!info)
        return;

    info->Width           = img.width;
    info->Height          = img.height;
    info->Depth           = img.depth;
    info->MipLevels       = img.mipLevels;
    info->Format          = img.format;
    info->ResourceType    = img.isCube   ? D3DRTYPE_CUBETEXTURE
                          : img.isVolume ? D3DRTYPE_VOLUMETEXTURE
                                         : D3DRTYPE_TEXTURE;
    info->ImageFileFormat = D3DXIFF_DDS;
}

// D3DPOOL_DEFAULT textures without D3DUSAGE_DYNAMIC cannot be locked, so they are
// filled through a SYSTEMMEM staging copy and UpdateTexture. Returns the pool to
// actually create the fillable texture in, and whether staging is needed.
bool NeedsStaging(D3DPOOL pool, DWORD usage)
{
    return pool == D3DPOOL_DEFAULT && !(usage & D3DUSAGE_DYNAMIC);
}

//////////////////////////////////////////////////////////////////////////
// Whole-file helper for the FromFile entry points
//////////////////////////////////////////////////////////////////////////

bool ReadWholeFile(const char* path, std::vector<BYTE>& out)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    const DWORD size = GetFileSize(h, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0)
    {
        CloseHandle(h);
        return false;
    }

    out.resize(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(h, &out[0], size, &read, nullptr);
    CloseHandle(h);

    return ok && read == size;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Image info
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT srcDataSize,
                                         D3DXIMAGE_INFO* pSrcInfo)
{
    DdsImage img;
    if (!ParseDds(pSrcData, srcDataSize, img, "D3DXGetImageInfoFromFileInMemory"))
        return D3DERR_INVALIDCALL;

    FillImageInfo(img, pSrcInfo);
    return S_OK;
}

HRESULT D3DXGetImageInfoFromFileA(LPCSTR pSrcFile, D3DXIMAGE_INFO* pSrcInfo)
{
    std::vector<BYTE> bytes;
    if (!pSrcFile || !ReadWholeFile(pSrcFile, bytes))
        return D3DERR_INVALIDCALL;

    return D3DXGetImageInfoFromFileInMemory(&bytes[0], (UINT)bytes.size(), pSrcInfo);
}

//////////////////////////////////////////////////////////////////////////
// 2D textures
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData,
                                            UINT srcDataSize, UINT width, UINT height,
                                            UINT mipLevels, DWORD usage, D3DFORMAT format,
                                            D3DPOOL pool, DWORD /*filter*/, DWORD /*mipFilter*/,
                                            D3DCOLOR /*colorKey*/, D3DXIMAGE_INFO* pSrcInfo,
                                            PALETTEENTRY* /*pPalette*/,
                                            LPDIRECT3DTEXTURE9* ppTexture)
{
    if (ppTexture)
        *ppTexture = nullptr;

    if (!pDevice || !ppTexture)
        return D3DERR_INVALIDCALL;

    DdsImage img;
    UINT startMip = 0, levels = 0, w = 0, h = 0, d = 1;
    D3DFORMAT fmt = D3DFMT_UNKNOWN;

    if (!ResolveRequest(pSrcData, srcDataSize, "D3DXCreateTextureFromFileInMemoryEx",
                        width, height, 1, mipLevels, format,
                        img, startMip, levels, fmt, w, h, d))
    {
        return D3DERR_INVALIDCALL;
    }

    FillImageInfo(img, pSrcInfo);

    const bool staged = NeedsStaging(pool, usage);
    const D3DPOOL fillPool = staged ? D3DPOOL_SYSTEMMEM : pool;
    const DWORD   fillUsage = staged ? 0 : usage;

    IDirect3DTexture9* fill = nullptr;
    HRESULT hr = pDevice->CreateTexture(w, h, levels, fillUsage, fmt, fillPool, &fill, nullptr);
    if (FAILED(hr))
    {
        r3dOutToLog("D3DX compat: CreateTexture(%ux%u, %u levels, fmt %u) failed: 0x%08lX\n",
                    w, h, levels, (unsigned)fmt, hr);
        return hr;
    }

    // Walk to the first mip we want, then upload the rest of the chain.
    UINT sw = img.width, sh = img.height, sd = img.depth;
    const BYTE* src = SkipMips(img, img.pixels, sw, sh, sd, startMip);

    // Guard against a truncated file before touching a single byte of it.
    if (static_cast<size_t>(src - img.pixels) + MipChainBytes(img, sw, sh, 1, levels) > img.pixelSize)
    {
        r3dOutToLog("D3DX compat: D3DXCreateTextureFromFileInMemoryEx -- DDS is truncated; "
                    "%u bytes of surface data present, more required\n", (unsigned)img.pixelSize);
        fill->Release();
        return D3DERR_INVALIDCALL;
    }

    for (UINT level = 0; level < levels; ++level)
    {
        D3DLOCKED_RECT rect;
        hr = fill->LockRect(level, &rect, nullptr, 0);
        if (FAILED(hr))
        {
            fill->Release();
            return hr;
        }

        const bool ok = CopySurface(img.format, src, fmt,
                                    static_cast<BYTE*>(rect.pBits), rect.Pitch, sw, sh);

        fill->UnlockRect(level);

        if (!ok)
        {
            r3dOutToLog("D3DX compat: no conversion from format %u to %u\n",
                        (unsigned)img.format, (unsigned)fmt);
            fill->Release();
            return D3DERR_INVALIDCALL;
        }

        src += SurfaceBytes(img.format, sw, sh);
        sw = NextMip(sw);
        sh = NextMip(sh);
    }

    if (!staged)
    {
        *ppTexture = fill;
        return S_OK;
    }

    IDirect3DTexture9* target = nullptr;
    hr = pDevice->CreateTexture(w, h, levels, usage, fmt, pool, &target, nullptr);
    if (SUCCEEDED(hr))
        hr = pDevice->UpdateTexture(fill, target);

    fill->Release();

    if (FAILED(hr))
    {
        if (target)
            target->Release();
        return hr;
    }

    *ppTexture = target;
    return S_OK;
}

HRESULT D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile,
                                     UINT width, UINT height, UINT mipLevels, DWORD usage,
                                     D3DFORMAT format, D3DPOOL pool, DWORD filter,
                                     DWORD mipFilter, D3DCOLOR colorKey,
                                     D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette,
                                     LPDIRECT3DTEXTURE9* ppTexture)
{
    if (ppTexture)
        *ppTexture = nullptr;

    std::vector<BYTE> bytes;
    if (!pSrcFile || !ReadWholeFile(pSrcFile, bytes))
        return D3DERR_INVALIDCALL;

    return D3DXCreateTextureFromFileInMemoryEx(pDevice, &bytes[0], (UINT)bytes.size(),
                                               width, height, mipLevels, usage, format,
                                               pool, filter, mipFilter, colorKey,
                                               pSrcInfo, pPalette, ppTexture);
}

HRESULT D3DXCreateTextureFromFileA(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile,
                                   LPDIRECT3DTEXTURE9* ppTexture)
{
    return D3DXCreateTextureFromFileExA(pDevice, pSrcFile,
                                        D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                        D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                        D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0,
                                        nullptr, nullptr, ppTexture);
}

//////////////////////////////////////////////////////////////////////////
// Cube textures
//
// DDS stores a cube map as six complete mip chains, one per face, in +X -X +Y -Y +Z -Z
// order -- which is D3DCUBEMAP_FACE_POSITIVE_X..NEGATIVE_Z, so the index doubles as
// the face enum.
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXCreateCubeTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData,
                                                UINT srcDataSize, UINT size, UINT mipLevels,
                                                DWORD usage, D3DFORMAT format, D3DPOOL pool,
                                                DWORD /*filter*/, DWORD /*mipFilter*/,
                                                D3DCOLOR /*colorKey*/, D3DXIMAGE_INFO* pSrcInfo,
                                                PALETTEENTRY* /*pPalette*/,
                                                LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    if (ppCubeTexture)
        *ppCubeTexture = nullptr;

    if (!pDevice || !ppCubeTexture)
        return D3DERR_INVALIDCALL;

    DdsImage img;
    UINT startMip = 0, levels = 0, w = 0, h = 0, d = 1;
    D3DFORMAT fmt = D3DFMT_UNKNOWN;

    if (!ResolveRequest(pSrcData, srcDataSize, "D3DXCreateCubeTextureFromFileInMemoryEx",
                        size, size, 1, mipLevels, format,
                        img, startMip, levels, fmt, w, h, d))
    {
        return D3DERR_INVALIDCALL;
    }

    if (!img.isCube)
    {
        r3dOutToLog("D3DX compat: D3DXCreateCubeTextureFromFileInMemoryEx -- DDS is not a "
                    "cube map (caps2 has no DDSCAPS2_CUBEMAP)\n");
        return D3DERR_INVALIDCALL;
    }

    FillImageInfo(img, pSrcInfo);

    const size_t faceBytes = MipChainBytes(img, img.width, img.height, 1, img.mipLevels);

    if (faceBytes * 6 > img.pixelSize)
    {
        r3dOutToLog("D3DX compat: D3DXCreateCubeTextureFromFileInMemoryEx -- DDS is "
                    "truncated; six faces need %u bytes, %u present\n",
                    (unsigned)(faceBytes * 6), (unsigned)img.pixelSize);
        return D3DERR_INVALIDCALL;
    }

    const bool staged = NeedsStaging(pool, usage);

    IDirect3DCubeTexture9* fill = nullptr;
    HRESULT hr = pDevice->CreateCubeTexture(w, levels, staged ? 0 : usage, fmt,
                                            staged ? D3DPOOL_SYSTEMMEM : pool,
                                            &fill, nullptr);
    if (FAILED(hr))
    {
        r3dOutToLog("D3DX compat: CreateCubeTexture(%u, %u levels, fmt %u) failed: 0x%08lX\n",
                    w, levels, (unsigned)fmt, hr);
        return hr;
    }

    for (UINT face = 0; face < 6; ++face)
    {
        UINT sw = img.width, sh = img.height, sd = 1;
        const BYTE* src = SkipMips(img, img.pixels + faceBytes * face, sw, sh, sd, startMip);

        for (UINT level = 0; level < levels; ++level)
        {
            D3DLOCKED_RECT rect;
            hr = fill->LockRect(static_cast<D3DCUBEMAP_FACES>(face), level, &rect, nullptr, 0);
            if (FAILED(hr))
            {
                fill->Release();
                return hr;
            }

            const bool ok = CopySurface(img.format, src, fmt,
                                        static_cast<BYTE*>(rect.pBits), rect.Pitch, sw, sh);

            fill->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face), level);

            if (!ok)
            {
                fill->Release();
                return D3DERR_INVALIDCALL;
            }

            src += SurfaceBytes(img.format, sw, sh);
            sw = NextMip(sw);
            sh = NextMip(sh);
        }
    }

    if (!staged)
    {
        *ppCubeTexture = fill;
        return S_OK;
    }

    IDirect3DCubeTexture9* target = nullptr;
    hr = pDevice->CreateCubeTexture(w, levels, usage, fmt, pool, &target, nullptr);
    if (SUCCEEDED(hr))
        hr = pDevice->UpdateTexture(fill, target);

    fill->Release();

    if (FAILED(hr))
    {
        if (target)
            target->Release();
        return hr;
    }

    *ppCubeTexture = target;
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// Volume textures
//
// A volume mip level is `depth` slices stored back to back, and the slice pitch in
// the locked box is not necessarily the tight one, so the copy walks slices.
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXCreateVolumeTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData,
                                                  UINT srcDataSize, UINT width, UINT height,
                                                  UINT depth, UINT mipLevels, DWORD usage,
                                                  D3DFORMAT format, D3DPOOL pool,
                                                  DWORD /*filter*/, DWORD /*mipFilter*/,
                                                  D3DCOLOR /*colorKey*/, D3DXIMAGE_INFO* pSrcInfo,
                                                  PALETTEENTRY* /*pPalette*/,
                                                  LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
    if (ppVolumeTexture)
        *ppVolumeTexture = nullptr;

    if (!pDevice || !ppVolumeTexture)
        return D3DERR_INVALIDCALL;

    DdsImage img;
    UINT startMip = 0, levels = 0, w = 0, h = 0, d = 1;
    D3DFORMAT fmt = D3DFMT_UNKNOWN;

    if (!ResolveRequest(pSrcData, srcDataSize, "D3DXCreateVolumeTextureFromFileInMemoryEx",
                        width, height, depth, mipLevels, format,
                        img, startMip, levels, fmt, w, h, d))
    {
        return D3DERR_INVALIDCALL;
    }

    FillImageInfo(img, pSrcInfo);

    // Volume textures cannot be created in SYSTEMMEM and copied with UpdateTexture on
    // every driver, and nothing here loads one into D3DPOOL_DEFAULT -- r3dTex.cpp
    // passes m_Pool, which is MANAGED, or SYSTEMMEM. Refuse rather than guess.
    if (NeedsStaging(pool, usage))
    {
        r3dOutToLog("D3DX compat: D3DXCreateVolumeTextureFromFileInMemoryEx -- "
                    "D3DPOOL_DEFAULT without D3DUSAGE_DYNAMIC is not supported\n");
        return D3DERR_INVALIDCALL;
    }

    IDirect3DVolumeTexture9* tex = nullptr;
    HRESULT hr = pDevice->CreateVolumeTexture(w, h, d, levels, usage, fmt, pool, &tex, nullptr);
    if (FAILED(hr))
    {
        r3dOutToLog("D3DX compat: CreateVolumeTexture(%ux%ux%u, %u levels, fmt %u) failed: 0x%08lX\n",
                    w, h, d, levels, (unsigned)fmt, hr);
        return hr;
    }

    UINT sw = img.width, sh = img.height, sd = img.depth;
    const BYTE* src = SkipMips(img, img.pixels, sw, sh, sd, startMip);

    if (static_cast<size_t>(src - img.pixels) + MipChainBytes(img, sw, sh, sd, levels) > img.pixelSize)
    {
        r3dOutToLog("D3DX compat: D3DXCreateVolumeTextureFromFileInMemoryEx -- DDS is truncated\n");
        tex->Release();
        return D3DERR_INVALIDCALL;
    }

    for (UINT level = 0; level < levels; ++level)
    {
        D3DLOCKED_BOX box;
        hr = tex->LockBox(level, &box, nullptr, 0);
        if (FAILED(hr))
        {
            tex->Release();
            return hr;
        }

        const UINT sliceBytes = SurfaceBytes(img.format, sw, sh);
        bool ok = true;

        for (UINT slice = 0; slice < sd && ok; ++slice)
        {
            ok = CopySurface(img.format, src + static_cast<size_t>(slice) * sliceBytes,
                             fmt,
                             static_cast<BYTE*>(box.pBits) + static_cast<size_t>(slice) * box.SlicePitch,
                             box.RowPitch, sw, sh);
        }

        tex->UnlockBox(level);

        if (!ok)
        {
            tex->Release();
            return D3DERR_INVALIDCALL;
        }

        src += static_cast<size_t>(sliceBytes) * sd;
        sw = NextMip(sw);
        sh = NextMip(sh);
        sd = NextMip(sd);
    }

    *ppVolumeTexture = tex;
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// Empty cube texture -- a straight pass-through to the device
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXCreateCubeTexture(LPDIRECT3DDEVICE9 pDevice, UINT size, UINT mipLevels,
                              DWORD usage, D3DFORMAT format, D3DPOOL pool,
                              LPDIRECT3DCUBETEXTURE9* ppCubeTexture)
{
    if (ppCubeTexture)
        *ppCubeTexture = nullptr;

    if (!pDevice || !ppCubeTexture)
        return D3DERR_INVALIDCALL;

    return pDevice->CreateCubeTexture(size,
                                      mipLevels == D3DX_DEFAULT ? 0 : mipLevels,
                                      usage, format, pool, ppCubeTexture, nullptr);
}

//////////////////////////////////////////////////////////////////////////
// Surface to surface
//
// Terrain3 uses this at runtime to move tiles between its scratch surfaces, always
// with NULL rects and D3DX_FILTER_NONE -- i.e. a straight full-surface copy. That is
// what this implements: same dimensions, converting the format if it has to. Scaling
// is not supported, for the same reason the loaders do not resample.
//////////////////////////////////////////////////////////////////////////

HRESULT D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9 pDestSurface,
                                   const PALETTEENTRY* /*pDestPalette*/, const RECT* pDestRect,
                                   LPDIRECT3DSURFACE9 pSrcSurface,
                                   const PALETTEENTRY* /*pSrcPalette*/, const RECT* pSrcRect,
                                   DWORD /*filter*/, D3DCOLOR /*colorKey*/)
{
    if (!pDestSurface || !pSrcSurface)
        return D3DERR_INVALIDCALL;

    if (pDestRect || pSrcRect)
    {
        r3dOutToLog("D3DX compat: D3DXLoadSurfaceFromSurface -- sub-rectangle copies are "
                    "not implemented; every call site in this codebase passes NULL\n");
        return E_NOTIMPL;
    }

    D3DSURFACE_DESC srcDesc, dstDesc;
    HRESULT hr = pSrcSurface->GetDesc(&srcDesc);
    if (FAILED(hr)) return hr;
    hr = pDestSurface->GetDesc(&dstDesc);
    if (FAILED(hr)) return hr;

    if (srcDesc.Width != dstDesc.Width || srcDesc.Height != dstDesc.Height)
    {
        r3dOutToLog("D3DX compat: D3DXLoadSurfaceFromSurface -- %ux%u to %ux%u would need "
                    "scaling, which is not implemented\n",
                    srcDesc.Width, srcDesc.Height, dstDesc.Width, dstDesc.Height);
        return E_NOTIMPL;
    }

    // A SYSTEMMEM source and a DEFAULT destination is the one pairing that cannot be
    // done by locking both, and it is exactly what the device offers UpdateSurface for.
    if (srcDesc.Pool == D3DPOOL_SYSTEMMEM && dstDesc.Pool == D3DPOOL_DEFAULT &&
        srcDesc.Format == dstDesc.Format)
    {
        IDirect3DDevice9* device = nullptr;
        if (SUCCEEDED(pDestSurface->GetDevice(&device)) && device)
        {
            hr = device->UpdateSurface(pSrcSurface, nullptr, pDestSurface, nullptr);
            device->Release();
            return hr;
        }
    }

    D3DLOCKED_RECT srcLock, dstLock;

    hr = pSrcSurface->LockRect(&srcLock, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr))
        return hr;

    hr = pDestSurface->LockRect(&dstLock, nullptr, 0);
    if (FAILED(hr))
    {
        pSrcSurface->UnlockRect();
        return hr;
    }

    bool ok = true;

    if (srcDesc.Format == dstDesc.Format)
    {
        // Honour both pitches: a locked surface's rows are not necessarily tight, and
        // the two surfaces need not agree.
        UINT rows, rowBytes;
        SurfaceRows(srcDesc.Format, srcDesc.Width, srcDesc.Height, rows, rowBytes);

        if (rowBytes == 0)
        {
            ok = false;
        }
        else
        {
            for (UINT y = 0; y < rows; ++y)
            {
                memcpy(static_cast<BYTE*>(dstLock.pBits) + static_cast<size_t>(y) * dstLock.Pitch,
                       static_cast<const BYTE*>(srcLock.pBits) + static_cast<size_t>(y) * srcLock.Pitch,
                       rowBytes);
            }
        }
    }
    else if (!IsBlockCompressed(srcDesc.Format) && !IsBlockCompressed(dstDesc.Format))
    {
        std::vector<Bgra> scratch(srcDesc.Width);

        for (UINT y = 0; y < srcDesc.Height && ok; ++y)
        {
            const BYTE* s = static_cast<const BYTE*>(srcLock.pBits) + static_cast<size_t>(y) * srcLock.Pitch;
            BYTE*       dm = static_cast<BYTE*>(dstLock.pBits)      + static_cast<size_t>(y) * dstLock.Pitch;

            ok = DecodeRow(srcDesc.Format, s, &scratch[0], srcDesc.Width) &&
                 EncodeRow(dstDesc.Format, &scratch[0], dm, srcDesc.Width);
        }
    }
    else
    {
        ok = false;
    }

    pDestSurface->UnlockRect();
    pSrcSurface->UnlockRect();

    if (!ok)
    {
        r3dOutToLog("D3DX compat: D3DXLoadSurfaceFromSurface -- no conversion from format "
                    "%u to %u\n", (unsigned)srcDesc.Format, (unsigned)dstDesc.Format);
        return E_NOTIMPL;
    }

    return S_OK;
}
