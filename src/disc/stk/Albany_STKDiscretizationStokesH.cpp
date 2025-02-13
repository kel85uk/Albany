//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <limits>

#include "Albany_Utils.hpp"
#include "Albany_STKDiscretizationStokesH.hpp"
#include "Albany_NodalGraphUtils.hpp"
#include "Albany_STKNodeFieldContainer.hpp"
#include "Albany_BucketArray.hpp"

#include <string>
#include <iostream>
#include <fstream>

#include <Shards_BasicTopologies.hpp>

#include <Intrepid2_CellTools.hpp>
#include <Intrepid2_Basis.hpp>

#include <stk_util/parallel/Parallel.hpp>

#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/base/Selector.hpp>

#include <PHAL_Dimension.hpp>

#include <stk_mesh/base/FEMHelpers.hpp>

namespace Albany
{

STKDiscretizationStokesH::
STKDiscretizationStokesH(const Teuchos::RCP<Teuchos::ParameterList>& discParams,
                         Teuchos::RCP<AbstractSTKMeshStruct>& stkMeshStruct_,
                         const Teuchos::RCP<const Teuchos_Comm>& comm_,
                         const Teuchos::RCP<RigidBodyModes>& rigidBodyModes_)
 : STKDiscretization(discParams, stkMeshStruct_,  comm_, rigidBodyModes_)
{
  // Nothing to do here
}

void STKDiscretizationStokesH::computeGraphs()
{
  std::map<int, stk::mesh::Part*>::iterator pv = stkMeshStruct->partVec.begin();
  int nodes_per_element =  metaData.get_cell_topology(*(pv->second)).getNodeCount();

  //super bad hack based on current LandIce probelms.. make this general!!
  unsigned int n3dEq = (neq == 4) ? 4 : 2;
  n3dEq = std::min(n3dEq,neq);

  m_overlap_jac_factory = Teuchos::rcp(new ThyraCrsMatrixFactory(m_overlap_vs,m_overlap_vs,neq*nodes_per_element));

  stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector( metaData.universal_part() ) &
    stk::mesh::Selector( metaData.locally_owned_part() );

  stk::mesh::get_selected_entities( select_owned_in_part ,
            bulkData.buckets( stk::topology::ELEMENT_RANK ) ,
            cells );

  if (comm->getRank()==0) {
    *out << "STKDisc: " << cells.size() << " elements on Proc 0 " << std::endl;
  }

  const LayeredMeshNumbering<LO>& layeredMeshNumbering = *getLayeredMeshNumbering();
  int numLayers = layeredMeshNumbering.numLayers;

  GO row, col;
  for (std::size_t i=0; i < cells.size(); i++) {
    stk::mesh::Entity e = cells[i];
    stk::mesh::Entity const* node_rels = bulkData.begin_nodes(e);
    const size_t num_nodes = bulkData.num_nodes(e);

    // loop over local nodes
    for (std::size_t j=0; j < num_nodes; j++) {
      stk::mesh::Entity rowNode = node_rels[j];

      // loop over eqs
      for (std::size_t k=0; k < n3dEq; k++) {
        row = this->getGlobalDOF(this->gid(rowNode), k);
        for (std::size_t l=0; l < num_nodes; l++) {
          stk::mesh::Entity colNode = node_rels[l];
          for (std::size_t m=0; m < n3dEq; m++) {
            col = this->getGlobalDOF(this->gid(colNode), m);
            m_overlap_jac_factory->insertGlobalIndices(row, Teuchos::arrayView(&col, 1));
          }
        }
      }

      if(neq > n3dEq)
      {
        row = this->getGlobalDOF(this->gid(rowNode), n3dEq);
        GO node_gid = this->gid(rowNode);
        LO base_id, ilayer;
        int node_lid = getLocalElement(m_overlap_node_vs,node_gid);
        layeredMeshNumbering.getIndices(node_lid, base_id, ilayer);
        if(ilayer == 0) {
          for (std::size_t l=0; l < num_nodes; l++) {
            stk::mesh::Entity colNode = node_rels[l];
            node_gid = this->gid(colNode);
            node_lid = getLocalElement(m_overlap_node_vs,node_gid);
            layeredMeshNumbering.getIndices(node_lid, base_id, ilayer);
            if(ilayer == 0) {
              for (int il_col=0; il_col<numLayers+1; il_col++) {
                LO inode = layeredMeshNumbering.getId(base_id, il_col);
                GO gnode = getGlobalElement(m_overlap_node_vs,inode);
                for (std::size_t m=0; m < n3dEq; m++) {
                  col = getGlobalDOF(gnode, m);
                  m_overlap_jac_factory->insertGlobalIndices(row, Teuchos::arrayView(&col, 1));
                  m_overlap_jac_factory->insertGlobalIndices(col, Teuchos::arrayView(&row, 1));
                }
                col = getGlobalDOF(gnode, n3dEq);
                m_overlap_jac_factory->insertGlobalIndices(row, Teuchos::arrayView(&col, 1));
              }
            }
          }
        } else {
          // Insert diagonal elements
          m_overlap_jac_factory->insertGlobalIndices(row, Teuchos::arrayView(&row, 1));
        }
      }
    }
  }
  m_overlap_jac_factory->fillComplete();

  // Create Owned graph by exporting overlap with known row map
  m_jac_factory = Teuchos::rcp(new ThyraCrsMatrixFactory(m_vs, m_vs, m_overlap_jac_factory));
}

} // namespace Albany
