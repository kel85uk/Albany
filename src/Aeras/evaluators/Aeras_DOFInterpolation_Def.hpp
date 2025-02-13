//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Intrepid2_FunctionSpaceTools.hpp"

namespace Aeras {

//**********************************************************************
template<typename EvalT, typename Traits>
DOFInterpolation<EvalT, Traits>::
DOFInterpolation(Teuchos::ParameterList& p,
                 const Teuchos::RCP<Aeras::Layouts>& dl) :
  val_node    (p.get<std::string>   ("Variable Name"), 
               p.get<Teuchos::RCP<PHX::DataLayout> >("Nodal Variable Layout",     dl->node_scalar_level)),
  BF          (p.get<std::string>   ("BF Name"),                                  dl->node_qp_scalar),
  val_qp      (p.get<std::string>   ("Variable Name"), 
               p.get<Teuchos::RCP<PHX::DataLayout> >("Quadpoint Variable Layout", dl->qp_scalar_level)),
  numNodes   (dl->node_scalar             ->extent(1)),
  numLevels  (dl->node_scalar_level       ->extent(2)),
  numRank    (val_node.fieldTag().dataLayout().rank())
{
  this->addDependentField(val_node);
  this->addDependentField(BF);
  this->addEvaluatedField(val_qp);

  this->setName("Aeras::DOFInterpolation" + PHX::typeAsString<EvalT>());

  TEUCHOS_TEST_FOR_EXCEPTION( (numRank!=2 && numRank!=3),
     std::logic_error,"Aeras::DOFGradInterpolation supports scalar or vector only");
}

//**********************************************************************
template<typename EvalT, typename Traits>
void DOFInterpolation<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(val_node,fm);
  this->utils.setFieldData(BF,fm);
  this->utils.setFieldData(val_qp,fm);
  d.fill_field_dependencies(this->dependentFields(),this->evaluatedFields());
}

//**********************************************************************
// Kokkos kernels
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void DOFInterpolation<EvalT, Traits>::
operator() (const DOFInterpolation_numRank2_Tag& tag, const int& cell) const{
  for (int node=0; node < numNodes; ++node) {
    typename PHAL::Ref<ScalarT>::type vqp = val_qp(cell,node) = 0.0;
    vqp += val_node(cell, node) * BF(cell, node, node);
  }
}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void DOFInterpolation<EvalT, Traits>::
operator() (const DOFInterpolation_Tag& tag, const int& cell) const{
  for (int node=0; node < numNodes; ++node) {
    for (int level=0; level < numLevels; ++level) {
      typename PHAL::Ref<ScalarT>::type vqp = val_qp(cell,node,level) = 0.0;
      vqp += val_node(cell, node, level) * BF(cell, node, node);
    }
  }
}

#endif

//**********************************************************************
template<typename EvalT, typename Traits>
void DOFInterpolation<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  //Intrepid2 version:
  // for (int i=0; i < val_qp.size() ; i++) val_qp[i] = 0.0;
  // Intrepid2::FunctionSpaceTools:: evaluate<ScalarT>(val_qp, val_node, BF);
  
  if (numRank == 2) {
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int node=0; node < numNodes; ++node) {
        typename PHAL::Ref<ScalarT>::type vqp = val_qp(cell,node) = 0.0;
        vqp += val_node(cell, node) * BF(cell, node, node);
      }
    }
  } else {
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int node=0; node < numNodes; ++node) {
        for (int level=0; level < numLevels; ++level) {
          typename PHAL::Ref<ScalarT>::type vqp = val_qp(cell,node,level) = 0.0;
          vqp += val_node(cell, node, level) * BF(cell, node, node);
        }
      }
    }
  }

#else
  if (numRank == 2) {
    Kokkos::parallel_for(DOFInterpolation_numRank2_Policy(0,workset.numCells),*this);
  } else {
    Kokkos::parallel_for(DOFInterpolation_Policy(0,workset.numCells),*this);
  }

#endif
}
}

