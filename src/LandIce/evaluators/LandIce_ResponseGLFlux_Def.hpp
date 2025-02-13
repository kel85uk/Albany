//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "PHAL_Utilities.hpp"

template<typename EvalT, typename Traits>
LandIce::ResponseGLFlux<EvalT, Traits>::
ResponseGLFlux(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl)
{
  // get and validate Response parameter list
  Teuchos::ParameterList* plist = p.get<Teuchos::ParameterList*>("Parameter List");
  Teuchos::RCP<Teuchos::ParameterList> paramList = p.get<Teuchos::RCP<Teuchos::ParameterList> >("Parameters From Problem");
  Teuchos::RCP<ParamLib> paramLib = paramList->get< Teuchos::RCP<ParamLib> > ("Parameter Library");
  scaling = plist->get<double>("Scaling Coefficient", 1.0);

  basalSideName = paramList->get<std::string> ("Basal Side Name");

  const std::string& avg_vel_name     = paramList->get<std::string>("Averaged Vertical Velocity Side Variable Name");
  const std::string& thickness_name   = paramList->get<std::string>("Thickness Side Variable Name");
  const std::string& bed_name         = paramList->get<std::string>("Bed Topography Side Variable Name");
  const std::string& coords_name      = paramList->get<std::string>("Coordinate Vector Side Variable Name");

  TEUCHOS_TEST_FOR_EXCEPTION (dl->side_layouts.find(basalSideName)==dl->side_layouts.end(), std::runtime_error,
                              "Error! Basal side data layout not found.\n");

  Teuchos::RCP<Albany::Layouts> dl_basal = dl->side_layouts.at(basalSideName);

  avg_vel        = decltype(avg_vel)(avg_vel_name, dl_basal->node_vector);
  thickness      = decltype(thickness)(thickness_name, dl_basal->node_scalar);
  bed            = decltype(bed)(bed_name, dl_basal->node_scalar);
  coords         = decltype(coords)(coords_name, dl_basal->vertices_vector);

  cell_topo = paramList->get<Teuchos::RCP<const CellTopologyData> >("Cell Topology");
  Teuchos::RCP<const Teuchos::ParameterList> reflist = this->getValidResponseParameters();
  plist->validateParameters(*reflist, 0);

  // Get Dimensions
  numSideNodes = dl_basal->node_scalar->extent(2);
  numSideDims  = dl_basal->vertices_vector->extent(3);

  // add dependent fields
  this->addDependentField(avg_vel);
  this->addDependentField(thickness);
  this->addDependentField(bed);
  this->addDependentField(coords);

  this->setName("Response Grounding Line Flux" + PHX::typeAsString<EvalT>());

  using PHX::MDALayout;

  rho_w = paramList->sublist("LandIce Physical Parameters List",true).get<double>("Water Density");
  rho_i = paramList->sublist("LandIce Physical Parameters List",true).get<double>("Ice Density");

  // Setup scatter evaluator
  p.set("Stand-alone Evaluator", false);
  std::string local_response_name = "Local Response GL Flux";
  std::string global_response_name = "Global Response GL Flux";
  int worksetSize = dl_basal->node_scalar->extent(0);
  int responseSize = 1;
  auto local_response_layout = Teuchos::rcp(new MDALayout<Cell, Dim>(worksetSize, responseSize));
  auto global_response_layout = Teuchos::rcp(new MDALayout<Dim>(responseSize));
  PHX::Tag<ScalarT> local_response_tag(local_response_name, local_response_layout);
  PHX::Tag<ScalarT> global_response_tag(global_response_name, global_response_layout);
  p.set("Local Response Field Tag", local_response_tag);
  p.set("Global Response Field Tag", global_response_tag);
  PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::setup(p, dl);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void LandIce::ResponseGLFlux<EvalT, Traits>::postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
{
  PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::postRegistrationSetup(d, fm);
  gl_func = Kokkos::createDynRankView(bed.get_view(), "gl_func", numSideNodes);
  H = Kokkos::createDynRankView(bed.get_view(), "H", 2);
  x = Kokkos::createDynRankView(bed.get_view(), "x", 2);
  y = Kokkos::createDynRankView(bed.get_view(), "y", 2);
  velx = Kokkos::createDynRankView(avg_vel.get_view(), "velx", 2);
  vely = Kokkos::createDynRankView(avg_vel.get_view(), "vely", 2);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void LandIce::ResponseGLFlux<EvalT, Traits>::preEvaluate(typename Traits::PreEvalData workset) {
  PHAL::set(this->global_response_eval, 0.0);


  // Do global initialization
  PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::preEvaluate(workset);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void LandIce::ResponseGLFlux<EvalT, Traits>::evaluateFields(typename Traits::EvalData workset)
{
  if (workset.sideSets == Teuchos::null)
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Side sets defined in input file but not properly specified on the mesh" << std::endl);

  // Zero out local response
  PHAL::set(this->local_response_eval, 0.0);

  if (workset.sideSets->find(basalSideName) != workset.sideSets->end())
  {
    double coeff = rho_i*1e6*scaling; //to convert volume flux [km^2 m yr^{-1}] in a mass flux [kg yr^{-1}]
    const std::vector<Albany::SideStruct>& sideSet = workset.sideSets->at(basalSideName);
    for (auto const& it_side : sideSet)
    {
      // Get the local data of side and cell
      const int cell = it_side.elem_LID;
      const int side = it_side.side_local_id;

      for (int inode=0; inode<numSideNodes; ++inode)
        gl_func(inode) = rho_i*thickness(cell,side,inode)+rho_w*bed(cell,side,inode);

      bool isGLCell = false;

      for (int inode=1; inode<numSideNodes; ++inode)
        isGLCell = isGLCell || (gl_func(0)*gl_func(inode) <=0);

      if(!isGLCell)
        continue;

      int node_plus, node_minus;
      bool skip_edge = false, edge_on_GL=false;
      MeshScalarT gl_sum=0, gl_max=0, gl_min=0;

      int counter=0;
      for (int inode=0; (inode<numSideNodes); ++inode) {
        int inode1 = (inode+1)%numSideNodes;
        MeshScalarT gl0 = gl_func(inode), gl1 = gl_func(inode1);
        if(gl0 >= gl_max) {
          node_plus = inode;
          gl_max = gl0;
        }
        if(gl0 <= gl_min) {
          node_minus = inode;
          gl_min = gl0;
        }
        if((gl0 == 0) && (gl1 == 0)) {edge_on_GL = true; continue;}
        gl_sum += gl0; //needed to know whether the element is floating or grounded when GL is exactly on an edge of the element
        if((gl0*gl1 <= 0) && (counter <2)) {
          //we want to avoid selecting two edges sharing the same vertex on the GL
          if(skip_edge) {skip_edge = false; continue;}
          skip_edge = (gl1 == 0);
          MeshScalarT theta = gl0/(gl0-gl1);
          H(counter) = thickness(cell,side,inode1)*theta + thickness(cell,side,inode)*(1-theta);
          x(counter) = coords(cell,side,inode1,0)*theta + coords(cell,side,inode,0)*(1-theta);
          y(counter) = coords(cell,side,inode1,1)*theta + coords(cell,side,inode,1)*(1-theta);
          velx(counter) = avg_vel(cell,side,inode1,0)*theta + avg_vel(cell,side,inode,0)*(1-theta);
          vely(counter) = avg_vel(cell,side,inode1,1)*theta + avg_vel(cell,side,inode,1)*(1-theta);
          ++counter;
        }
      }

      //skip when a grounding line intersect the element in one vertex only (counter<1)
      //also, when an edge is on grounding line, consider only the grounded element to avoid double-counting.
      if(counter<2 || (edge_on_GL && gl_sum<0)) continue;

      //we consider the direction [(y[1]-y[0]), -(x[1]-x[0])] orthogonal to the GL segment and compute the flux along that direction.
      //we then compute the sign of the of the flux by looking at the sign of the dot-product between the GL segment and an edge crossed by the grounding line
      ScalarT t = 0.5*((H(0)*velx(0)+H(1)*velx(1))*(y(1)-y(0))-(H(0)*vely(0)+H(1)*vely(1))*(x(1)-x(0)));
      bool positive_sign = (y[1]-y[0])*(coords(cell,side,node_minus,0)-coords(cell,side,node_plus,0))-(x[1]-x[0])*(coords(cell,side,node_minus,1)-coords(cell,side,node_plus,1)) > 0;
      if(!positive_sign) t = -t;

      this->local_response_eval(cell, 0) += t*coeff;
      this->global_response_eval(0) += t*coeff;
    }
  }

  // Do any local-scattering necessary
  PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::evaluateFields(workset);
  PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::evaluate2DFieldsDerivativesDueToExtrudedSolution(workset,basalSideName, cell_topo);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void LandIce::ResponseGLFlux<EvalT, Traits>::postEvaluate(typename Traits::PostEvalData workset) {
  //amb Deal with op[], pointers, and reduceAll.
  PHAL::reduceAll<ScalarT>(*workset.comm, Teuchos::REDUCE_SUM,
                           this->global_response_eval);

  // Do global scattering
  PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::postEvaluate(workset);
}

// **********************************************************************
template<typename EvalT, typename Traits>
Teuchos::RCP<const Teuchos::ParameterList> LandIce::ResponseGLFlux<EvalT, Traits>::getValidResponseParameters() const {
  Teuchos::RCP<Teuchos::ParameterList> validPL = rcp(new Teuchos::ParameterList("Valid ResponseGLFlux Params"));
  Teuchos::RCP<const Teuchos::ParameterList> baseValidPL = PHAL::SeparableScatterScalarResponseWithExtrudedParams<EvalT, Traits>::getValidResponseParameters();
  validPL->setParameters(*baseValidPL);

  validPL->set<std::string>("Name", "", "Name of response function");
  validPL->set<double>("Scaling Coefficient", 1.0, "Coefficient that scales the response");
  validPL->set<Teuchos::RCP<const CellTopologyData> >("Cell Topology",Teuchos::RCP<const CellTopologyData>(),"Cell Topology Data");
  validPL->set<int>("Cubature Degree", 3, "degree of cubature used to compute the velocity mismatch");
  validPL->set<int>("Phalanx Graph Visualization Detail", 0, "Make dot file to visualize phalanx graph");
  validPL->set<std::string>("Description", "", "Description of this response used by post processors");
  validPL->set<std::string> ("Basal Side Name", "", "Name of the side set corresponding to the ice-bedrock interface");

  return validPL;
}
// **********************************************************************

