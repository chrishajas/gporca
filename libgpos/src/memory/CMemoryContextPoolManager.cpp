//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryContextPoolManager.cpp
//
//	@doc:
//		CMemoryContextPoolManager implementation that uses PostgreSQL
//		memory contexts.
//
//---------------------------------------------------------------------------

#include "gpos/memory/CMemoryContextPool.h"
#include "gpos/memory/CMemoryContextPoolManager.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CMemoryContextPoolManager::CMemoryContextPoolManager
//
//	@doc:
//		Ctor.
//
//---------------------------------------------------------------------------
CMemoryContextPoolManager::CMemoryContextPoolManager(void* (*alloc) (SIZE_T), void (*free_func) (void*))
{
	m_alloc = alloc;
	m_free = free_func;
	m_global_memory_pool = Create(EatTracker);
}


//---------------------------------------------------------------------------
//	@function:
//		CMemoryContextPoolManager::~CMemoryContextPoolManager
//
//	@doc:
//		Dtor.
//
//---------------------------------------------------------------------------
CMemoryContextPoolManager::~CMemoryContextPoolManager()
{
}

CMemoryPool *
CMemoryContextPoolManager::Create(CMemoryPoolManager::AllocType)
{
	/*
	 * We use the same implementation for all "kinds" of pools.
	 * 'alloc_type' is ignored.
	 */
	return new CMemoryContextPool(m_alloc, m_free);
}


void
CMemoryContextPoolManager::Destroy(CMemoryPool *mp)
{
	mp->TearDown();
	delete (CMemoryContextPool *) mp;
}

// EOF

