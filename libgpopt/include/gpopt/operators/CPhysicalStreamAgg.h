//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalStreamAgg.h
//
//	@doc:
//		Sort-based stream Aggregate operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalStreamAgg_H
#define GPOS_CPhysicalStreamAgg_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalAgg.h"

namespace gpopt
{
	// fwd declaration
	class CDistributionSpec;

	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalStreamAgg
	//
	//	@doc:
	//		Sort-based aggregate operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalStreamAgg : public CPhysicalAgg
	{
		private:

			// private copy ctor
			CPhysicalStreamAgg(const CPhysicalStreamAgg &);

			// local order spec
			COrderSpec *m_pos;

			// set representation of minimal grouping columns
			CColRefSet *m_pcrsMinimalGrpCols;

			// construct order spec on grouping column so that it covers required order spec
			COrderSpec *PosCovering(IMemoryPool *mp, COrderSpec *posRequired, CColRefArray *pdrgpcrGrp) const;

			BOOL m_should_enforce_distribution;

		protected:

			// compute required sort columns of the n-th child
			COrderSpec *PosRequiredStreamAgg
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				COrderSpec *posRequired,
				ULONG child_index,
				CColRefArray *pdrgpcrGrp
				)
				const;

			// initialize the order spec using the given array of columns
			void InitOrderSpec(IMemoryPool *mp, CColRefArray *pdrgpcrOrder);

		public:

			// ctor
			CPhysicalStreamAgg
				(
				IMemoryPool *mp,
				CColRefArray *colref_array,
				CColRefArray *pdrgpcrMinimal, // minimal grouping columns based on FD's
				COperator::EGbAggType egbaggtype,
				BOOL fGeneratesDuplicates,
				CColRefArray *pdrgpcrArgDQA,
				BOOL fMultiStage,
				BOOL isAggFromSplitDQA,
				CLogicalGbAgg::EAggStage aggStage,
				BOOL should_enforce_distribution
				);

			// dtor
			virtual
			~CPhysicalStreamAgg();


			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopPhysicalStreamAgg;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CPhysicalStreamAgg";
			}

			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required sort columns of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				COrderSpec *posRequired,
				ULONG child_index,
				CDrvdProp2dArray *, //pdrgpdpCtxt,
				ULONG //ulOptReq
				)
				const
			{
				return PosRequiredStreamAgg(mp, exprhdl, posRequired, child_index, m_pdrgpcrMinimal);
			}

			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive sort order
			virtual
			COrderSpec *PosDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				)
				const;

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CPhysicalStreamAgg *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopPhysicalStreamAgg == pop->Eopid() ||
						EopPhysicalStreamAggDeduplicate == pop->Eopid());

				return reinterpret_cast<CPhysicalStreamAgg*>(pop);
			}

	}; // class CPhysicalStreamAgg

}


#endif // !GPOS_CPhysicalStreamAgg_H

// EOF
