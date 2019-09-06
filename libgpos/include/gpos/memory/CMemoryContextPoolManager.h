//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal, Inc.
//
//	@filename:
//		CMemoryContextPoolManager.h
//
//	@doc:
//		Bridge between PostgreSQL memory contexts and GPORCA memory pools.
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CMemoryContextPoolManager_H
#define GPDXL_CMemoryContextPoolManager_H

#include "gpos/base.h"

#include "gpos/memory/CMemoryPoolManager.h"

namespace gpos
{
	// memory pool manager that uses GPDB memory contexts
	class CMemoryContextPoolManager : public CMemoryPoolManager
	{
	private:
		void* (*m_alloc) (SIZE_T);
		void (*m_free) (void*);
	protected:

		// dtor
		virtual
		~CMemoryContextPoolManager();

		// private no copy ctor
		CMemoryContextPoolManager(const CMemoryContextPoolManager&);

	public:

		// ctor
		CMemoryContextPoolManager(void* (*) (SIZE_T), void (*) (void*));

		// create new memory pool
		CMemoryPool *Create
		(
		 CMemoryPoolManager::AllocType alloc_type
		 );

		// release memory pool
		void Destroy(CMemoryPool *);

		// delete memory pools and release manager
		void Shutdown()
		{
		}

	};
}

#endif // !GPDXL_CMemoryContextPoolManager_H

// EOF
