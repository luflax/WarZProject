#ifndef	TL_TFIXEDARRAY_H
#define	TL_TFIXEDARRAY_H

// [PORT] Declared HERE, in the global namespace, rather than at block scope inside
// r3dTL::...::ReportOutOfBounds where it used to live. A function declaration inside a
// function body names the nearest ENCLOSING NAMESPACE, so within namespace r3dTL it
// declared r3dTL::r3dGetMainModuleBaseAddress -- a function nothing ever defines. The
// real one is global, in r3dDebug.cpp, and the call came out undefined at link time.
void* r3dGetMainModuleBaseAddress();

namespace r3dTL
{

// PORT NOTE: MSVC inline _asm replaced with a portable return-address intrinsic.
// The original captured EIP via "call $+5 / pop eax", which is 32-bit x86 MSVC only.
#ifndef R3D_RETURN_ADDRESS
  #if defined(_MSC_VER)
    #include <intrin.h>
    #define R3D_RETURN_ADDRESS() (_ReturnAddress())
  #else
    #define R3D_RETURN_ADDRESS() (__builtin_return_address(0))
  #endif
#endif

#define R3D_TFIXEDARRAY_CHECKBOUNDS( idx_ )							\
	if( (idx_) >= N )												\
	{																\
		void* EIPVal;												\
		EIPVal = R3D_RETURN_ADDRESS();	\
		ReportOutOfBounds( idx_, __FUNCTION__, EIPVal );			\
	}


	template < typename T, uint32_t N>
	class TFixedArray
	{
	public:
		static const uint32_t COUNT = N;

	public:
		TFixedArray();

	public:
		T& operator [] ( uint32_t idx );
		const T& operator [] ( uint32_t idx ) const;

		T* operator + ( uint32_t idx );
		const T* operator + ( uint32_t idx ) const;

		void Move( uint32_t targIdx, uint32_t srcIdx, uint32_t count );

		int Count() const;

	private:
		void ReportOutOfBounds( int idx, const char* func, void* eip ) const;

	private:
		T			mElems[ N ];
	};

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	TFixedArray< T, N > :: TFixedArray()
	{
		for( uint32_t i = 0, e = N; i < e; i ++ )
		{
			mElems[ i ] = T();
		}
	}

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	T&
	TFixedArray< T, N > :: operator [] ( uint32_t idx )
	{
		R3D_TFIXEDARRAY_CHECKBOUNDS(idx);
		return mElems[ idx ];
	}

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	const T&
	TFixedArray< T, N > :: operator [] ( uint32_t idx ) const
	{
		R3D_TFIXEDARRAY_CHECKBOUNDS(idx);
		return mElems[ idx ];
	}

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	T* 
	TFixedArray< T, N > :: operator + ( uint32_t idx )
	{
		R3D_TFIXEDARRAY_CHECKBOUNDS(idx);
		return mElems + idx ;
	}

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	const T*
	TFixedArray< T, N > :: operator + ( uint32_t idx ) const
	{
		R3D_TFIXEDARRAY_CHECKBOUNDS(idx);
		return mElems + idx ;
	}

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	void
	TFixedArray< T, N > :: Move( uint32_t targIdx, uint32_t srcIdx, uint32_t count )
	{
		for( uint32_t i = 0, e = count, t = targIdx, s = srcIdx ; i < e ; i ++, t ++, s ++ )
		{
			mElems[ t ] = mElems[ s ] ;
		}
	}

	//------------------------------------------------------------------------

	template < typename T, uint32_t N >
	int TFixedArray< T, N > :: Count() const
	{
		return COUNT;
	}

	//------------------------------------------------------------------------

	template< typename T, uint32_t N >
	R3D_NO_INLINE
	void TFixedArray< T, N > :: ReportOutOfBounds( int idx, const char* func, void* eip ) const
	{
		void* base = ::r3dGetMainModuleBaseAddress();

		r3dOutToLog( "ERROR: Bounds check failed (%d of %d) in %s (EIP=0x%X, BASE=0x%X)\n", idx, N, func, eip, base );
#ifndef FINAL_BUILD
		r3d_actual_assert( "idx < N", __FILE__, __LINE__, 0 );
#endif
	}


}

#endif