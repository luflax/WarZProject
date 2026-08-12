//=========================================================================
//	Module: PhysXRepXHelpers.h
//	Copyright (C) 2011.
//=========================================================================

#pragma once

//////////////////////////////////////////////////////////////////////////

#include "common/PxIO.h"

//////////////////////////////////////////////////////////////////////////

namespace physx
{
	namespace repx
	{
		class RepXCollection;   // [PORT] PhysX 3.x only; unused after the PhysX 4 port
	}
}
//////////////////////////////////////////////////////////////////////////

// [PORT] PhysX 4 removed RepX. XML deserialization is now
// PxSerialization::createCollectionFromXml, which yields a PxCollection.
// The allocator argument is no longer needed -- PhysX takes it from PxPhysics --
// but is kept so call sites compile unchanged.
physx::PxCollection* loadCollection(const char* inPath, PxAllocatorCallback& inCallback);

//////////////////////////////////////////////////////////////////////////

class PhysxUserFileReadStream : public PxInputStream
{
public:
	PhysxUserFileReadStream(const char* filename);
	virtual                     ~PhysxUserFileReadStream();
	virtual     PxU32			read(void* buffer, PxU32 size);

	r3dFile*    fpr;	// reading stream can be used from archives
};

//////////////////////////////////////////////////////////////////////////

class PhysxUserMemoryReadStream: public PxInputStream
{
public:
	PhysxUserMemoryReadStream(PxU8* data, PxU32 length);

	PxU32		read(void* dest, PxU32 count);
	PxU32		getLength() const;
	void		seek(PxU32 pos);
	PxU32		tell() const;

private:
	PxU32		mSize;
	const PxU8*	mData;
	PxU32		mPos;
};

//////////////////////////////////////////////////////////////////////////

class PhysxUserFileWriteStream: public PxOutputStream
{
public:
	PhysxUserFileWriteStream(const char *fileName);
	virtual				~PhysxUserFileWriteStream();
	virtual		PxU32	write(const void* src, PxU32 count);
	FILE*       fpw;	// direct writing stream
};

//////////////////////////////////////////////////////////////////////////

class PhysxUserMemoryWriteStream: public PxOutputStream
{
public:
				 PhysxUserMemoryWriteStream();
	virtual		~PhysxUserMemoryWriteStream();

	PxU32		write(const void* src, PxU32 count);

	PxU32		getSize()	const	{ return mSize; }
	PxU8*		getData()	const	{ return mData; }
private:
	PxU8*		mData;
	PxU32		mSize;
	PxU32		mCapacity;
};
