//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "PHAL_Utilities.hpp"

#include "Albany_ThyraUtils.hpp"
#include "Albany_CombineAndScatterManager.hpp"

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
void
QCAD::FieldValueScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* response
  Teuchos::RCP<Thyra_Vector> g = workset.g;
  if (g != Teuchos::null) {
    Teuchos::ArrayRCP<ST> g_nonconstView = Albany::getNonconstLocalData(g);
    for (int res = 0; res < this->field_components.size(); res++) {
      g_nonconstView[res] = this->global_response(this->field_components[res]).val();
  }
  }

  Teuchos::RCP<Thyra_MultiVector> dgdx = workset.dgdx;
  Teuchos::RCP<Thyra_MultiVector> dgdxdot = workset.dgdxdot;
  Teuchos::RCP<Thyra_MultiVector> overlapped_dgdx = workset.overlapped_dgdx;
  Teuchos::RCP<Thyra_MultiVector> overlapped_dgdxdot = workset.overlapped_dgdxdot;
  Teuchos::RCP<Thyra_MultiVector> dg, overlapped_dg;
  if (dgdx != Teuchos::null) {
    dg = dgdx;
    overlapped_dg = overlapped_dgdx;
  } else {
    dg = dgdxdot;
    overlapped_dg = overlapped_dgdxdot;
  }

  dg->assign(0.0);
  overlapped_dg->assign(0.0);

  Teuchos::ArrayRCP<Teuchos::ArrayRCP<ST>> overlapped_dg_nonconst2dView = Albany::getNonconstLocalData(overlapped_dg);
  // Extract derivatives for the cell corresponding to nodeID
  auto nodeID = workset.wsElNodeEqID;
  if (max_cell != -1) {

    // Loop over responses
    for (int res = 0; res < this->field_components.size(); res++) {
     // ScalarT& val = this->global_response(this->field_components[res]);

      // Loop over nodes in cell
      for (int node_dof=0; node_dof<numNodes; node_dof++) {
        int neq = nodeID.extent(2);

        // Loop over equations per node
        for (int eq_dof=0; eq_dof<neq; eq_dof++) {

          // local derivative component
          int deriv = neq * node_dof + eq_dof;

          // local DOF
          int dof = nodeID(max_cell,node_dof,eq_dof);

          // Set dg/dx
          // NOTE: 2d views are in column major
          overlapped_dg_nonconst2dView[res][dof] = this->global_response(this->field_components[res]).dx(deriv);

        } // column equations
      } // column nodes
    } // response
  } // cell belongs to this proc

  workset.x_cas_manager->combine(overlapped_dg, dg, Albany::CombineMode::ADD);
}

// **********************************************************************
// Specialization: Distributed Parameter Derivative
// **********************************************************************

template<typename Traits>
void
QCAD::FieldValueScatterScalarResponse<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
/*
  // Here we scatter the *global* response
  Teuchos::RCP<Epetra_Vector> g = workset.g;
  if (g != Teuchos::null)
    for (int res = 0; res < this->field_components.size(); res++) {
      (*g)[res] = this->global_response[this->field_components[res]].val();
  }

  Teuchos::RCP<Epetra_MultiVector> dgdx = workset.dgdx;
  Teuchos::RCP<Epetra_MultiVector> dgdxdot = workset.dgdxdot;
  Teuchos::RCP<Epetra_MultiVector> overlapped_dgdx = workset.overlapped_dgdx;
  Teuchos::RCP<Epetra_MultiVector> overlapped_dgdxdot =
    workset.overlapped_dgdxdot;
  Teuchos::RCP<Epetra_MultiVector> dg, overlapped_dg;
  if (dgdx != Teuchos::null) {
    dg = dgdx;
    overlapped_dg = overlapped_dgdx;
  }
  else {
    dg = dgdxdot;
    overlapped_dg = overlapped_dgdxdot;
  }

  dg->PutScalar(0.0);
  overlapped_dg->PutScalar(0.0);

  // Extract derivatives for the cell corresponding to nodeID
  auto nodeID = workset.wsElNodeEqID;
  if (max_cell != -1) {

    // Loop over responses
    for (int res = 0; res < this->field_components.size(); res++) {
      ScalarT& val = this->global_response(this->field_components[res]);

      // Loop over nodes in cell
      for (int node_dof=0; node_dof<numNodes; node_dof++) {
        int neq = nodeID.extent(2);

        // Loop over equations per node
        for (int eq_dof=0; eq_dof<neq; eq_dof++) {

          // local derivative component
          int deriv = neq * node_dof + eq_dof;

          // local DOF
          int dof = nodeID(max_cell,node_dof,eq_dof);

          // Set dg/dx
          overlapped_dg->ReplaceMyValue(dof, res, val.dx(deriv));

        } // column equations
      } // column nodes
    } // response
  } // cell belongs to this proc

  dg->Export(*overlapped_dg, *workset.x_importer, Add);
*/
}

template<typename EvalT, typename Traits>
QCAD::ResponseFieldValue<EvalT, Traits>::
ResponseFieldValue(Teuchos::ParameterList& p,
                   const Teuchos::RCP<Albany::Layouts>& dl) :
  coordVec("Coord Vec", dl->qp_vector),
  weights("Weights", dl->qp_scalar)
{
  // get and validate Response parameter list
  Teuchos::ParameterList* plist =
    p.get<Teuchos::ParameterList*>("Parameter List");
  Teuchos::RCP<const Teuchos::ParameterList> reflist =
    this->getValidResponseParameters();
  plist->validateParameters(*reflist,0);

  //! parameters passed down from problem
  Teuchos::RCP<Teuchos::ParameterList> paramsFromProblem =
    p.get< Teuchos::RCP<Teuchos::ParameterList> >("Parameters From Problem");

    // Material database (if given)
  if(paramsFromProblem != Teuchos::null)
    materialDB = paramsFromProblem->get< Teuchos::RCP<Albany::MaterialDatabase> >("MaterialDB");
  else materialDB = Teuchos::null;

  // number of quad points per cell and dimension of space
  Teuchos::RCP<PHX::DataLayout> scalar_dl = dl->qp_scalar;
  Teuchos::RCP<PHX::DataLayout> vector_dl = dl->qp_vector;

  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  opRegion  = Teuchos::rcp( new QCAD::MeshRegion<EvalT, Traits>("Coord Vec","Weights",*plist,materialDB,dl) );

  // User-specified parameters
  operation    = plist->get<std::string>("Operation");

  bOpFieldIsVector = false;
  if(plist->isParameter("Operation Vector Field Name")) {
    opFieldName  = plist->get<std::string>("Operation Vector Field Name");
    bOpFieldIsVector = true;
  }
  else opFieldName  = plist->get<std::string>("Operation Field Name");

  bRetFieldIsVector = false;
  if(plist->isParameter("Return Vector Field Name")) {
    retFieldName  = plist->get<std::string>("Return Vector Field Name");
    bRetFieldIsVector = true;
  }
  else retFieldName = plist->get<std::string>("Return Field Name", opFieldName);
  bReturnOpField = (opFieldName == retFieldName);

  opX = plist->get<bool>("Operate on x-component", true) && (numDims > 0);
  opY = plist->get<bool>("Operate on y-component", true) && (numDims > 1);
  opZ = plist->get<bool>("Operate on z-component", true) && (numDims > 2);

  // setup operation field and return field (if it's a different field)
  if(bOpFieldIsVector)
    opField = decltype(opField)(opFieldName, vector_dl);
  else
    opField = decltype(opField)(opFieldName, scalar_dl);

  if(!bReturnOpField) {
    if(bRetFieldIsVector)
      retField = decltype(retField)(retFieldName, vector_dl);
    else
      retField = decltype(retField)(retFieldName, scalar_dl);
  }

  // add dependent fields
  this->addDependentField(opField.fieldTag());
  this->addDependentField(coordVec.fieldTag());
  this->addDependentField(weights.fieldTag());
  opRegion->addDependentFields(this);
  if(!bReturnOpField) this->addDependentField(retField.fieldTag()); //when return field is *different* from op field

  // Set sentinal values for max/min problems
  initVals = Teuchos::Array<double>(5, 0.0);
  if( operation == "Maximize" ) initVals[1] = -1e200;
  else if( operation == "Minimize" ) initVals[1] = 1e100;
  else TEUCHOS_TEST_FOR_EXCEPTION (
    true, Teuchos::Exceptions::InvalidParameter, std::endl
    << "Error!  Invalid operation type " << operation << std::endl);

  this->setName(opFieldName+" Response Field Value" );

  // Setup scatter evaluator
  std::string global_response_name =
    opFieldName + " Global Response Field Value";
  //int worksetSize = scalar_dl->extent(0);
  int responseSize = 5;
  Teuchos::RCP<PHX::DataLayout> global_response_layout =
    Teuchos::rcp(new PHX::MDALayout<Dim>(responseSize));
  PHX::Tag<ScalarT> global_response_tag(global_response_name,
                                        global_response_layout);
  p.set("Stand-alone Evaluator", false);
  p.set("Global Response Field Tag", global_response_tag);
  this->setup(p,dl);

  // Specify which components of response (in this case 0th and 1st) to
  //  scatter derivatives for.
  FieldValueScatterScalarResponse<EvalT,Traits>::field_components.resize(2);
  FieldValueScatterScalarResponse<EvalT,Traits>::field_components[0] = 0;
  FieldValueScatterScalarResponse<EvalT,Traits>::field_components[1] = 1;
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(opField,fm);
  this->utils.setFieldData(coordVec,fm);
  this->utils.setFieldData(weights,fm);
  if(!bReturnOpField) this->utils.setFieldData(retField,fm);
  opRegion->postRegistrationSetup(fm);
  QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::postRegistrationSetup(d,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  for (typename PHX::MDField<ScalarT>::size_type i=0;
       i<this->global_response.size(); i++)
    this->global_response_eval(i) = initVals[i];

  // Do global initialization
  QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::preEvaluate(workset);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  ScalarT opVal, qpVal, cellVol;

  if(!opRegion->elementBlockIsInRegion(workset.EBName))
    return;

  for (std::size_t cell=0; cell < workset.numCells; ++cell)
  {
    if(!opRegion->cellIsInRegion(cell)) continue;

    // Get the cell volume, used for averaging over a cell
    cellVol = 0.0;
    for (std::size_t qp=0; qp < numQPs; ++qp)
      cellVol += weights(cell,qp);

    // Get the scalar value of the field being operated on which will be used
    //  in the operation (all operations just deal with scalar data so far)
    opVal = 0.0;
    for (std::size_t qp=0; qp < numQPs; ++qp) {
      qpVal = 0.0;
      if(bOpFieldIsVector) {
        if(opX) qpVal += opField(cell,qp,0) * opField(cell,qp,0);
        if(opY) qpVal += opField(cell,qp,1) * opField(cell,qp,1);
        if(opZ) qpVal += opField(cell,qp,2) * opField(cell,qp,2);
      }
      else qpVal = opField(cell,qp);
      opVal += qpVal * weights(cell,qp);
    }
    opVal /= cellVol;
    // opVal = the average value of the field operated on over the current cell


    // Check if the currently stored min/max value needs to be updated
    if( (operation == "Maximize" && opVal > this->global_response(1)) ||
        (operation == "Minimize" && opVal < this->global_response(1)) ) {
      max_cell = cell;

      // set g[0] = value of return field at the current cell (avg)
      this->global_response_eval(0)=0.0;
      if(bReturnOpField) {
        for (std::size_t qp=0; qp < numQPs; ++qp) {
          qpVal = 0.0;
          if(bOpFieldIsVector) {
            for(std::size_t i=0; i<numDims; i++) {
              qpVal += opField(cell,qp,i)*opField(cell,qp,i);
            }
          }
          else qpVal = opField(cell,qp);
          this->global_response_eval(0) += qpVal * weights(cell,qp);
        }
      }
      else {
        for (std::size_t qp=0; qp < numQPs; ++qp) {
          qpVal = 0.0;
          if(bRetFieldIsVector) {
            for(std::size_t i=0; i<numDims; i++) {
              qpVal += retField(cell,qp,i)*retField(cell,qp,i);
            }
          }
          else qpVal = retField(cell,qp);
          this->global_response_eval(0) += qpVal * weights(cell,qp);
        }
      }
      this->global_response_eval(0) /= cellVol;

      // set g[1] = value of the field operated on at the current cell (avg)
      this->global_response_eval(1) = opVal;

      // set g[2+] = average qp coordinate values of the current cell
      for(std::size_t i=0; i<numDims; i++) {
        this->global_response_eval(i+2) = 0.0;
        for (std::size_t qp=0; qp < numQPs; ++qp)
          this->global_response_eval(i+2) += coordVec(cell,qp,i);
        this->global_response_eval(i+2) /= numQPs;
      }
    }

  } // end of loop over cells

  // No local scattering
}

// **********************************************************************
template<typename EvalT, typename Traits>
void QCAD::ResponseFieldValue<EvalT, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  int indexToMax = 1;
  ScalarT max = this->global_response(indexToMax);
  Teuchos::EReductionType reductType;
  if (operation == "Maximize")
    reductType = Teuchos::REDUCE_MAX;
  else
    reductType = Teuchos::REDUCE_MIN;

  ScalarT send = this->global_response(indexToMax);
  Teuchos::reduceAll<int, ScalarT>(
    *workset.comm, reductType, 1, &send, &max);

  int procToBcast;
  if( this->global_response(indexToMax) == max )
    procToBcast = workset.comm->getRank();
  else procToBcast = -1;

  int winner;
  Teuchos::reduceAll(
    *workset.comm, Teuchos::REDUCE_MAX, 1, &procToBcast, &winner);

  PHAL::broadcast(*workset.comm, winner, this->global_response_eval);

  // Do global scattering
  if (workset.comm->getRank() == winner)
    QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::setMaxCell(max_cell);
  else
    QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::setMaxCell(-1);

  QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::postEvaluate(workset);
}

// **********************************************************************
template<typename EvalT,typename Traits>
Teuchos::RCP<const Teuchos::ParameterList>
QCAD::ResponseFieldValue<EvalT,Traits>::getValidResponseParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
        rcp(new Teuchos::ParameterList("Valid ResponseFieldValue Params"));
  Teuchos::RCP<const Teuchos::ParameterList> baseValidPL =
    QCAD::FieldValueScatterScalarResponse<EvalT,Traits>::getValidResponseParameters();
  validPL->setParameters(*baseValidPL);

  Teuchos::RCP<const Teuchos::ParameterList> regionValidPL =
    QCAD::MeshRegion<EvalT,Traits>::getValidParameters();
  validPL->setParameters(*regionValidPL);

  validPL->set<std::string>("Name", "", "Name of response function");
  validPL->set<int>("Phalanx Graph Visualization Detail", 0, "Make dot file to visualize phalanx graph");

  validPL->set<std::string>("Operation", "Maximize", "Operation to perform");
  validPL->set<std::string>("Operation Field Name", "", "Scalar field to perform operation on");
  validPL->set<std::string>("Operation Vector Field Name", "", "Vector field to perform operation on");
  validPL->set<std::string>("Return Field Name", "<operation field name>",
                       "Scalar field to return value from");
  validPL->set<std::string>("Return Vector Field Name", "<operation vector field name>",
                       "Vector field to return value from");

  validPL->set<bool>("Operate on x-component", true,
                     "Whether to perform operation on x component of vector field");
  validPL->set<bool>("Operate on y-component", true,
                     "Whether to perform operation on y component of vector field");
  validPL->set<bool>("Operate on z-component", true,
                     "Whether to perform operation on z component of vector field");

  validPL->set<std::string>("Description", "", "Description of this response used by post processors");

  return validPL;
}

// **********************************************************************

