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

#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Aeras_Layouts.hpp"
#include "Aeras_Dimension.hpp"

namespace Aeras {

//**********************************************************************
template<typename EvalT, typename Traits>
XZHydrostatic_VirtualT<EvalT, Traits>::
XZHydrostatic_VirtualT(const Teuchos::ParameterList& p,
              const Teuchos::RCP<Aeras::Layouts>& dl) :
  virt_t     (p.get<std::string> ("Virtual_Temperature"), dl->node_scalar_level),
  Cpstar     (p.get<std::string> ("Cpstar"),              dl->node_scalar_level),
  temperature(p.get<std::string> ("Temperature"),         dl->node_scalar_level),
  Pi         (p.get<std::string> ("Pi"),                  dl->node_scalar_level),
  tracerNames(p.get< Teuchos::ArrayRCP<std::string> >("Tracer Names")),
  numNodes   (dl->node_scalar             ->extent(1)),
  numLevels  (dl->node_scalar_level       ->extent(2)),
  Cp         (p.isParameter("XZHydrostatic Problem") ? 
                p.get<Teuchos::ParameterList*>("XZHydrostatic Problem")->get<double>("Cp", 1005.7):
                p.get<Teuchos::ParameterList*>("Hydrostatic Problem")->get<double>("Cp", 1005.7)),
  vapor      (false)
{
  Cpv = 1870.0; // (J/kgK)
  Cvv = 1410.0; // (J/kgK)
  R =287.0;
  Rv=461.5;
  factor = Rv/R - 1.0;
  //std::cout << "XZHydrostatic_Omega: Cp = " << Cp << std::endl;

  const Teuchos::ArrayRCP<std::string> RequiredTracers(1, "Vapor");
  for (int i=0; i<RequiredTracers.size(); ++i) {
    for (int j=0; j<tracerNames.size() && !vapor; ++j)
      if (RequiredTracers[i] == tracerNames[j]) vapor = true;
  }

  if (vapor) {
    qv = decltype(qv)("Vapor",   dl->node_scalar_level);
    this->addDependentField(qv);
  }

  this->addDependentField(temperature);
  this->addDependentField(Pi);
  
  this->addEvaluatedField(virt_t);
  this->addEvaluatedField(Cpstar);
  this->setName("Aeras::XZHydrostatic_VirtualT"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits>
void XZHydrostatic_VirtualT<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(virt_t,     fm);
  this->utils.setFieldData(Cpstar,     fm);
  this->utils.setFieldData(temperature,fm);
  this->utils.setFieldData(Pi,fm);
  if (vapor) this->utils.setFieldData(qv,fm);
  d.fill_field_dependencies(this->dependentFields(),this->evaluatedFields());
}

//**********************************************************************
// Kokkos kernels
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void XZHydrostatic_VirtualT<EvalT, Traits>::
operator() (const XZHydrostatic_VirtualT_Tag& tag, const int& cell) const{
  for (int node=0; node < numNodes; ++node) {
    for (int level=0; level < numLevels; ++level) {
      virt_t(cell,node,level) = temperature(cell,node,level);
      Cpstar(cell,node,level) = Cp;
    }
  }
}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void XZHydrostatic_VirtualT<EvalT, Traits>::
operator() (const XZHydrostatic_VirtualT_vapor_Tag& tag, const int& cell) const{
  for (int node=0; node < numNodes; ++node) {
    for (int level=0; level < numLevels; ++level) {
      virt_t(cell,node,level) = temperature(cell,node,level) 
                              + factor * temperature(cell,node,level)*qv(cell,node,level)/Pi(cell,node,level);
      Cpstar(cell,node,level) = Cp + (Cpv - Cp)*qv(cell,node,level)/Pi(cell,node,level);
    }
  }
}

#endif

//**********************************************************************
template<typename EvalT, typename Traits>
void XZHydrostatic_VirtualT<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  if (!vapor) {
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int node=0; node < numNodes; ++node) {
        for (int level=0; level < numLevels; ++level) {
          virt_t(cell,node,level) = temperature(cell,node,level);
          Cpstar(cell,node,level) = Cp;
        }
      }
    }
  } else { 
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int node=0; node < numNodes; ++node) {
        for (int level=0; level < numLevels; ++level) {
          virt_t(cell,node,level) = temperature(cell,node,level) 
                                  + factor * temperature(cell,node,level)*qv(cell,node,level)/Pi(cell,node,level);
          Cpstar(cell,node,level) = Cp + (Cpv - Cp)*qv(cell,node,level)/Pi(cell,node,level);
        }
      }
    }
  }

#else
  if (!vapor) {
    Kokkos::parallel_for(XZHydrostatic_VirtualT_Policy(0,workset.numCells),*this);
  } else { 
    Kokkos::parallel_for(XZHydrostatic_VirtualT_vapor_Policy(0,workset.numCells),*this);
  }

#endif
}
}
