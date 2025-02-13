//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_RCP.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "PHAL_Utilities.hpp"

#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Aeras_Layouts.hpp"

namespace Aeras {

//**********************************************************************
template<typename EvalT, typename Traits>
XZHydrostatic_KineticEnergy<EvalT, Traits>::
XZHydrostatic_KineticEnergy(const Teuchos::ParameterList& p,
              const Teuchos::RCP<Aeras::Layouts>& dl) :
  u  (p.get<std::string> ("Velx"),           dl->node_vector_level),
  ke (p.get<std::string> ("Kinetic Energy"), dl->node_scalar_level),
  numNodes ( dl->node_scalar             ->extent(1)),
  numDims  ( dl->node_qp_gradient        ->extent(3)),
  numLevels( dl->node_scalar_level       ->extent(2))
{

  this->addDependentField(u);
  this->addEvaluatedField(ke);

  this->setName("Aeras::XZHydrostatic_KineticEnergy" + PHX::typeAsString<EvalT>());

  ke0 = 0.0;

}

//**********************************************************************
template<typename EvalT, typename Traits>
void XZHydrostatic_KineticEnergy<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(u,fm);
  this->utils.setFieldData(ke,fm);
  d.fill_field_dependencies(this->dependentFields(),this->evaluatedFields());
}

//**********************************************************************
// Kokkos kernels
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void XZHydrostatic_KineticEnergy<EvalT, Traits>::
operator() (const XZHydrostatic_KineticEnergy_Tag& tag, const int& cell) const{
  for (int node=0; node < numNodes; ++node) {
    for (int level=0; level < numLevels; ++level) {
      ke(cell,node,level) = 0;
      for (int dim=0; dim < numDims; ++dim) {
        ke(cell,node,level) += 0.5*u(cell,node,level,dim)*u(cell,node,level,dim);
      }
    }
  }
}

#endif

//**********************************************************************
template<typename EvalT, typename Traits>
void XZHydrostatic_KineticEnergy<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  for (int cell=0; cell < workset.numCells; ++cell) {
    for (int node=0; node < numNodes; ++node) {
      for (int level=0; level < numLevels; ++level) {
        ke(cell,node,level) = 0;
        for (int dim=0; dim < numDims; ++dim) {
          ke(cell,node,level) += 0.5*u(cell,node,level,dim)*u(cell,node,level,dim);
        }
      }
    }
  }

#else
  Kokkos::parallel_for(XZHydrostatic_KineticEnergy_Policy(0,workset.numCells),*this);

#endif
}

//**********************************************************************
template<typename EvalT,typename Traits>
typename XZHydrostatic_KineticEnergy<EvalT,Traits>::ScalarT& 
XZHydrostatic_KineticEnergy<EvalT,Traits>::getValue(const std::string &n)
{
  if (n=="KineticEnergy") return ke0;
  return ke0;
}

}
