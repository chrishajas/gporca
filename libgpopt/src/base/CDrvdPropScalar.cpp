//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDrvdPropScalar.cpp
//
//	@doc:
//		Scalar derived properties
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CScalar.h"
#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarProjectElement.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropScalar::CDrvdPropScalar
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDrvdPropScalar::CDrvdPropScalar
	(
	CMemoryPool *mp
	)
	:
	m_mp(mp),
	m_is_prop_derived(NULL),
	m_pcrsDefined(NULL),
	m_pcrsSetReturningFunction(NULL),
	m_pcrsUsed(NULL),
	m_fHasSubquery(false),
	m_ppartinfo(NULL),
	m_pfp(NULL),
	m_fHasNonScalarFunction(false),
	m_ulDistinctAggs(0),
	m_fHasMultipleDistinctAggs(false),
	m_fHasScalarArrayCmp(false),
	m_is_complete(false)
{
	m_is_prop_derived = GPOS_NEW(mp) CBitSet(mp, EdptSentinel);
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropScalar::~CDrvdPropScalar
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDrvdPropScalar::~CDrvdPropScalar()
{
	CRefCount::SafeRelease(m_is_prop_derived);
	CRefCount::SafeRelease(m_pcrsDefined);
	CRefCount::SafeRelease(m_pcrsSetReturningFunction);
	CRefCount::SafeRelease(m_pcrsUsed);
	CRefCount::SafeRelease(m_ppartinfo);
	CRefCount::SafeRelease(m_pfp);
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropScalar::Derive
//
//	@doc:
//		Derive scalar props
//
//---------------------------------------------------------------------------
void
CDrvdPropScalar::Derive
	(
	CMemoryPool *,
	CExpressionHandle &exprhdl,
	CDrvdPropCtxt * // pdpctxt
	)
{
	// call derivation functions on the operator
	DeriveDefinedColumns(exprhdl);

	DeriveSetReturningFunctionColumns(exprhdl);

	DeriveUsedColumns(exprhdl);

	DeriveFunctionProperties(exprhdl);


	// derive existence of subqueries
	DeriveHasSubquery(exprhdl);

	DerivePartitionInfo(exprhdl);

	DeriveHasNonScalarFunction(exprhdl);

	DeriveTotalDistinctAggs(exprhdl);

	DeriveHasMultipleDistinctAggs(exprhdl);

	DeriveHasScalarArrayCmp(exprhdl);

	m_is_complete = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropScalar::GetDrvdScalarProps
//
//	@doc:
//		Short hand for conversion
//
//---------------------------------------------------------------------------
CDrvdPropScalar *
CDrvdPropScalar::GetDrvdScalarProps
	(
	CDrvdProp *pdp
	)
{
	GPOS_ASSERT(NULL != pdp);
	GPOS_ASSERT(EptScalar == pdp->Ept() && "This is not a scalar properties container");

	return dynamic_cast<CDrvdPropScalar*>(pdp);
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropScalar::FSatisfies
//
//	@doc:
//		Check for satisfying required properties
//
//---------------------------------------------------------------------------
BOOL
CDrvdPropScalar::FSatisfies
	(
	const CReqdPropPlan *prpp
	)
	const
{
	GPOS_ASSERT(NULL != prpp);
	GPOS_ASSERT(NULL != prpp->PcrsRequired());

	BOOL fSatisfies = m_pcrsDefined->ContainsAll(prpp->PcrsRequired());

	return fSatisfies;
}

// defined columns
CColRefSet *
CDrvdPropScalar::GetDefinedColumns() const
{
	return m_pcrsDefined;
}

CColRefSet *
CDrvdPropScalar::DeriveDefinedColumns(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsDefined))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
        m_pcrsDefined = popScalar->PcrsDefined(m_mp, exprhdl);

		// add defined columns of children
		const ULONG arity = exprhdl.Arity();
		for (ULONG i = 0; i < arity; i++)
		{
			// only propagate properties from scalar children
			if (exprhdl.FScalarChild(i))
			{
				m_pcrsDefined->Union(exprhdl.DeriveDefinedColumns(i));
			}
		}
	}
	return m_pcrsDefined;
}

// used columns
CColRefSet *
CDrvdPropScalar::GetUsedColumns() const
{
	return m_pcrsUsed;
}

CColRefSet *
CDrvdPropScalar::DeriveUsedColumns(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsUsed))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
		m_pcrsUsed = popScalar->PcrsUsed(m_mp, exprhdl);

		// add used columns of children
		const ULONG arity = exprhdl.Arity();
		for (ULONG i = 0; i < arity; i++)
		{
			// only propagate properties from scalar children
			if (exprhdl.FScalarChild(i))
			{
				m_pcrsUsed->Union(exprhdl.DeriveUsedColumns(i));
			}
			else
			{
				GPOS_ASSERT(CUtils::FSubquery(popScalar));
				// parent operator is a subquery, add outer references
				// from its relational child as used columns
				m_pcrsUsed->Union(exprhdl.DeriveOuterReferences(0));
			}
		}
	}
	return m_pcrsUsed;
}

// columns containing set-returning function
CColRefSet *
CDrvdPropScalar::GetSetReturningFunctionColumns() const
{
	return m_pcrsSetReturningFunction;
}

CColRefSet *
CDrvdPropScalar::DeriveSetReturningFunctionColumns(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsSetReturningFunction))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
		m_pcrsSetReturningFunction = popScalar->PcrsSetReturningFunction(m_mp, exprhdl);

		const ULONG arity = exprhdl.Arity();
		for (ULONG i = 0; i < arity; i++)
		{
			// only propagate properties from scalar children
			if (exprhdl.FScalarChild(i))
			{
			m_pcrsSetReturningFunction->Union(exprhdl.DeriveSetReturningFunctionColumns(i));
			}
		}
		if (COperator::EopScalarProjectElement == exprhdl.Pop()->Eopid())
		{
			if (DeriveHasNonScalarFunction(exprhdl))
			{
				CScalarProjectElement *pspeProject = (CScalarProjectElement *)(exprhdl.Pop());
				m_pcrsSetReturningFunction->Include(pspeProject->Pcr());
			}
		}
	}
	return m_pcrsSetReturningFunction;
}

// do subqueries appear in the operator's tree?
BOOL
CDrvdPropScalar::GetHasSubquery() const
{
	return m_fHasSubquery;
}

BOOL
CDrvdPropScalar::DeriveHasSubquery(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptFHasSubquery))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
		m_fHasSubquery = popScalar->FHasSubquery(exprhdl);
	}
	return m_fHasSubquery;
}

// derived partition consumers
CPartInfo *
CDrvdPropScalar::GetPartitionInfo() const
{
	return m_ppartinfo;
}

CPartInfo *
CDrvdPropScalar::DerivePartitionInfo(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPPartInfo))
	{
		if (DeriveHasSubquery(exprhdl))
		{
			CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
			m_ppartinfo = popScalar->PpartinfoDerive(m_mp, exprhdl);
		}
		else
		{
			m_ppartinfo = GPOS_NEW(m_mp) CPartInfo(m_mp);
		}

	}
	return m_ppartinfo;
}

// function properties
CFunctionProp *
CDrvdPropScalar::GetFunctionProperties() const
{
	return m_pfp;
}

CFunctionProp *
CDrvdPropScalar::DeriveFunctionProperties(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPfp))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
		m_pfp = popScalar->DeriveFunctionProperties(m_mp, exprhdl);
	}
	return m_pfp;
}

// scalar expression contains non-scalar function?
BOOL
CDrvdPropScalar::GetHasNonScalarFunction() const
{
	return m_fHasNonScalarFunction;
}

BOOL
CDrvdPropScalar::DeriveHasNonScalarFunction(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptFHasNonScalarFunction))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
		m_fHasNonScalarFunction = popScalar->FHasNonScalarFunction(exprhdl);
	}
	return m_fHasNonScalarFunction;
}

// return total number of Distinct Aggs, only applicable to project list
ULONG
CDrvdPropScalar::GetTotalDistinctAggs() const
{
	return m_ulDistinctAggs;
}

ULONG
CDrvdPropScalar::DeriveTotalDistinctAggs(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptUlDistinctAggs))
	{
		if (COperator::EopScalarProjectList == exprhdl.Pop()->Eopid())
		{
			m_ulDistinctAggs = CScalarProjectList::UlDistinctAggs(exprhdl);
		}
	}
	return m_ulDistinctAggs;
}

// does operator define Distinct Aggs on different arguments, only applicable to project lists
BOOL
CDrvdPropScalar::GetHasMultipleDistinctAggs() const
{
	return m_fHasMultipleDistinctAggs;
}

BOOL
CDrvdPropScalar::DeriveHasMultipleDistinctAggs(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptFHasMultipleDistinctAggs))
	{
		if (COperator::EopScalarProjectList == exprhdl.Pop()->Eopid())
		{
			m_fHasMultipleDistinctAggs = CScalarProjectList::FHasMultipleDistinctAggs(exprhdl);
		}
	}
	return m_fHasMultipleDistinctAggs;
}

BOOL
CDrvdPropScalar::GetHasScalarArrayCmp() const
{
	return m_fHasScalarArrayCmp;
}

BOOL
CDrvdPropScalar::DeriveHasScalarArrayCmp(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptFHasScalarArrayCmp))
	{
		CScalar *popScalar = CScalar::PopConvert(exprhdl.Pop());
		m_fHasScalarArrayCmp = popScalar->FHasScalarArrayCmp(exprhdl);
	}
	return m_fHasScalarArrayCmp;
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropScalar::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CDrvdPropScalar::OsPrint
	(
	IOstream &os
	)
	const
{
		os	<<	"Defined Columns: [" << *m_pcrsDefined << "], "
			<<	"Used Columns: [" << *m_pcrsUsed << "], "
			<<	"Set Returning Function Columns: [" << *m_pcrsSetReturningFunction << "], "
			<<	"Has Subqs: [" << m_fHasSubquery << "], "
			<<	"Function Properties: [" << *m_pfp << "], "
			<<	"Has Non-scalar Funcs: [" << m_fHasNonScalarFunction << "], ";

		if (0 < m_ulDistinctAggs)
		{
			os
				<<	"Distinct Aggs: [" << m_ulDistinctAggs << "]"
				<<	"Has Multiple Distinct Aggs: [" << m_fHasMultipleDistinctAggs << "]";
		}

		return os;
}

// EOF
