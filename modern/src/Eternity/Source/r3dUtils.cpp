
#include "r3dPCH.h"
#include "r3d.h"

#include "r3dConst.h"
#include "r3dUtils.h"

//C
//
// r3dImageLoader
//
//

#define	R3D_BMP_SIGNATURE	'MB'

#pragma pack(push)
#pragma pack(1)
struct R3D_BMP_HEADER
{
	WORD		Signature;		// 'BM'
	DWORD		FileSize;
	DWORD		Unused;
	DWORD		OffsetOfData;

	DWORD		StructSize;		// 40
	DWORD		Width;
	DWORD		Height;
	WORD		NumPlanes;
	WORD		BitsPerPixel;
	DWORD		Compression;
	DWORD		ImageSize;
	DWORD		xPelsPerMeter;
	DWORD		yPelsPerMeter;
	DWORD		ColorsUsed;
	DWORD		ImportantColors;
};

struct R3D_BMP_RGBQUAD
{
  BYTE		b, g, r, u;
};

#pragma pack(pop)

r3dImageLoader::r3dImageLoader()
{
  ClrData = NULL;
}

r3dImageLoader::~r3dImageLoader()
{
  if(ClrData)
    delete[] ClrData;
}

int r3dImageLoader::Load(r3dFile *f)
{
	char		*RawData;
	R3D_BMP_HEADER 	hdr;

  if(!f->IsValid())
    return 0;

  fread(&hdr, sizeof(hdr), 1, f);
  if(hdr.Signature != R3D_BMP_SIGNATURE) {
    r3dArtBug("ImageLoader: [%s] - isn't BMP\n", f->GetFileName());
    return 0;
  }

  if(hdr.Compression) {
    r3dArtBug("ImageLoader: [%s] - compressed BMP is not supported\n", f->Location.FileName);
    return 0;
  }

  Width  = hdr.Width;
  Height = hdr.Height;

	R3D_BMP_RGBQUAD	OriginalPal[256];
	int		bpp  = (hdr.BitsPerPixel / 8);
	char		*src;
	char		*dst;
	int		x, y;

  if(hdr.BitsPerPixel == 8)
    fread(OriginalPal, 1024, 1, f);

  // read bitmap upside down
  RawData = game_new char[Width * Height * bpp];
  src     = RawData + (Height - 1) * hdr.Width * bpp;
  for(y = 0; y < Height; y++) {
    fread(src, hdr.Width * bpp, 1, f);
    src -= hdr.Width * bpp;
  }

  switch(hdr.BitsPerPixel) {
    // convert 8bpp to 24bpp
    case 8:
      ClrData = game_new char[Width * Height * 3];

      src = RawData;
      dst = ClrData;
      for(y = 0; y < Height; y++) {
	for(x = 0; x < Width; x++) {
	  *dst++ = OriginalPal[*src].b;
	  *dst++ = OriginalPal[*src].g;
	  *dst++ = OriginalPal[*src].r;
	  src++;
	}
      }
      delete[] RawData;
      break;

    case 24:
      // switch arrays
      ClrData = RawData;
      break;
  }

  return 1;
}


//C
//
// High Performance Counter
//
//
// PORT NOTE: MSVC inline _asm replaced with the __rdtsc intrinsic.
// The original depended on two MSVC-only behaviours: `this` arriving in ecx under
// thiscall, and a value being returned in eax with no return statement (UB).
// The arithmetic below is a faithful translation of the original instructions.
#if defined(_MSC_VER)
  #include <intrin.h>
#else
  #include <x86intrin.h>
#endif

void r3dPerfCounter::Start()
{
  const unsigned long long tsc = __rdtsc();
  perf_StartHi = (long)(unsigned long)(tsc >> 32);
  perf_StartLo = (long)(unsigned long)(tsc & 0xFFFFFFFFull);
  perf_Start   = (long)(((unsigned long)(tsc & 0xFFFFFFFFull)) >> 4);
}

long r3dPerfCounter::GetDiffTicks()
{
  const unsigned long long tsc = __rdtsc();

  const unsigned long hi = (unsigned long)(tsc >> 32);
  const unsigned long lo = (unsigned long)(tsc & 0xFFFFFFFFull);

  // sub edx, perf_StartHi ; shr eax, 4 ; shl edx, 28 ; or eax, edx ; sub eax, perf_Start
  const unsigned long dhi = hi - (unsigned long)perf_StartHi;
  unsigned long       eax = lo >> 4;
  eax |= (dhi << 28);

  return (long)(eax - (unsigned long)perf_Start);
}

float r3dPerfCounter::GetDiff()
{
  return GetDiffTicks() / 50000000.0f;
}

void r3dPerfCounter::GetCurrent(long &Hi, long &Lo)
{
  __asm rdtsc
  __asm mov Hi, edx
  __asm mov Lo, eax
}

//C
//
// r3dFunctionStack
//
//

static	char		*FnStackNames[256];
static	int		FnStackTop = 0;

r3dFunctionStack::r3dFunctionStack(char *Name)
{
  FnStackNames[FnStackTop++] = Name;
}

r3dFunctionStack::~r3dFunctionStack()
{
  FnStackTop--;
}



#define MAX_VA_STRING	4096
#define	MAX_VA_BUFFERS	16

//---------------------------------------------------------------------------------------------
// Name: const char * Va( const char * str, ... )
// Desc:  
//---------------------------------------------------------------------------------------------
const char * Va( const char * str, ... )
{
	static char		_buffer_string[ MAX_VA_BUFFERS ][ MAX_VA_STRING ];
	static int		_index_string = 0;
	char			* buffer;
	va_list			args;

	buffer = _buffer_string[ _index_string ];

	if ( ++_index_string >= MAX_VA_BUFFERS )
		_index_string = 0;

	va_start( args, str );
	vsnprintf( buffer, MAX_VA_STRING, str, args );
	va_end( args );

	return buffer;
}

void DumbWCHAR( WCHAR* dest, const char* source )
{
	char* cdes = (char*)dest ;

	size_t len = strlen( source ) ;

	for( size_t i = 0, e = len ; i < e; i ++ )
	{
		*cdes ++ = *source ++ ;
		*cdes ++ = 0 ;
	}

	*cdes ++ = 0 ;
	*cdes ++ = 0 ;
}

void DumbANSI( char* dest, const WCHAR* source )
{
	size_t len = wcslen( source ) ;

	for( size_t i = 0, e = len ; i < e; i ++ )
	{
		*dest ++ = (char)*source ++ ;
	}

	*dest ++ = 0 ;
}