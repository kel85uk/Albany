//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef AERAS_COMPUTE_BASIS_FUNCTIONS_HPP
#define AERAS_COMPUTE_BASIS_FUNCTIONS_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Intrepid2_CellTools.hpp"
#include "Intrepid2_Cubature.hpp"

#include "Albany_ScalarOrdinalTypes.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_Utilities.hpp"
#include "Aeras_Layouts.hpp"

namespace Albany { class StateManager; }

namespace Aeras {

/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates nodal DOF values to quad points.

*/

template<typename EvalT, typename Traits>
class ComputeBasisFunctions : public PHX::EvaluatorWithBaseImpl<Traits>,
 			                        public PHX::EvaluatorDerived<EvalT, Traits>  {
public:

  ComputeBasisFunctions(const Teuchos::ParameterList& p,
                        const Teuchos::RCP<Aeras::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                             PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  const int spatialDim;
  int basisDim;
  int numelements;
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  int  numVertices, numDims, numNodes, numQPs;

  // Input:
  //! Coordinate vector at vertices
  PHX::MDField<const MeshScalarT,Cell,Vertex,Dim> coordVec;
  Teuchos::RCP<Intrepid2::Cubature<PHX::Device> > cubature;
  Teuchos::RCP<Intrepid2::Basis<PHX::Device, RealType, RealType> > intrepidBasis;
  Teuchos::RCP<shards::CellTopology> cellType;

  // Temporary Views
  //PHX::MDField<RealType,Node,QuadPoint>    val_at_cub_points;
  Kokkos::DynRankView<RealType, PHX::Device>    val_at_cub_points;
  Kokkos::DynRankView<RealType, PHX::Device>    grad_at_cub_points;
  Kokkos::DynRankView<RealType, PHX::Device>    D2_at_cub_points;
  Kokkos::DynRankView<RealType, PHX::Device>    refPoints;
  Kokkos::DynRankView<RealType, PHX::Device>    refWeights;

  // Output:
  //! Basis Functions at quadrature points
  PHX::MDField<MeshScalarT,Cell,QuadPoint> weighted_measure;
  PHX::MDField<RealType,Cell,Node,QuadPoint> BF;
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>   sphere_coord; 
  PHX::MDField<MeshScalarT,Cell,Node> lambda_nodal;
  PHX::MDField<MeshScalarT,Cell,Node> theta_nodal;
  PHX::MDField<MeshScalarT,Cell,QuadPoint>     jacobian_det; 
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim,Dim> jacobian_inv;
  PHX::MDField<MeshScalarT,Cell,Node,Dim,Dim> jacobian_inv_node;
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim,Dim> jacobian;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> GradBF;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;
         
  const double earthRadius;
  void div_check(const int spatialDim, const int numelements) const;
  void spherical_divergence(Kokkos::DynRankView<MeshScalarT, PHX::Device> &,
                            const Kokkos::DynRankView<MeshScalarT, PHX::Device> &,
                            const int e,
                            const double rrearth=1) const;
  void initialize_grad(Kokkos::DynRankView<RealType, PHX::Device> &) const;

  PHAL::MDFieldMemoizer<Traits> memoizer;

  // Kokkos
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
public:
  
  Kokkos::View<RealType**, PHX::Device> val_at_cub_points_CUDA;
  Kokkos::View<RealType***, PHX::Device> grad_at_cub_points_CUDA;

  double pi;
  double DIST_THRESHOLD;

  typedef Kokkos::View<int***, PHX::Device>::execution_space ExecutionSpace;
  typedef PHX::KokkosViewFactory<MeshScalarT,PHX::Device> ViewFactory;
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim> Phi;
  PHX::MDField<MeshScalarT, Cell, QuadPoint> Norm;
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> dPhi;
  PHX::MDField<MeshScalarT, Cell, QuadPoint> SinL;
  PHX::MDField<MeshScalarT, Cell, QuadPoint> CosL;
  PHX::MDField<MeshScalarT, Cell, QuadPoint> SinT;
  PHX::MDField<MeshScalarT, Cell, QuadPoint> CosT;
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> DD1;
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> DD2;
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim, Dim> DD3;

  struct ComputeBasisFunctions_Tag{};

  typedef Kokkos::RangePolicy<ExecutionSpace, ComputeBasisFunctions_Tag> ComputeBasisFunctions_Policy;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ComputeBasisFunctions_Tag& tag, const int& i) const;

  KOKKOS_INLINE_FUNCTION
  void compute_jacobian (const int cell) const;
  
  KOKKOS_INLINE_FUNCTION
  void compute_phi_and_norm (const int cell) const;
  
  KOKKOS_INLINE_FUNCTION
  void compute_dphi (const int cell) const;
  
  KOKKOS_INLINE_FUNCTION
  void compute_sphere_coord (const int cell) const;
  
  KOKKOS_INLINE_FUNCTION
  void compute_lambda_and_theta_nodal (const int cell) const;
#endif // ALBANY_KOKKOS_UNDER_DEVELOPMENT
};

} // namespace Aeras

#endif // AERAS_COMPUTE_BASIS_FUNCTIONS_HPP
