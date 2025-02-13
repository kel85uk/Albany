//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Aeras_XScalarAdvectionProblem.hpp"

#include "Intrepid2_DefaultCubatureFactory.hpp"
#include "Shards_CellTopology.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Albany_Utils.hpp"
#include "Albany_BCUtils.hpp"
#include "Albany_ProblemUtils.hpp"
#include <string>
#include <sstream>


Aeras::XScalarAdvectionProblem::
XScalarAdvectionProblem( const Teuchos::RCP<Teuchos::ParameterList>& params_,
             const Teuchos::RCP<ParamLib>& paramLib_,
             const int numDim_) :
  Albany::AbstractProblem(params_, paramLib_),
  dof_names_tracers(arcpFromArray(params_->sublist("XScalarAdvection Problem").
        get<Teuchos::Array<std::string> >("Tracers",
            Teuchos::Array<std::string>()))),
  numDim(numDim_),
  numLevels (params_->sublist("XScalarAdvection Problem").get<int>("Number of Vertical Levels", 10)),
  numTracers(dof_names_tracers.size())
{
  // Set number of scalar equation per node, neq,  based on numDim
  std::cout << "Number of Vertical Levels: " << numLevels << std::endl;
  std::cout << "Number of Tracers        : " << numTracers << std::endl;
  std::cout << "Names of Tracers         : ";
  for (int i=0; i<numTracers; ++i) std::cout <<dof_names_tracers[i]<<"  ";
  std::cout << std::endl;

  neq       = numLevels*(1+numTracers);

  // Set the num PDEs for the null space object to pass to ML
  this->rigidBodyModes->setNumPDEs(neq);
}

Aeras::XScalarAdvectionProblem::
~XScalarAdvectionProblem()
{
}

void
Aeras::XScalarAdvectionProblem::
buildProblem(
  Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
  Albany::StateManager& stateMgr)
{
  using Teuchos::rcp;

 /* Construct All Phalanx Evaluators */
  TEUCHOS_TEST_FOR_EXCEPTION(
      meshSpecs.size()!=1,
      std::logic_error,
      "Problem supports one Material Block");
  fm.resize(1);
  fm[0] = rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
  buildEvaluators(*fm[0],
                  *meshSpecs[0],
                  stateMgr,
                  Albany::BUILD_RESID_FM,
      Teuchos::null);
  constructDirichletEvaluators(*meshSpecs[0]);

  // Build a sideset evaluator if sidesets are present
  if(meshSpecs[0]->ssNames.size() > 0)
     constructNeumannEvaluators(meshSpecs[0]);
}

Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
Aeras::XScalarAdvectionProblem::
buildEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fmchoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  // Call constructeEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  Albany::ConstructEvaluatorsOp<XScalarAdvectionProblem>
    op(*this,
       fm0,
       meshSpecs,
       stateMgr,
       fmchoice,
       responseList);
  Sacado::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes> fe(op);
  return *op.tags;
}

void
Aeras::XScalarAdvectionProblem::constructDirichletEvaluators(
        const Albany::MeshSpecsStruct& meshSpecs)
{
   // Construct Dirichlet evaluators for all nodesets and names
   std::vector<std::string> dirichletNames(numLevels*(1+numTracers));
   std::ostringstream s;
   for (int i=0,n=0; i<numLevels; ++i) {
     s.str(std::string());
     s << "rho_"<<i;
     dirichletNames[n++] = s.str();
     for (int j=0; j<numTracers; ++j) {
       s.str(std::string());
       s << dof_names_tracers[j] <<"_"<<i;
       dirichletNames[n++] = s.str();
     }
   }
   Albany::BCUtils<Albany::DirichletTraits> dirUtils;
   dfm = dirUtils.constructBCEvaluators(meshSpecs.nsNames,
                                        dirichletNames,
                                        this->params,
                                        this->paramLib);
   offsets_ = dirUtils.getOffsets();
   nodeSetIDs_ = dirUtils.getNodeSetIDs();
}

// Neumann BCs
void
Aeras::XScalarAdvectionProblem::
constructNeumannEvaluators(const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs)
{

   // Note: we only enter this function if sidesets are defined in the mesh file
   // i.e. meshSpecs.ssNames.size() > 0

   Albany::BCUtils<Albany::NeumannTraits> nbcUtils;

   // Check to make sure that Neumann BCs are given in the input file

   if(!nbcUtils.haveBCSpecified(this->params)) {
      return;
   }


   // Construct BC evaluators for all side sets and names
   // Note that the string index sets up the equation offset, so ordering is important

   std::vector<std::string> neumannNames(1 + 1);
   Teuchos::Array<Teuchos::Array<int> > offsets;
   offsets.resize(1 + 1);

   neumannNames[0] = "rho";
   offsets[0].resize(1);
   offsets[0][0] = 0;
   offsets[1].resize(1);
   offsets[1][0] = 0;

   neumannNames[1] = "all";

   // Construct BC evaluators for all possible names of conditions
   // Should only specify flux vector components (dUdx, dUdy, dUdz)
   std::vector<std::string> condNames(1); //(dUdx, dUdy, dUdz)
   Teuchos::ArrayRCP<std::string> dof_names(1);
     dof_names[0] = "rho";

   // Note that sidesets are only supported for two and 3D currently
   if(numDim == 1)
    condNames[0] = "(dFluxdx)";
   else if(numDim == 2)
    condNames[0] = "(dFluxdx, dFluxdy)";
   else if(numDim == 3)
    condNames[0] = "(dFluxdx, dFluxdy, dFluxdz)";
   else
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
       std::endl << "Error: Sidesets only supported in 2 and 3D." << std::endl);

   nfm.resize(1); // Aeras X scalar advection problem only has one
                  // element block

   nfm[0] = nbcUtils.constructBCEvaluators(meshSpecs,
                                           neumannNames,
                                           dof_names,
                                           true,
                                           0,
                                           condNames,
                                           offsets,
                                           dl,
                                           this->params,
                                           this->paramLib);


}

Teuchos::RCP<const Teuchos::ParameterList>
Aeras::XScalarAdvectionProblem::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericProblemParams("ValidXScalarAdvectionProblemParams");

  validPL->sublist("XScalarAdvection Problem", false, "");
  return validPL;
}

