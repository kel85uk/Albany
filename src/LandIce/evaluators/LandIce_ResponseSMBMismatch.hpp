//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef LANDICE_RESPONSE_SMB_MISMATCH_HPP
#define LANDICE_RESPONSE_SMB_MISMATCH_HPP

#include "PHAL_SeparableScatterScalarResponse.hpp"
#include "Intrepid2_CellTools.hpp"
#include "Intrepid2_Cubature.hpp"

namespace LandIce {
/**
 * \brief Response Description
 */
  template<typename EvalT, typename Traits, typename ThicknessScalarType>
  class ResponseSMBMismatch :
    public PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT,Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;
    typedef typename EvalT::ParamScalarT ParamScalarT;

    ResponseSMBMismatch(Teuchos::ParameterList& p,
       const Teuchos::RCP<Albany::Layouts>& dl);

    void postRegistrationSetup(typename Traits::SetupData d,
             PHX::FieldManager<Traits>& vm);

    void preEvaluate(typename Traits::PreEvalData d);

    void evaluateFields(typename Traits::EvalData d);

    void postEvaluate(typename Traits::PostEvalData d);

  private:
    Teuchos::RCP<const Teuchos::ParameterList> getValidResponseParameters() const;

    std::string surfaceSideName;
    std::string basalSideName;

    int numSideNodes;
    int numBasalQPs;
    int numSideDims;

    PHX::MDField<const ScalarT,Cell,Side,QuadPoint>                   flux_div;
    PHX::MDField<const RealType,Cell,Side,QuadPoint>                  SMB;
    PHX::MDField<const RealType,Cell,Side,QuadPoint>                  SMBRMS;
    PHX::MDField<const RealType,Cell,Side,QuadPoint>                  obs_thickness;
    PHX::MDField<const RealType,Cell,Side,QuadPoint>                  thicknessRMS;
    PHX::MDField<const ThicknessScalarType,Cell,Side,QuadPoint>       thickness;
    PHX::MDField<const ThicknessScalarType,Cell,Side,QuadPoint,Dim>   grad_thickness;
    PHX::MDField<const MeshScalarT,Cell,Side,QuadPoint>               w_measure_2d;
    PHX::MDField<const MeshScalarT,Cell,Side,QuadPoint,Dim,Dim>       tangents;

    ScalarT p_resp, p_reg, resp, reg, p_misH, misH;
    double scaling, alpha, alphaH, alphaSMB, asinh_scaling;

    Teuchos::RCP<const CellTopologyData> cell_topo;
  };

} // namespace LandIce

#endif // LANDICE_RESPONSE_SMB_MISMATCH_HPP
