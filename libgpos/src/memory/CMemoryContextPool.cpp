//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CMemoryContextPool.cpp
//
//	@doc:
//		CMemoryPoolTracker implementation that uses PostgreSQL memory
//		contexts.
//
//---------------------------------------------------------------------------

#include "gpos/memory/CMemoryPool.h"

#include "gpos/memory/CMemoryContextPool.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CMemoryContextPool::~CMemoryContextPool
//
//	@doc:
//		Dtor.
//
//---------------------------------------------------------------------------
CMemoryContextPool::~CMemoryContextPool()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolTracker::Allocate
//
//	@doc:
//		Allocate memory.
//
//---------------------------------------------------------------------------
void *
CMemoryContextPool::Allocate
(
 const ULONG bytes,
 const CHAR*,
 const ULONG
 )
{
	void* ptr = m_alloc(bytes);
	return ptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolTracker::Free
//
//	@doc:
//		Free memory.
//
//---------------------------------------------------------------------------
void
CMemoryContextPool::Free
(
 void *ptr
 )
{
	m_free(ptr);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryPoolTracker::TearDown
//
//	@doc:
//		Prepare the memory pool to be deleted;
// 		this function is called only once so locking is not required;
//
//---------------------------------------------------------------------------
void
CMemoryContextPool::TearDown()
{}

// EOF
