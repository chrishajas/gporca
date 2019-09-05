//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryContextPool.h
//
//	@doc:
//		Bridge between PostgreSQL memory contexts and GPORCA memory pools.
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CMemoryContextPool_H
#define GPDXL_CMemoryContextPool_H

#include "gpos/base.h"

#include "gpos/memory/CMemoryPool.h"

namespace gpos
{
	// Memory pool that maps to a Postgres MemoryContext.
	class CMemoryContextPool : public CMemoryPool
	{
	private:

		void* (*m_alloc) (SIZE_T);
		void (*m_free) (void*);

	public:

		// ctor
		CMemoryContextPool
				(
				void* (*alloc)(SIZE_T),
				void (*free_func)(void*)
				)
				:
				m_alloc(alloc),
				m_free(free_func)
		{}

		// dtor
		virtual
		~CMemoryContextPool();

		// allocate memory
		void *Allocate
		(
		 const ULONG bytes,
		 const CHAR *file,
		 const ULONG line
		 );

		// free memory
		void Free(void *ptr);

		// prepare the memory pool to be deleted
		void TearDown();

		// return total allocated size
		virtual
		ULLONG TotalAllocatedSize() const
		{
			//return m_memory_pool_statistics.TotalAllocatedSize();
			return 0;
		}

#ifdef GPOS_DEBUG

		// check if the memory pool keeps track of live objects
		virtual
		BOOL SupportsLiveObjectWalk() const
		{
			return false;
		}

		// walk the live objects
		virtual
		void WalkLiveObjects(gpos::IMemoryVisitor *visitor)
		{
		}

		// check if statistics tracking is supported
		virtual
		BOOL SupportsStatistics() const
		{
			return false;
		}

		// return the current statistics
		virtual
		void UpdateStatistics(CMemoryPoolStatistics &memory_pool_statistics)
		{
		}

#endif // GPOS_DEBUG

	};
}

#endif // !GPDXL_CMemoryContextPool_H

// EOF
