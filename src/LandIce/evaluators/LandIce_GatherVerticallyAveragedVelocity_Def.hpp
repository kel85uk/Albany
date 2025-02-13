//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Phalanx_TypeStrings.hpp"
#include "Sacado.hpp"

#include "Albany_ThyraUtils.hpp"
#include "Albany_AbstractDiscretization.hpp"

#include "LandIce_GatherVerticallyAveragedVelocity.hpp"

//uncomment the following line if you want debug output to be printed to screen
//#define OUTPUT_TO_SCREEN

namespace LandIce {

//**********************************************************************

template<typename EvalT, typename Traits>
GatherVerticallyAveragedVelocityBase<EvalT, Traits>::
GatherVerticallyAveragedVelocityBase(const Teuchos::ParameterList& p,
                  const Teuchos::RCP<Albany::Layouts>& dl)
{
  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());
  cell_topo = p.get<Teuchos::RCP<const CellTopologyData> >("Cell Topology");

  std::vector<PHX::DataLayout::size_type> dims;

  dl->node_vector->dimensions(dims);
  numNodes = dims[1];
  vecDimFO = dims[2];

  meshPart = p.get<std::string>("Mesh Part");

  std::string sideSetName  = p.get<std::string> ("Side Set Name");
  TEUCHOS_TEST_FOR_EXCEPTION (dl->side_layouts.find(sideSetName)==dl->side_layouts.end(), std::runtime_error,
                              "Error! Layout for side set " << sideSetName << " not found.\n");
  Teuchos::RCP<Albany::Layouts> dl_side = dl->side_layouts.at(sideSetName);

  averagedVel = decltype(averagedVel)(p.get<std::string>("Averaged Velocity Name"), dl_side->node_vector);
  this->addEvaluatedField(averagedVel);

  this->setName("GatherVerticallyAveragedVelocity"+PHX::typeAsString<EvalT>());
}

//**********************************************************************

template<typename EvalT, typename Traits>
void GatherVerticallyAveragedVelocityBase<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData /* d */,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(averagedVel,fm);
}

//**********************************************************************

template<typename Traits>
GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::Residual, Traits>::
GatherVerticallyAveragedVelocity(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyAveragedVelocityBase<PHAL::AlbanyTraits::Residual, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);

  Kokkos::deep_copy(this->averagedVel.get_view(), ScalarT(0.0));

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSets.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::SideSetList& ssList = *(workset.sideSets);
  Albany::SideSetList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const std::vector<Albany::SideStruct>& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<LO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");

    const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
    int numLayers = layeredMeshNumbering.numLayers;

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1); //doing trapezoidal rule
    quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
    for(int i=1; i<numLayers; ++i) {
      quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (std::size_t iSide = 0; iSide < sideSet.size(); ++iSide) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet[iSide].elem_LID;
      const int elem_side = sideSet[iSide].side_local_id;
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      LO baseId, ilayer;
      for (int i = 0; i < numSideNodes; ++i) {
        std::size_t node = side.node[i];
        LO lnodeId = Albany::getLocalElement(workset.disc->getOverlapNodeVectorSpace(),elNodeID[node]);
        layeredMeshNumbering.getIndices(lnodeId, baseId, ilayer);
        std::vector<double> avVel(this->vecDimFO,0);
        for(int il=0; il<numLayers+1; ++il) {
          LO inode = layeredMeshNumbering.getId(baseId, il);
          for(int comp=0; comp<this->vecDimFO; ++comp)
            avVel[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp)]*quadWeights[il];
        }
        for(int comp=0; comp<this->vecDimFO; ++comp) {
          this->averagedVel(elem_LID,elem_side,i,comp) = avVel[comp];
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::Jacobian, Traits>::
GatherVerticallyAveragedVelocity(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyAveragedVelocityBase<PHAL::AlbanyTraits::Jacobian, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);
  
  int neq = workset.wsElNodeEqID.extent(2);

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSets.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::LayeredMeshNumbering<LO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
  int numLayers = layeredMeshNumbering.numLayers;

  Kokkos::deep_copy(this->averagedVel.get_view(), ScalarT(0.0));

  const Albany::SideSetList& ssList = *(workset.sideSets);
  Albany::SideSetList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const std::vector<Albany::SideStruct>& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");

    const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1); //doing trapezoidal rule

    quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
    for(int i=1; i<numLayers; ++i) {
      quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (std::size_t iSide = 0; iSide < sideSet.size(); ++iSide) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet[iSide].elem_LID;
      const int elem_side = sideSet[iSide].side_local_id;
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];
      std::vector<double> velx(this->numNodes,0), vely(this->numNodes,0);

      LO baseId, ilayer;
      for (int i = 0; i < numSideNodes; ++i) {
        std::size_t node = side.node[i];
        LO lnodeId = Albany::getLocalElement(workset.disc->getOverlapNodeVectorSpace(),elNodeID[node]);
        layeredMeshNumbering.getIndices(lnodeId, baseId, ilayer);
        std::vector<double> avVel(this->vecDimFO,0);
        for(int il=0; il<numLayers+1; ++il) {
          LO inode = layeredMeshNumbering.getId(baseId, il);
          for(int comp=0; comp<this->vecDimFO; ++comp)
            avVel[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp)]*quadWeights[il];
        }

        for(int comp=0; comp<this->vecDimFO; ++comp) {
          this->averagedVel(elem_LID,elem_side,i,comp) = FadType(this->averagedVel(elem_LID,elem_side,i,comp).size(), avVel[comp]);
          for(int il=0; il<numLayers+1; ++il)
            this->averagedVel(elem_LID,elem_side,i,comp).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+comp) = quadWeights[il]*workset.j_coeff;
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::Tangent, Traits>::
GatherVerticallyAveragedVelocity(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyAveragedVelocityBase<PHAL::AlbanyTraits::Tangent, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::Tangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);

  Kokkos::deep_copy(this->averagedVel.get_view(), ScalarT(0.0));

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSets.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::SideSetList& ssList = *(workset.sideSets);
  Albany::SideSetList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const std::vector<Albany::SideStruct>& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<LO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");

    const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
    int numLayers = layeredMeshNumbering.numLayers;

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1); //doing trapezoidal rule
    quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
    for(int i=1; i<numLayers; ++i) {
      quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (std::size_t iSide = 0; iSide < sideSet.size(); ++iSide) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet[iSide].elem_LID;
      const int elem_side = sideSet[iSide].side_local_id;
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      LO baseId, ilayer;
      for (int i = 0; i < numSideNodes; ++i) {
        std::size_t node = side.node[i];
        LO lnodeId = Albany::getLocalElement(workset.disc->getOverlapNodeVectorSpace(),elNodeID[node]);
        layeredMeshNumbering.getIndices(lnodeId, baseId, ilayer);
        std::vector<double> avVel(this->vecDimFO,0);
        for(int il=0; il<numLayers+1; ++il) {
          LO inode = layeredMeshNumbering.getId(baseId, il);
          for(int comp=0; comp<this->vecDimFO; ++comp)
            avVel[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp)]*quadWeights[il];
        }
        for(int comp=0; comp<this->vecDimFO; ++comp) {
          this->averagedVel(elem_LID,elem_side,i,comp) = avVel[comp];
          if (workset.Vx != Teuchos::null && workset.j_coeff != 0.0) {
            TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Not Implemented yet" << std::endl);
          }
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
GatherVerticallyAveragedVelocity(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyAveragedVelocityBase<PHAL::AlbanyTraits::DistParamDeriv, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyAveragedVelocity<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);

  Kokkos::deep_copy(this->averagedVel.get_view(), ScalarT(0.0));

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSets.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::SideSetList& ssList = *(workset.sideSets);
  Albany::SideSetList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const std::vector<Albany::SideStruct>& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<LO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");

    const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
    int numLayers = layeredMeshNumbering.numLayers;

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1); //doing trapezoidal rule
    quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
    for(int i=1; i<numLayers; ++i) {
      quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (std::size_t iSide = 0; iSide < sideSet.size(); ++iSide) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet[iSide].elem_LID;
      const int elem_side = sideSet[iSide].side_local_id;
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      LO baseId, ilayer;
      for (int i = 0; i < numSideNodes; ++i) {
        std::size_t node = side.node[i];
        LO lnodeId = Albany::getLocalElement(workset.disc->getOverlapNodeVectorSpace(),elNodeID[node]);
        layeredMeshNumbering.getIndices(lnodeId, baseId, ilayer);
        std::vector<double> avVel(this->vecDimFO,0);
        for(int il=0; il<numLayers+1; ++il) {
          LO inode = layeredMeshNumbering.getId(baseId, il);
          for(int comp=0; comp<this->vecDimFO; ++comp)
            avVel[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp)]*quadWeights[il];
        }
        for(int comp=0; comp<this->vecDimFO; ++comp) {
          this->averagedVel(elem_LID,elem_side,i,comp) = avVel[comp];
        }
      }
    }
  }
}

} // namespace LandIce
