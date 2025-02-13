/*
 * LandIce_BasalMeltRate.hpp
 *
 *  Created on: Jun 16, 2016
 *      Author: abarone
 */

#ifndef LANDICE_BASALMELT_RATE_HPP
#define LANDICE_BASALMELT_RATE_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"
#include "PHAL_Dimension.hpp"

namespace LandIce
{

template<typename EvalT, typename Traits, typename VelocityST, typename MeltEnthST>
class BasalMeltRate : public PHX::EvaluatorWithBaseImpl<Traits>,
                      public PHX::EvaluatorDerived<EvalT, Traits>
{
public:

  BasalMeltRate(const Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl_basal);

  void postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  typedef typename EvalT::ParamScalarT ParamScalarT;

  // Input:
  PHX::MDField<const ScalarT,Cell,Side,Node>              phi;  // []
  PHX::MDField<const VelocityST,Cell,Side,Node>    	      beta;  // [kPa yr/m]
  PHX::MDField<const VelocityST,Cell,Side,Node,VecDim>    velocity; // [m/yr]
  PHX::MDField<const ParamScalarT,Cell,Side,Node>         geoFluxHeat; // [W m^{-2}] = [Pa m s^{-1}]
  PHX::MDField<const ScalarT,Cell,Side,Node> 	            Enthalpy; //[MW s m^{-3}]
  // PHX::MDField<const ScalarT,Cell,Side,Node>              basal_dTdz; // [K km^{-1}]
  PHX::MDField<const MeltEnthST,Cell,Side,Node>           EnthalpyHs; ////[MW s m^{-3}]
  PHX::MDField<const ScalarT,Dim>                         homotopy;

  // Output:
  PHX::MDField<ScalarT,Cell,Side,Node> basalMeltRate; // [W m^{-2}]
  PHX::MDField<ScalarT,Cell,Side,Node> basalVertVelocity; // [m/yr]

  std::vector<std::vector<int> >  sideNodes;
  std::string                     basalSideName;

  int numCellNodes, numSideNodes, sideDim;

  double rho_w; 	// [kg m^{-3}] density of water
  double rho_i; 	// [kg m^{-3}] density of ice
  double L;       //[J kg^{-1} ] Latent heat of fusion", 3e5);
  double g;       //[m s^{-2}], Gravity Acceleration
  double a;       // [adim], Diffusivity homotopy exponent

  double k_0;     //[m^2], Permeability factor
  double k_i;   //[W m^{-1} K^{-1}], Conductivity of ice
  double eta_w;   //[Pa s], Viscosity of water
  double alpha_om; //[adim], Omega exponent alpha
  double beta_p;

  double flux_reg_alpha;
  double flux_reg_beta;
  double basalMelt_reg_alpha;
  double basalMelt_reg_beta;
};

} // namespace LandIce

#endif // LANDICE_BASALMELT_RATE_HPP
