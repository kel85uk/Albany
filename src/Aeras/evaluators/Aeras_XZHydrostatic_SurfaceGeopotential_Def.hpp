//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_RCP.hpp"
#include "Phalanx_DataLayout.hpp"
#include "PHAL_Utilities.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "Albany_Utils.hpp"

#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Aeras_Layouts.hpp"

#include "Aeras_Eta.hpp"
#include "Aeras_ShallowWaterConstants.hpp"

namespace Aeras {

//**********************************************************************
template<typename EvalT, typename Traits>
XZHydrostatic_SurfaceGeopotential<EvalT, Traits>::
XZHydrostatic_SurfaceGeopotential(const Teuchos::ParameterList& p,
              const Teuchos::RCP<Aeras::Layouts>& dl) :
  PhiSurf       (p.get<std::string> ("SurfaceGeopotential"), dl->node_scalar),
  coordVec      (p.get<std::string>  ("Coordinate Vector Name"), dl->vertices_vector ),
  numNodes ( dl->node_scalar          ->extent(1)),
  numParam(0)

{

  // Teuchos::ParameterList* xzhydrostatic_list = p.get<Teuchos::ParameterList*>("XZHydrostatic Problem");
  Teuchos::ParameterList* xzhydrostatic_list =
    p.isParameter("XZHydrostatic Problem") ?
    p.get<Teuchos::ParameterList*>("XZHydrostatic Problem"):
    p.get<Teuchos::ParameterList*>("Hydrostatic Problem");


  //std::string topoTypeString = surfGeopList->get<std::string>("Topography Type", "None");

  std::string topoTypeString = xzhydrostatic_list->get<std::string> ("Topography Type", "None");

  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());

  const bool invalidString = ( (topoTypeString != "None") && (topoTypeString != "Mountain1") && (topoTypeString != "SphereMountain1")
                         && (topoTypeString != "AspBaroclinic") );

  TEUCHOS_TEST_FOR_EXCEPTION( invalidString,
                             std::logic_error,
                             "Unknown topography type string of " << topoTypeString
                             << " encountered. " << std::endl);

  if (topoTypeString == "None"){
    topoType = NONE;
  }

  else if (topoTypeString == "Mountain1") {

    topoType = MOUNTAIN1;

    numParam = 3;

    Teuchos::Array<double> defaultData(numParam);

    defaultData[0] = 150.0;//center
    defaultData[1] = 50.0;//width
    defaultData[2] = 1000.0;//height

    topoData = xzhydrostatic_list->get("Topography Data", defaultData);

    //these will eventually be grabbed from a constants class
    //it is not clear if in XZHydro gravity is used
    local_pi = 3.141592653589793;
    local_gravity = 9.80616;
    center = topoData[0];
    width = topoData[1];
    height = topoData[2];

    TEUCHOS_TEST_FOR_EXCEPTION((topoData.size() != numParam),
                               std::logic_error,
                               "Error! Invalid specification of params for Mountain1: incorrect length of " <<
                               "Topography Data ; required numParam = " << numParam <<
                               ", provided data.size() = " << topoData.size()
                               <<  std::endl) ;
  }

  std::cout << "The topography type is " << topoTypeString << "\n";


  this->addEvaluatedField(PhiSurf);
  this->addDependentField(coordVec);

  this->setName("Aeras::XZHydrostatic_SurfaceGeopotential"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits>
void XZHydrostatic_SurfaceGeopotential<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(PhiSurf      , fm);
  this->utils.setFieldData(coordVec      , fm);
  d.fill_field_dependencies(this->dependentFields(),this->evaluatedFields());
}

//**********************************************************************
// Kokkos kernels
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void XZHydrostatic_SurfaceGeopotential<EvalT, Traits>::
operator() (const XZHydrostatic_SurfaceGeopotential_MOUNTAIN1_Tag& tag, const int& cell) const{
  for (int node=0; node < numNodes; ++node) {
    const MeshScalarT xcoord = coordVec(cell,node,0);
    if (std::abs(Albany::ADValue(xcoord) - center) <= (width/2.)) {
      PhiSurf(cell,node) =
             (std::cos( (xcoord - center)*local_pi*2./width ) + 1.)
             *height/2. ;//*local_gravity;
    }
    else
      PhiSurf(cell,node) = 0.0;
  }
}

#endif

//**********************************************************************
template<typename EvalT, typename Traits>
void XZHydrostatic_SurfaceGeopotential<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  if (topoType == NONE){
    PHAL::set(PhiSurf, 0.0);
    /*
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int node=0; node < numNodes; ++node) {

        //How to get x coordinate:
        //workset.wsCoords[cell][node][0]
        PhiSurf(cell,node) = 0.0;

        //std::cout << "topotype = none" <<std::endl;

        //std::cout << "cell="<<cell<<", node="<<node<<", coord x="<<
        //workset.wsCoords[cell][node][0] << std::endl;

      }
    }
    */
  }

  else if (topoType == MOUNTAIN1) {
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int node=0; node < numNodes; ++node) {

        //How to get x coordinate:
        //workset.wsCoords[cell][node][0]

        double xcoord = workset.wsCoords[cell][node][0];
        if (std::abs(xcoord - center) <= (width/2.)) {
          PhiSurf(cell,node) =
                 (std::cos( (xcoord - center)*local_pi*2./width ) + 1.)
                 *height/2. ;//*local_gravity;
        }
        else
          PhiSurf(cell,node) = 0.0;

        //std::cout << "topotype = mountain1"<<std::endl;
        //std::cout << "center width height"<<center<<" "<<width<<" "<<height<<std::endl;
        //std::cout << "cell="<<cell<<", node="<<node<<", coord x="<<
        //xcoord <<" and f_s="<<PhiSurf(cell,node) << std::endl;

      }
    }
  }

#else
  if (topoType == NONE){
    PHAL::set(PhiSurf, 0.0);
  }

  else if (topoType == MOUNTAIN1) {
    Kokkos::parallel_for(XZHydrostatic_SurfaceGeopotential_MOUNTAIN1_Policy(0,workset.numCells),*this);
    cudaCheckError();
  }

#endif
}
}
