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
CMemoryContextPoolManager::CMemoryContextPoolManager(void* (*alloc) (SIZE_T), void (*free_func) (void*), CMemoryPool* internal)
	:
	CMemoryPoolManager(internal),
	m_alloc(alloc),
	m_free(free_func)
{}


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
CMemoryContextPoolManager::New
(
 AllocType
 )
{
	return GPOS_NEW(m_internal_memory_pool) CMemoryContextPool(m_alloc, m_free);
}

// EOF

