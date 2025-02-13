//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef LANDICE_BASAL_FRICTION_COEFFICIENT_GRADIENT_HPP
#define LANDICE_BASAL_FRICTION_COEFFICIENT_GRADIENT_HPP 1

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"
#include "Albany_ScalarOrdinalTypes.hpp"
#include "PHAL_Dimension.hpp"

namespace LandIce
{

/** \brief Basal friction coefficient evaluator

    This evaluator computes the friction coefficient beta for basal natural BC

*/

template<typename EvalT, typename Traits>
class BasalFrictionCoefficientGradient : public PHX::EvaluatorWithBaseImpl<Traits>,
                                         public PHX::EvaluatorDerived<EvalT, Traits>
{
public:
  typedef typename EvalT::ScalarT ScalarT;

  BasalFrictionCoefficientGradient (const Teuchos::ParameterList& p,
                                    const Teuchos::RCP<Albany::Layouts>& dl);

  virtual ~BasalFrictionCoefficientGradient () {}

  void postRegistrationSetup (typename Traits::SetupData,
                              PHX::FieldManager<Traits>&) {}

  void evaluateFields (typename Traits::EvalData d);

  typedef typename PHX::Device execution_space;

private:

  void computeEffectivePressure (int cell, int side);

  typedef typename EvalT::MeshScalarT   MeshScalarT;
  typedef typename EvalT::ParamScalarT  ParamScalarT;

  // Input:
  PHX::MDField<const RealType,Cell,Side,Node>                   given_field;
  PHX::MDField<const ParamScalarT,Cell,Side,Node>               given_field_param;
  PHX::MDField<const MeshScalarT,Cell,Side,Node,QuadPoint,Dim>  GradBF;
  PHX::MDField<const ParamScalarT,Cell,Side,QuadPoint>          N;
  PHX::MDField<const ScalarT,Cell,Side,QuadPoint,Dim>           U;
  PHX::MDField<const ScalarT,Cell,Side,QuadPoint,Dim>           gradN;
  PHX::MDField<const ScalarT,Cell,Side,QuadPoint,Dim,Dim>       gradU;
  PHX::MDField<const ScalarT,Cell,Side,QuadPoint>               u_norm;
  PHX::MDField<const MeshScalarT,Cell,Side,QuadPoint,Dim>       coordVec;

  PHX::MDField<const ScalarT,Dim>                               lambdaParam;
  PHX::MDField<const ScalarT,Dim>                               muParam;
  PHX::MDField<const ScalarT,Dim>                               powerParam;

  // Output:
  PHX::MDField<ScalarT,Cell,Side,QuadPoint,Dim>           grad_beta;

  std::string basalSideName;

  int numSideNodes;
  int numSideQPs;
  int sideDim;
  int vecDim;

  double A;
  double x_0;
  double y_0;
  double R2;

  bool use_stereographic_map;
  bool is_given_field_param;

  enum BETA_TYPE {INVALID, GIVEN_CONSTANT, GIVEN_FIELD, REGULARIZED_COULOMB};
  BETA_TYPE beta_type;
};

} // Namespace LandIce

#endif // LANDICE_BASAL_FRICTION_COEFFICIENT_GRADIENT_HPP
