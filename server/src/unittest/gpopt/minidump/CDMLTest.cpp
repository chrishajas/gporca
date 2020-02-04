//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CDMLTest.cpp
//
//	@doc:
//		Test for optimizing DML queries
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/exception.h"
#include "gpopt/minidump/CMinidumperUtils.h"

#include "unittest/gpopt/CTestUtils.h"

#include "unittest/gpopt/minidump/CDMLTest.h"

using namespace gpopt;

ULONG CDMLTest::m_ulDMLTestCounter = 0;  // start from first test

// minidump files
const CHAR *rgszDMLFileNames[] =
	{
	"../data/dxl/minidump/DML-UnionAll-With-Universal-Child.mdp",
	"../data/dxl/minidump/DML-With-MasterOnlyTable-1.mdp",
	"../data/dxl/minidump/DML-With-HJ-And-UniversalChild.mdp",
	"../data/dxl/minidump/DML-With-Join-With-Universal-Child.mdp",
	"../data/dxl/minidump/DML-With-CorrelatedNLJ-With-Universal-Child.mdp",
	"../data/dxl/minidump/DML-Function-With-SQL-Access.mdp",
	};

//---------------------------------------------------------------------------
//	@function:
//		CDMLTest::EresUnittest
//
//	@doc:
//		Unittest for expressions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDMLTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(EresUnittest_RunTests),
		};

	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	// reset metadata cache
	CMDCache::Reset();

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CDMLTest::EresUnittest_RunTests
//
//	@doc:
//		Run all Minidump-based tests with plan matching
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDMLTest::EresUnittest_RunTests()
{
	return CTestUtils::EresUnittest_RunTests
						(
						rgszDMLFileNames,
						&m_ulDMLTestCounter,
						GPOS_ARRAY_SIZE(rgszDMLFileNames)
						);
}

// EOF
