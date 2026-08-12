#ifndef	__R3DSYS_WIN_H
#define	__R3DSYS_WIN_H


// PORT NOTE: MSVC inline _asm replaced with SSE intrinsics.
// The original x87/SSE assembly is unusable outside 32-bit MSVC -- MSVC itself
// rejects __asm entirely when targeting x64, so this blocked any 64-bit build,
// not just cross-compilation. Intrinsics are portable across MSVC/GCC/Clang and
// x86/x64 and generate the same instructions.
#include <xmmintrin.h>
#include <cmath>

#include "r3dTypedefs.h"

#define extern_nspace(nspace, var)  namespace nspace { extern var; };

#define INVALID_INDEX			(-1)

//
// Miscellaneous helper functions
//
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define PURE_DELETE(p)       { delete (p); }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

#define R3D_NOCLASSCOPY(xx)		\
	xx(const xx &);			\
	xx &operator= (const xx &);


//
// various templates
//
template<class T>
__forceinline  int R3D_SIGN(const T a) 	{ return (a < 0) ? -1 : (a > 0) ? 1 : 0; }

template<class T>
__forceinline void R3D_SWAP(T& one, T& two) { T temp; temp = one; one = two; two = temp; }

template<class T>
__forceinline T R3D_MAX(const T a, const T b) { return a > b ? a : b; }

template<class T>
__forceinline T R3D_MIN(const T a, const T b)	{ return a < b ? a : b; }

template<class T>
__forceinline T R3D_ABS(const T a) 		{ return a >= 0 ? a : -a; }

template<class T>
__forceinline T R3D_CLAMP(const T val, const T min, const T max) 
{
  return ((val < min) ? min : (val > max) ? max : val);
}
template <class T> 
__forceinline T R3D_LERP( T from, T to, float weight )
{
	return T( from + weight * ( to - from ) );
}
//
// linked list stuff
//
template <class T>
int LList_Create(T **base)
{
  *base = NULL;
  return 1;
}

template <class T>
int LList_Insert(T **base, T *what)
{
	T	*tmp;

  // check if it's already here..
  for(tmp=*base; tmp; tmp=tmp->pNext)
    if(tmp == what)
      return 0;

  what->pNext = *base;
  *base      = what;
  return 1;
};

template <class T>
int LList_InsertLast(T **base, T *what)
{
	T	*tmp;
  // traverse to last element and add there..
  for(tmp=*base; tmp && tmp->pNext; tmp=tmp->pNext)
    if(tmp == what)
      return 0;

  if(*base) tmp->pNext  = what;
  else      *base      = what;
  return 1;
};

template <class T>
int LList_Remove(T **base, T *what)
{
	T	*tmp;

  if(!*base)
    return 0;

  if(*base == what) {
    *base = what->pNext;
    return 1;
  }
  // scan thru list and see if that object can be removed
  for(tmp=*base; tmp && tmp->pNext; tmp=tmp->pNext) {
    if(tmp->pNext == what) {
      tmp->pNext = what->pNext;
      return 1;
    }
  }

  return 0;
}

template <class T>
int LList_Destroy(T **base)
{
	T	*tmp, *tmp2;
  for(tmp = *base; tmp; tmp=tmp2) {
    tmp2 = tmp->pNext;
    delete tmp;
  }
  *base = NULL;
  return 1;
}


//----------------------------------------------------------------------------
//	General math functions
//----------------------------------------------------------------------------
inline FLOAT r3dExp( FLOAT Value ) { return expf(Value); }
inline FLOAT r3dLoge( FLOAT Value ) {	return logf(Value); }
inline FLOAT r3dFmod( FLOAT Y, FLOAT X ) { return fmodf(Y,X); }
inline FLOAT r3dSin( FLOAT Value ) { return sinf(Value); }
inline FLOAT r3dAsin( FLOAT Value ) { return asinf(Value); }
inline FLOAT r3dCos( FLOAT Value ) { return cosf(Value); }
inline FLOAT r3dAcos( FLOAT Value ) { return acosf(Value); }
inline FLOAT r3dTan( FLOAT Value ) { return tanf(Value); }
inline FLOAT r3dAtan( FLOAT Value ) { return atanf(Value); }
inline FLOAT r3dAtan2( FLOAT Y, FLOAT X ) { return atan2f(Y,X); }
inline FLOAT r3dPow( FLOAT A, FLOAT B ) { return powf(A,B); }




inline int 	r3dFloatToInt_2(float _fvar)
{
  _fvar += (1l<<23l);
  return *((int *)&_fvar) & 0x7fffffl;
}

__forceinline int 	r3dFloatToInt(float _fvar)
{
  // fld/fistp used the current x87 rounding mode (round-to-nearest by default).
  // _mm_cvtss_si32 uses the MXCSR rounding mode, likewise round-to-nearest.
  return _mm_cvtss_si32(_mm_set_ss(_fvar));
}


/*
inline INT r3dFloatToInt( FLOAT F )
{
	__asm cvtss2si eax,[F]
	// return value in eax.
}
*/

inline INT r3dFloor( FLOAT F )
{
  // The original switched MXCSR to round-toward-negative-infinity, ran cvtss2si,
  // then restored it -- and relied on the value being left in eax with no return
  // statement, which is undefined behaviour. Mutating global rounding state is
  // also unsafe with any concurrent FP work.
  return (INT)std::floor( F );
}

//
// MSM: Fast float inverse square root using SSE.
// Accurate to within 1 LSB.
//
inline FLOAT r3dInvSqrt( FLOAT F )
{
  // rsqrtss (12-bit estimate) + one Newton-Raphson step:
  //   X1 = 0.5 * X0 * (3 - (Y * X0) * X0)
  const __m128 y  = _mm_set_ss( F );
  const __m128 x0 = _mm_rsqrt_ss( y );

  const __m128 yx0   = _mm_mul_ss( y, x0 );
  const __m128 yx0x0 = _mm_mul_ss( yx0, x0 );
  const __m128 half  = _mm_mul_ss( x0, _mm_set_ss( 0.5f ) );
  const __m128 three = _mm_sub_ss( _mm_set_ss( 3.0f ), yx0x0 );

  return _mm_cvtss_f32( _mm_mul_ss( three, half ) );
}

//
// MSM: Fast float square root using SSE.
// Accurate to within 1 LSB.
//
inline FLOAT r3dSqrt( FLOAT F )
{
  // sqrt(f) = f * (1/sqrt(f)), with the input-is-zero case masked to zero
  // exactly as the original andps did.
  const __m128 y  = _mm_set_ss( F );
  const __m128 x0 = _mm_rsqrt_ss( y );

  const __m128 yx0   = _mm_mul_ss( y, x0 );
  const __m128 yx0x0 = _mm_mul_ss( yx0, x0 );
  const __m128 half  = _mm_mul_ss( x0, _mm_set_ss( 0.5f ) );
  const __m128 three = _mm_sub_ss( _mm_set_ss( 3.0f ), yx0x0 );

  __m128 r = _mm_mul_ss( three, half );
  r = _mm_mul_ss( r, y );
  r = _mm_and_ps( r, _mm_cmpneq_ss( _mm_set_ss( 0.0f ), y ) );

  return _mm_cvtss_f32( r );
}


//----------------------------------------------------------------------------
//	Time functions.
//----------------------------------------------------------------------------


	// our random generator functions
	unsigned long	u_random(unsigned long seed);
	void	      	u_srand(unsigned long seed);
	void		u_thread_rand_init();
	void		u_thread_rand_close();
	float 		u_GetRandom();
	float 		u_GetRandom(float r1, float r2);
	// helper for random numbers initialization in thread
	struct r3dRandInitInTread {
		r3dRandInitInTread() {
			u_thread_rand_init();
			u_srand(GetTickCount());
		}
		~r3dRandInitInTread() {
			u_thread_rand_close();
		}
	};

//----------------------------------------------------------------------------
//	Memory functions.
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//	Callbacks
//----------------------------------------------------------------------------

extern void (*OnDblClick)();
extern void (*OnDrawClipboardCallback)(WPARAM wParam, LPARAM lParam);

#endif	//__R3DSYS_WIN_H
