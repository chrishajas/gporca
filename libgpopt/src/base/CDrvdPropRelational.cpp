//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2011 EMC CORP.
//
//	@filename:
//		CDrvdPropRelational.cpp
//
//	@doc:
//		Relational derived properties;
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/task/CAutoSuspendAbort.h"
#include "gpos/task/CWorker.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CLogical.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/base/CDrvdPropRelational.h"
#include "gpopt/base/CReqdPropPlan.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CPartInfo.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::CDrvdPropRelational
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CDrvdPropRelational::CDrvdPropRelational
	(
	CMemoryPool *mp
	)
	:
	m_mp(mp),
	m_is_prop_derived(NULL),
	m_pcrsOutput(NULL),
	m_pcrsOuter(NULL),
	m_pcrsNotNull(NULL),
	m_pcrsCorrelatedApply(NULL),
	m_pkc(NULL),
	m_pdrgpfd(NULL),
	m_ulJoinDepth(0),
	m_ppartinfo(NULL),
	m_ppc(NULL),
	m_pfp(NULL),
	m_is_complete(false)
{
	m_is_prop_derived = GPOS_NEW(mp) CBitSet(mp, EdptSentinel);
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::~CDrvdPropRelational
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CDrvdPropRelational::~CDrvdPropRelational()
{
	{
		CAutoSuspendAbort asa;

		CRefCount::SafeRelease(m_is_prop_derived);
		CRefCount::SafeRelease(m_pcrsOutput);
		CRefCount::SafeRelease(m_pcrsOuter);
		CRefCount::SafeRelease(m_pcrsNotNull);
		CRefCount::SafeRelease(m_pcrsCorrelatedApply);
		CRefCount::SafeRelease(m_pkc);
		CRefCount::SafeRelease(m_pdrgpfd);
		CRefCount::SafeRelease(m_ppartinfo);
		CRefCount::SafeRelease(m_ppc);
		CRefCount::SafeRelease(m_pfp);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::Derive
//
//	@doc:
//		Derive relational props. This derives ALL properties
//
//---------------------------------------------------------------------------
void
CDrvdPropRelational::Derive
	(
	CMemoryPool *, //mp,
	CExpressionHandle &exprhdl,
	CDrvdPropCtxt * // pdpctxt
	)
{
	GPOS_CHECK_ABORT;

	// call output derivation function on the operator
	PcrsOutput(exprhdl);

	// derive outer-references
	PcrsOuter(exprhdl);
	
	// derive not null columns
	PcrsNotNull(exprhdl);

	// derive correlated apply columns
	PcrsCorrelatedApply(exprhdl);
	
	// derive constraint
	Ppc(exprhdl);

	// compute max card
	Maxcard(exprhdl);

	// derive keys
	Pkc(exprhdl);
	
	// derive join depth
	JoinDepth(exprhdl);

	// derive function properties
	Pfp(exprhdl);

	// derive functional dependencies
	Pdrgpfd(exprhdl);
	
	// derive partition consumers
	Ppartinfo(exprhdl);
	GPOS_ASSERT(NULL != m_ppartinfo);

	FHasPartialIndexes(exprhdl);

	m_is_complete = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::FSatisfies
//
//	@doc:
//		Check for satisfying required properties
//
//---------------------------------------------------------------------------
BOOL
CDrvdPropRelational::FSatisfies
	(
	const CReqdPropPlan *prpp
	)
	const
{
	GPOS_ASSERT(NULL != prpp);
	GPOS_ASSERT(NULL != prpp->PcrsRequired());

	BOOL fSatisfies = PcrsOutput()->ContainsAll(prpp->PcrsRequired());

	return fSatisfies;
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::GetRelationalProperties
//
//	@doc:
//		Short hand for conversion
//
//---------------------------------------------------------------------------
CDrvdPropRelational *
CDrvdPropRelational::GetRelationalProperties
	(
	DrvdPropArray *pdp
	)
{
	GPOS_ASSERT(NULL != pdp);
	GPOS_ASSERT(EptRelational == pdp->Ept() && "This is not a relational properties container");
	return dynamic_cast<CDrvdPropRelational*>(pdp);
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::PdrgpfdChild
//
//	@doc:
//		Helper for getting applicable FDs from child
//
//---------------------------------------------------------------------------
CFunctionalDependencyArray *
CDrvdPropRelational::PdrgpfdChild
	(
	CMemoryPool *mp,
	ULONG child_index,
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(child_index < exprhdl.Arity());
	GPOS_ASSERT(!exprhdl.FScalarChild(child_index));

	// get FD's of the child
	CFunctionalDependencyArray *pdrgpfdChild = exprhdl.Pdrgpfd(child_index);

	// get output columns of the parent
	CColRefSet *pcrsOutput = exprhdl.PcrsOutput();

	// collect child FD's that are applicable to the parent
	CFunctionalDependencyArray *pdrgpfd = GPOS_NEW(mp) CFunctionalDependencyArray(mp);
	const ULONG size = pdrgpfdChild->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CFunctionalDependency *pfd = (*pdrgpfdChild)[ul];

		// check applicability of FD's LHS
		if (pcrsOutput->ContainsAll(pfd->PcrsKey()))
		{
			// decompose FD's RHS to extract the applicable part
			CColRefSet *pcrsDetermined = GPOS_NEW(mp) CColRefSet(mp);
			pcrsDetermined->Include(pfd->PcrsDetermined());
			pcrsDetermined->Intersection(pcrsOutput);
			if (0 < pcrsDetermined->Size())
			{
				// create a new FD and add it to the output array
				pfd->PcrsKey()->AddRef();
				pcrsDetermined->AddRef();
				CFunctionalDependency *pfdNew =
					GPOS_NEW(mp) CFunctionalDependency(pfd->PcrsKey(), pcrsDetermined);
				pdrgpfd->Append(pfdNew);
			}
			pcrsDetermined->Release();
		}
	}

	return pdrgpfd;
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::PdrgpfdLocal
//
//	@doc:
//		Helper for deriving local FDs
//
//---------------------------------------------------------------------------
CFunctionalDependencyArray *
CDrvdPropRelational::PdrgpfdLocal
	(
	CMemoryPool *mp,
	CExpressionHandle &exprhdl
	)
{
	CFunctionalDependencyArray *pdrgpfd = GPOS_NEW(mp) CFunctionalDependencyArray(mp);

	// get local key
	CKeyCollection *pkc = exprhdl.Pkc();
	
	if (NULL == pkc)
	{
		return pdrgpfd;
	}
	
	ULONG ulKeys = pkc->Keys();
	for (ULONG ul = 0; ul < ulKeys; ul++)
	{
		CColRefArray *pdrgpcrKey = pkc->PdrgpcrKey(mp, ul);
		CColRefSet *pcrsKey = GPOS_NEW(mp) CColRefSet(mp);
		pcrsKey->Include(pdrgpcrKey);

		// get output columns
		CColRefSet *pcrsOutput = exprhdl.PcrsOutput();
		CColRefSet *pcrsDetermined = GPOS_NEW(mp) CColRefSet(mp);
		pcrsDetermined->Include(pcrsOutput);
		pcrsDetermined->Exclude(pcrsKey);

		if (0 < pcrsDetermined->Size())
		{
			// add FD between key and the rest of output columns
			pcrsKey->AddRef();
			pcrsDetermined->AddRef();
			CFunctionalDependency *pfdLocal = GPOS_NEW(mp) CFunctionalDependency(pcrsKey, pcrsDetermined);
			pdrgpfd->Append(pfdLocal);
		}

		pcrsKey->Release();
		pcrsDetermined->Release();
		CRefCount::SafeRelease(pdrgpcrKey);
	}

	return pdrgpfd;
}


//---------------------------------------------------------------------------
//	@function:
//		CDrvdPropRelational::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDrvdPropRelational::OsPrint
	(
	IOstream &os
	)
	const
{
	os	<<	"Output Cols: [" << *PcrsOutput() << "]"
		<<	", Outer Refs: [" << *PcrsOuter() << "]"
		<<	", Not Null Cols: [" << *PcrsNotNull() << "]"
		<< ", Corr. Apply Cols: [" << *PcrsCorrelatedApply() <<"]";

	if (NULL == Pkc())
	{
		os << ", Keys: []";
	}
	else
	{
		os << ", " << *Pkc();
	}
	
	os << ", Max Card: " << Maxcard();

	os << ", Join Depth: " << JoinDepth();

	os << ", Constraint Property: [" << *Ppc() << "]";

	const ULONG ulFDs = Pdrgpfd()->Size();
	
	os << ", FDs: [";
	for (ULONG ul = 0; ul < ulFDs; ul++)
	{
		CFunctionalDependency *pfd = (*Pdrgpfd())[ul];
		os << *pfd;
	}
	os << "]";
	
	os << ", Function Properties: [" << *Pfp() << "]";

	os << ", Part Info: [" << *Ppartinfo() << "]";

	if (FHasPartialIndexes())
	{
		os <<", Has Partial Indexes";
	}
	return os;
}

// output columns
CColRefSet *
CDrvdPropRelational::PcrsOutput() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pcrsOutput;
}

// output columns
CColRefSet *
CDrvdPropRelational::PcrsOutput(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsOutput))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_pcrsOutput = popLogical->PcrsDeriveOutput(m_mp, exprhdl);
	}

	return m_pcrsOutput;
}

// outer references
CColRefSet *
CDrvdPropRelational::PcrsOuter() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pcrsOuter;
}

// outer references
CColRefSet *
CDrvdPropRelational::PcrsOuter(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsOuter))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_pcrsOuter = popLogical->PcrsDeriveOuter(m_mp, exprhdl);
	}

	return m_pcrsOuter;
}

// nullable columns
CColRefSet *
CDrvdPropRelational::PcrsNotNull() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pcrsNotNull;
}

CColRefSet *
CDrvdPropRelational::PcrsNotNull(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsNotNull))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_pcrsNotNull = popLogical->PcrsDeriveNotNull(m_mp, exprhdl);
	}

	return m_pcrsNotNull;
}

// columns from the inner child of a correlated-apply expression that can be used above the apply expression
CColRefSet *
CDrvdPropRelational::PcrsCorrelatedApply() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pcrsCorrelatedApply;
}

CColRefSet *
CDrvdPropRelational::PcrsCorrelatedApply(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPcrsCorrelatedApply))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_pcrsCorrelatedApply = popLogical->PcrsDeriveCorrelatedApply(m_mp, exprhdl);
	}

	return m_pcrsCorrelatedApply;
}

// key collection
CKeyCollection *
CDrvdPropRelational::Pkc() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pkc;
}

CKeyCollection *
CDrvdPropRelational::Pkc(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPkc))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_pkc = popLogical->PkcDeriveKeys(m_mp, exprhdl);

		if (NULL == m_pkc && 1 == Maxcard(exprhdl))
		{
			m_pcrsOutput = PcrsOutput(exprhdl);

			if (0 < m_pcrsOutput->Size())
			{
				m_pcrsOutput->AddRef();
				m_pkc = GPOS_NEW(m_mp) CKeyCollection(m_mp, m_pcrsOutput);
			}
		}
	}

	return m_pkc;
}

// functional dependencies
CFunctionalDependencyArray *
CDrvdPropRelational::Pdrgpfd() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pdrgpfd;
}

CFunctionalDependencyArray *
CDrvdPropRelational::Pdrgpfd(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPdrgpfd))
	{
		CFunctionalDependencyArray *pdrgpfd = GPOS_NEW(m_mp) CFunctionalDependencyArray(m_mp);
		const ULONG arity = exprhdl.Arity();

		// collect applicable FD's from logical children
		for (ULONG ul = 0; ul < arity; ul++)
		{
			if (!exprhdl.FScalarChild(ul))
			{
				CFunctionalDependencyArray *pdrgpfdChild = PdrgpfdChild(m_mp, ul, exprhdl);
				CUtils::AddRefAppend<CFunctionalDependency, CleanupRelease>(pdrgpfd, pdrgpfdChild);
				pdrgpfdChild->Release();
			}
		}
		// add local FD's
		CFunctionalDependencyArray *pdrgpfdLocal = PdrgpfdLocal(m_mp, exprhdl);
		CUtils::AddRefAppend<CFunctionalDependency, CleanupRelease>(pdrgpfd, pdrgpfdLocal);
		pdrgpfdLocal->Release();

		m_pdrgpfd = pdrgpfd;
	}

	return m_pdrgpfd;
}

// max cardinality
CMaxCard
CDrvdPropRelational::Maxcard() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_maxcard;
}

CMaxCard
CDrvdPropRelational::Maxcard(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptMaxCard))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_maxcard = popLogical->Maxcard(m_mp, exprhdl);
	}

	return m_maxcard;
}

// join depth
ULONG
CDrvdPropRelational::JoinDepth() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_ulJoinDepth;
}

ULONG
CDrvdPropRelational::JoinDepth(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptJoinDepth))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_ulJoinDepth = popLogical->JoinDepth(m_mp, exprhdl);
	}

	return m_ulJoinDepth;
}

// partition consumers
CPartInfo *
CDrvdPropRelational::Ppartinfo() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_ppartinfo;
}

CPartInfo *
CDrvdPropRelational::Ppartinfo(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPpartinfo))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_ppartinfo = popLogical->PpartinfoDerive(m_mp, exprhdl);

		GPOS_ASSERT(NULL != m_ppartinfo);
	}

	return m_ppartinfo;
}

// constraint property
CPropConstraint *
CDrvdPropRelational::Ppc() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_ppc;
}

CPropConstraint *
CDrvdPropRelational::Ppc(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPpc))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_ppc = popLogical->PpcDeriveConstraint(m_mp, exprhdl);
	}

	return m_ppc;
}

// function properties
CFunctionProp *
CDrvdPropRelational::Pfp() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_pfp;
}

CFunctionProp *
CDrvdPropRelational::Pfp(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptPfp))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		m_pfp = popLogical->PfpDerive(m_mp, exprhdl);
	}

	return m_pfp;
}

// has partial indexes
BOOL
CDrvdPropRelational::FHasPartialIndexes() const
{
	GPOS_RTL_ASSERT(IsComplete());
	return m_fHasPartialIndexes;
}

BOOL
CDrvdPropRelational::FHasPartialIndexes(CExpressionHandle &exprhdl)
{
	if (!m_is_prop_derived->ExchangeSet(EdptFHasPartialIndexes))
	{
		CLogical *popLogical = CLogical::PopConvert(exprhdl.Pop());
		COperator::EOperatorId op_id = popLogical->Eopid();


		// determine if it is a dynamic get (with or without a select above it) with partial indexes
		if (COperator::EopLogicalDynamicGet == op_id)
		{
			m_fHasPartialIndexes =
					CLogicalDynamicGet::PopConvert(popLogical)->Ptabdesc()->FHasPartialIndexes();
		}
		else if (COperator::EopLogicalSelect == op_id)
		{
			m_fHasPartialIndexes = exprhdl.FHasPartialIndexes(0);
		}

	}

	return m_fHasPartialIndexes;
}
// EOF
