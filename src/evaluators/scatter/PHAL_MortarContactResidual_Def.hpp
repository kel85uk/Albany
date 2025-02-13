//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifdef ALBANY_TIMER
#include <chrono>
#endif
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ContactManager.hpp"

// **********************************************************************
// Base Class Generic Implemtation
// **********************************************************************
namespace PHAL {

template<typename EvalT, typename Traits>
MortarContactResidualBase<EvalT, Traits>::
MortarContactResidualBase(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl)
{

  scatter_operation = Teuchos::rcp(new PHX::Tag<ScalarT>
    ("MortarContact", dl->dummy));

  const Teuchos::ArrayRCP<std::string>& names =
    p.get< Teuchos::ArrayRCP<std::string> >("Residual Names");


    numFieldsBase = names.size();
    const std::size_t num_val = numFieldsBase;
    val.resize(num_val);
    for (std::size_t eq = 0; eq < numFieldsBase; ++eq) {
      PHX::MDField<ScalarT const,Cell,Node> mdf(names[eq],dl->node_scalar);
      val[eq] = mdf;
      this->addDependentField(val[eq]);
    }

#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
    val_kokkos.resize(numFieldsBase);
#endif

  if (p.isType<int>("Offset of First DOF"))
    offset = p.get<int>("Offset of First DOF");
  else offset = 0;

  this->addEvaluatedField(*scatter_operation);

  this->setName("MortarContactResidual"+PHX::typeAsString<EvalT>());
}

// **********************************************************************
template<typename EvalT, typename Traits>
void MortarContactResidualBase<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
    for (std::size_t eq = 0; eq < numFieldsBase; ++eq)
      this->utils.setFieldData(val[eq],fm);
    numNodes = val[0].extent(1);
}

// **********************************************************************
// Specialization: Residual
// **********************************************************************
template<typename Traits>
MortarContactResidual<PHAL::AlbanyTraits::Residual,Traits>::
MortarContactResidual(const Teuchos::ParameterList& p,
                       const Teuchos::RCP<Albany::Layouts>& dl)
  : MortarContactResidualBase<PHAL::AlbanyTraits::Residual,Traits>(p,dl),
  numFields(MortarContactResidualBase<PHAL::AlbanyTraits::Residual,Traits>::numFieldsBase) {}

// **********************************************************************
// Kokkos kernels
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Residual,Traits>::
operator() (const PHAL_MortarContactResRank0_Tag&, const int& cell) const
{
  for (std::size_t node = 0; node < this->numNodes; node++)
    for (std::size_t eq = 0; eq < numFields; eq++) {
      const LO id = nodeID(cell,node,this->offset + eq);
      Kokkos::atomic_fetch_add(&f_kokkos(id), val_kokkos[eq](cell,node));
    }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Residual,Traits>::
operator() (const PHAL_MortarContactResRank1_Tag&, const int& cell) const
{
  for (std::size_t node = 0; node < this->numNodes; node++)
    for (std::size_t eq = 0; eq < numFields; eq++) {
      const LO id = nodeID(cell,node,this->offset + eq);
      Kokkos::atomic_fetch_add(&f_kokkos(id), this->valVec(cell,node,eq));
    }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Residual,Traits>::
operator() (const PHAL_MortarContactResRank2_Tag&, const int& cell) const
{
  for (std::size_t node = 0; node < this->numNodes; node++)
    for (std::size_t i = 0; i < numDims; i++)
      for (std::size_t j = 0; j < numDims; j++) {
        const LO id = nodeID(cell,node,this->offset + i*numDims + j);
        Kokkos::atomic_fetch_add(&f_kokkos(id), this->valTensor(cell,node,i,j)); 
      }
}
#endif

// **********************************************************************
template<typename Traits>
void MortarContactResidual<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  Teuchos::RCP<Thyra_Vector> f = workset.f;
  Teuchos::RCP<const Albany::ContactManager> contactManager =
	workset.disc->getContactManager();

  //get nonconst (read and write) view of f
  Teuchos::ArrayRCP<ST> f_nonconstView = Albany::getNonconstLocalData(f);

  contactManager->fillInMortarResidual(workset.wsIndex, f_nonconstView);

#else
#ifdef ALBANY_TIMER
  auto start = std::chrono::high_resolution_clock::now();
#endif
  // Get map for local data structures
  nodeID = workset.wsElNodeEqID;

  // Get device view from thyra vector
  f_kokkos = Albany::getNonconstDeviceData(workset.f);

  // Get MDField views from std::vector
  for (int i = 0; i < numFields; i++) {
    val_kokkos[i] = this->val[i].get_view();
  }

  Kokkos::parallel_for(PHAL_MortarContactResRank0_Policy(0,workset.numCells),*this);
  cudaCheckError();

#ifdef ALBANY_TIMER
  PHX::Device::fence();
  auto elapsed = std::chrono::high_resolution_clock::now() - start;
  long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  long long millisec= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::cout<< "Mortar Contact Residual time = "  << millisec << "  "  << microseconds << std::endl;
#endif
#endif
}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
MortarContactResidual<PHAL::AlbanyTraits::Jacobian, Traits>::
MortarContactResidual(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl)
  : MortarContactResidualBase<PHAL::AlbanyTraits::Jacobian,Traits>(p,dl),
  numFields(MortarContactResidualBase<PHAL::AlbanyTraits::Jacobian,Traits>::numFieldsBase) {}

// **********************************************************************
// Kokkos kernels
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactResRank0_Tag&, const int& cell) const
{
  for (std::size_t node = 0; node < this->numNodes; node++)
    for (std::size_t eq = 0; eq < numFields; eq++) {
      const LO id = nodeID(cell,node,this->offset + eq);
      Kokkos::atomic_fetch_add(&f_kokkos(id), (val_kokkos[eq](cell,node)).val());
    }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactJacRank0_Adjoint_Tag&, const int& cell) const
{
  //const int neq = nodeID.extent(2);
  //const int nunk = neq*this->numNodes;
  // Irina TOFIX replace 500 with nunk with Kokkos::malloc is available
  LO colT[500];
  LO rowT;
  //std::vector<LO> colT(nunk);
  //colT=(LO*) Kokkos::cuda_malloc<PHX::Device>(nunk*sizeof(LO));

  if (nunk>500) Kokkos::abort("ERROR (MortarContactResidual): nunk > 500");

  for (int node_col=0; node_col<this->numNodes; node_col++) {
    for (int eq_col=0; eq_col<neq; eq_col++) {
      colT[neq * node_col + eq_col] =  nodeID(cell,node_col,eq_col);
    }
  }

  for (int node = 0; node < this->numNodes; ++node) {
    for (int eq = 0; eq < numFields; eq++) {
      rowT = nodeID(cell,node,this->offset + eq);
      auto valptr = val_kokkos[eq](cell,node);
      for (int lunk=0; lunk<nunk; lunk++) {
        ST val = valptr.fastAccessDx(lunk);
        Jac_kokkos.sumIntoValues(colT[lunk], &rowT, 1, &val, false, true); 
      }
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactJacRank0_Tag&, const int& cell) const
{
  //const int neq = nodeID.extent(2);
  //const int nunk = neq*this->numNodes;
  // Irina TOFIX replace 500 with nunk with Kokkos::malloc is available
  //colT=(LO*) Kokkos::cuda_malloc<PHX::Device>(nunk*sizeof(LO));
  LO rowT;
  LO colT[500];
  ST vals[500];
  //std::vector<LO> colT(nunk);
  //std::vector<ST> vals(nunk);

  if (nunk>500) Kokkos::abort("ERROR (MortarContactResidual): nunk > 500");

  for (int node_col=0, i=0; node_col<this->numNodes; node_col++) {
    for (int eq_col=0; eq_col<neq; eq_col++) {
      colT[neq * node_col + eq_col] = nodeID(cell,node_col,eq_col);
    }
  }

  for (int node = 0; node < this->numNodes; ++node) {
    for (int eq = 0; eq < numFields; eq++) {
      rowT = nodeID(cell,node,this->offset + eq);
      auto valptr = val_kokkos[eq](cell,node);
      for (int i = 0; i < nunk; ++i) vals[i] = valptr.fastAccessDx(i);
      Jac_kokkos.sumIntoValues(rowT, colT, nunk, vals, false, true);
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactResRank1_Tag&, const int& cell) const
{
  for (std::size_t node = 0; node < this->numNodes; node++)
    for (std::size_t eq = 0; eq < numFields; eq++) {
      const LO id = nodeID(cell,node,this->offset + eq);
      Kokkos::atomic_fetch_add(&f_kokkos(id), (this->valVec(cell,node,eq)).val());
    }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactJacRank1_Adjoint_Tag&, const int& cell) const
{
  //const int neq = nodeID.extent(2);
  //const int nunk = neq*this->numNodes;
  // Irina TOFIX replace 500 with nunk with Kokkos::malloc is available
  LO colT[500];
  LO rowT;
  ST vals[500];
  //std::vector<ST> vals(nunk);
  //std::vector<LO> colT(nunk);
  //colT=(LO*) Kokkos::malloc<PHX::Device>(nunk*sizeof(LO));

  if (nunk>500) Kokkos::abort("ERROR (MortarContactResidual): nunk > 500");

  for (int node_col=0, i=0; node_col<this->numNodes; node_col++) {
    for (int eq_col=0; eq_col<neq; eq_col++) {
      colT[neq * node_col + eq_col] = nodeID(cell,node_col,eq_col);
    }
  }

  for (int node = 0; node < this->numNodes; ++node) {
    for (int eq = 0; eq < numFields; eq++) {
      rowT = nodeID(cell,node,this->offset + eq);
      if (((this->valVec)(cell,node,eq)).hasFastAccess()) {
        for (int lunk=0; lunk<nunk; lunk++){
          ST val = ((this->valVec)(cell,node,eq)).fastAccessDx(lunk);
          Jac_kokkos.sumIntoValues(colT[lunk], &rowT, 1, &val, false, true);
        }
      }//has fast access
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactJacRank1_Tag&, const int& cell) const
{
  //const int neq = nodeID.extent(2);
  //const int nunk = neq*this->numNodes;
  // Irina TOFIX replace 500 with nunk with Kokkos::malloc is available
  LO colT[500];
  LO rowT;
  ST vals[500];
  //std::vector<LO> colT(nunk);
  //colT=(LO*) Kokkos::malloc<PHX::Device>(nunk*sizeof(LO));
  //std::vector<ST> vals(nunk);

  if (nunk>500) Kokkos::abort ("ERROR (MortarContactResidual): nunk > 500");

  for (int node_col=0, i=0; node_col<this->numNodes; node_col++) {
    for (int eq_col=0; eq_col<neq; eq_col++) {
      colT[neq * node_col + eq_col] = nodeID(cell,node_col,eq_col);
    }
  }

  for (int node = 0; node < this->numNodes; ++node) {
    for (int eq = 0; eq < numFields; eq++) {
      rowT = nodeID(cell,node,this->offset + eq);
      if (((this->valVec)(cell,node,eq)).hasFastAccess()) {
        for (int i = 0; i < nunk; ++i) vals[i] = (this->valVec)(cell,node,eq).fastAccessDx(i);
        Jac_kokkos.sumIntoValues(rowT, colT, nunk, vals, false, true);
      }
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactResRank2_Tag&, const int& cell) const
{
  for (std::size_t node = 0; node < this->numNodes; node++)
    for (std::size_t i = 0; i < numDims; i++)
      for (std::size_t j = 0; j < numDims; j++) {
        const LO id = nodeID(cell,node,this->offset + i*numDims + j);
        Kokkos::atomic_fetch_add(&f_kokkos(id), (this->valTensor(cell,node,i,j)).val()); 
      }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactJacRank2_Adjoint_Tag&, const int& cell) const
{
  //const int neq = nodeID.extent(2);
  //const int nunk = neq*this->numNodes;
  // Irina TOFIX replace 500 with nunk with Kokkos::malloc is available
  LO colT[500];
  LO rowT;
  //std::vector<LO> colT(nunk);
  //colT=(LO*) Kokkos::malloc<PHX::Device>(nunk*sizeof(LO));

  if (nunk>500) Kokkos::abort("ERROR (MortarContactResidual): nunk > 500");

  for (int node_col=0, i=0; node_col<this->numNodes; node_col++) {
    for (int eq_col=0; eq_col<neq; eq_col++) {
      colT[neq * node_col + eq_col] = nodeID(cell,node_col,eq_col);
    }
  }

  for (int node = 0; node < this->numNodes; ++node) {
    for (int eq = 0; eq < numFields; eq++) {
      rowT = nodeID(cell,node,this->offset + eq);
      if (((this->valTensor)(cell,node, eq/numDims, eq%numDims)).hasFastAccess()) {
        for (int lunk=0; lunk<nunk; lunk++) {
          ST val = ((this->valTensor)(cell,node, eq/numDims, eq%numDims)).fastAccessDx(lunk);
          Jac_kokkos.sumIntoValues (colT[lunk], &rowT, 1, &val, false, true);
        }
      }//has fast access
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian,Traits>::
operator() (const PHAL_MortarContactJacRank2_Tag&, const int& cell) const
{
  //const int neq = nodeID.extent(2);
  //const int nunk = neq*this->numNodes;
  // Irina TOFIX replace 500 with nunk with Kokkos::malloc is available
  LO colT[500];
  LO rowT;
  ST vals[500];
  //std::vector<LO> colT(nunk);
  //colT=(LO*) Kokkos::malloc<PHX::Device>(nunk*sizeof(LO));
  //std::vector<ST> vals(nunk);

  if (nunk>500) Kokkos::abort("ERROR (MortarContactResidual): nunk > 500");

  for (int node_col=0, i=0; node_col<this->numNodes; node_col++) {
    for (int eq_col=0; eq_col<neq; eq_col++) {
      colT[neq * node_col + eq_col] = nodeID(cell,node_col,eq_col);
    }
  }

  for (int node = 0; node < this->numNodes; ++node) {
    for (int eq = 0; eq < numFields; eq++) {
      rowT = nodeID(cell,node,this->offset + eq);
      if (((this->valTensor)(cell,node, eq/numDims, eq%numDims)).hasFastAccess()) {
        for (int i = 0; i < nunk; ++i) vals[i] = (this->valTensor)(cell,node, eq/numDims, eq%numDims).fastAccessDx(i);
        Jac_kokkos.sumIntoValues(rowT, colT, nunk,  vals, false, true);
      }
    }
  }
}
#endif

// **********************************************************************
template<typename Traits>
void MortarContactResidual<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  auto nodeID = workset.wsElNodeEqID;
  Teuchos::RCP<Tpetra_Vector> fT = workset.fT;
  Teuchos::RCP<Tpetra_CrsMatrix> JacT = workset.JacT;
  const bool loadResid = Teuchos::nonnull(fT);
  Teuchos::Array<LO> colT;
  const int neq = nodeID.extent(2);
  const int nunk = neq*this->numNodes;
  colT.resize(nunk);
  int numDims = 0;

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    // Local Unks: Loop over nodes in element, Loop over equations per node
    for (unsigned int node_col=0, i=0; node_col<this->numNodes; node_col++){
      for (unsigned int eq_col=0; eq_col<neq; eq_col++) {
        colT[neq * node_col + eq_col] = nodeID(cell,node_col,eq_col);
      }
    }
    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT const>::type
          valptr = this->val[eq](cell,node);
        const LO rowT = nodeID(cell,node,this->offset + eq);
        if (loadResid)
          fT->sumIntoLocalValue(rowT, valptr.val());
        // Check derivative array is nonzero
        if (valptr.hasFastAccess()) {
          if (workset.is_adjoint) {
            // Sum Jacobian transposed
            for (unsigned int lunk = 0; lunk < nunk; lunk++)
              JacT->sumIntoLocalValues(
                colT[lunk], Teuchos::arrayView(&rowT, 1),
                Teuchos::arrayView(&(valptr.fastAccessDx(lunk)), 1));
          }
          else {
            // Sum Jacobian entries all at once
            JacT->sumIntoLocalValues(
              rowT, colT, Teuchos::arrayView(&(valptr.fastAccessDx(0)), nunk));
          }
        } // has fast access
      }
    }
  }

#else
#ifdef ALBANY_TIMER
  auto start = std::chrono::high_resolution_clock::now();
#endif
  // Get map for local data structures
  nodeID = workset.wsElNodeEqID;

  // Get dimensions
  neq = nodeID.extent(2);
  nunk = neq*this->numNodes;

  // Get Tpetra vector view and local matrix
  const bool loadResid = Teuchos::nonnull(workset.f);
  if (loadResid) {
    f_kokkos = Albany::getNonconstDeviceData(workset.f);
  }
  Jac_kokkos = Albany::getNonconstDeviceData(workset.Jac);

    // Get MDField views from std::vector
    for (int i = 0; i < numFields; i++)
      val_kokkos[i] = this->val[i].get_view();

    if (loadResid) {
      Kokkos::parallel_for(PHAL_MortarContactResRank0_Policy(0,workset.numCells),*this);
      cudaCheckError();
    }

    if (workset.is_adjoint) {
      Kokkos::parallel_for(PHAL_MortarContactJacRank0_Adjoint_Policy(0,workset.numCells),*this);  
      cudaCheckError();
    }
    else {
      Kokkos::parallel_for(PHAL_MortarContactJacRank0_Policy(0,workset.numCells),*this);
      cudaCheckError();
    }

#ifdef ALBANY_TIMER
  PHX::Device::fence();
  auto elapsed = std::chrono::high_resolution_clock::now() - start;
  long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  long long millisec= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::cout<< "Mortar Contact Jacobian time = "  << millisec << "  "  << microseconds << std::endl;
#endif 
#endif
}

// **********************************************************************
// Specialization: Tangent
// **********************************************************************

template<typename Traits>
MortarContactResidual<PHAL::AlbanyTraits::Tangent, Traits>::
MortarContactResidual(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl)
  : MortarContactResidualBase<PHAL::AlbanyTraits::Tangent,Traits>(p,dl),
  numFields(MortarContactResidualBase<PHAL::AlbanyTraits::Tangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void MortarContactResidual<PHAL::AlbanyTraits::Tangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  auto nodeID = workset.wsElNodeEqID;
  Teuchos::RCP<Tpetra_Vector> fT = workset.fT;
  Teuchos::RCP<Tpetra_MultiVector> JVT = workset.JVT;
  Teuchos::RCP<Tpetra_MultiVector> fpT = workset.fpT;

  int numDims = 0;

  for (std::size_t cell = 0; cell < workset.numCells; ++cell ) {
    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT const>::type valref = this->val[eq] (cell, node);

        const LO row = nodeID(cell,node,this->offset + eq);

        if (Teuchos::nonnull (fT))
          fT->sumIntoLocalValue (row, valref.val ());

        if (Teuchos::nonnull (JVT))
          for (int col = 0; col < workset.num_cols_x; col++)
            JVT->sumIntoLocalValue (row, col, valref.dx (col));

        if (Teuchos::nonnull (fpT))
          for (int col = 0; col < workset.num_cols_p; col++)
            fpT->sumIntoLocalValue (row, col, valref.dx (col + workset.param_offset));
      }
    }
  }
}

// **********************************************************************
// Specialization: DistParamDeriv
// **********************************************************************

template<typename Traits>
MortarContactResidual<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
MortarContactResidual(const Teuchos::ParameterList& p,
                const Teuchos::RCP<Albany::Layouts>& dl)
  : MortarContactResidualBase<PHAL::AlbanyTraits::DistParamDeriv,Traits>(p,dl),
  numFields(MortarContactResidualBase<PHAL::AlbanyTraits::DistParamDeriv,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void MortarContactResidual<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  auto nodeID = workset.wsElNodeEqID;
  Teuchos::RCP<Tpetra_MultiVector> fpVT = workset.fpVT;
  bool trans = workset.transpose_dist_param_deriv;
  int num_cols = workset.VpT->domain()->dim();

  if(workset.local_Vp[0].size() == 0) return; //In case the parameter has not been gathered, e.g. parameter is used only in Dirichlet conditions.

  int numDims = 0;

  if (trans) {
    const int neq = nodeID.extent(2);
    const Albany::IDArray&  wsElDofs = workset.distParamLib->get(workset.dist_param_deriv_name)->workset_elem_dofs()[workset.wsIndex];
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> >& local_Vp =
        workset.local_Vp[cell];
      const int num_deriv = this->numNodes;//local_Vp.size()/numFields;
      for (int i=0; i<num_deriv; i++) {
        for (int col=0; col<num_cols; col++) {
          double val = 0.0;
          for (std::size_t node = 0; node < this->numNodes; ++node) {
            for (std::size_t eq = 0; eq < numFields; eq++) {
              typename PHAL::Ref<ScalarT const>::type
                        valref = this->val[eq](cell,node);
              val += valref.dx(i)*local_Vp[node*neq+eq+this->offset][col];  //numField can be less then neq
            }
          }
          const LO row = wsElDofs((int)cell,i,0);
          if(row >=0)
            fpVT->sumIntoLocalValue(row, col, val);
        }
      }
    }
  }
  else {
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<double> >& local_Vp =
        workset.local_Vp[cell];
      const int num_deriv = local_Vp.size();

      for (std::size_t node = 0; node < this->numNodes; ++node) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          typename PHAL::Ref<ScalarT const>::type
                    valref = this->val[eq](cell,node);
          const int row = nodeID(cell,node,this->offset + eq);
          for (int col=0; col<num_cols; col++) {
            double val = 0.0;
            for (int i=0; i<num_deriv; ++i)
              val += valref.dx(i)*local_Vp[i][col];
            fpVT->sumIntoLocalValue(row, col, val);
          }
        }
      }
    }
  }
}

}

