//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

// ===================================================
//! Includes
// ===================================================

#include "Interface.hpp"
#include "Albany_MpasSTKMeshStruct.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_RCP.hpp"
#include "Albany_Utils.hpp"
#include "Albany_SolverFactory.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_StandardCatchMacros.hpp"
#include <stk_mesh/base/FieldBase.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include "Piro_PerformSolve.hpp"
#include "Albany_OrdinarySTKFieldContainer.hpp"
#include "Albany_STKDiscretization.hpp"
#include "Thyra_DetachedVectorView.hpp"

#ifdef ALBANY_SEACAS
#include <stk_io/IossBridge.hpp>
#include <stk_io/StkMeshIoBroker.hpp>
#include <Ionit_Initializer.h>
#endif

Teuchos::RCP<Albany::MpasSTKMeshStruct> meshStruct;
Teuchos::RCP<Albany::Application> albanyApp;
Teuchos::RCP<Teuchos::ParameterList> paramList;
Teuchos::RCP<const Teuchos_Comm> mpiComm;
Teuchos::RCP<Teuchos::ParameterList> discParams;
Teuchos::RCP<Albany::SolverFactory> slvrfctry;
Teuchos::RCP<double> MPAS_dt;

double MPAS_gravity(9.8), MPAS_rho_ice(910.0), MPAS_rho_seawater(1028.0), MPAS_sea_level(0),
       MPAS_flowParamA(1e-4), MPAS_flowLawExponent(3), MPAS_dynamic_thickness(1e-2),
       MPAS_ClausiusClapeyoronCoeff(9.7546e-8);
bool MPAS_useGLP(true);

Teuchos::RCP<Thyra::ResponseOnlyModelEvaluatorBase<double> > solver;

bool keptMesh =false;

typedef struct TET_ {
  int verts[4];
  int neighbours[4];
  char bound_type[4];
} TET;

/***********************************************************/


void velocity_solver_solve_fo(int nLayers, int nGlobalVertices,
    int nGlobalTriangles, bool ordering, bool first_time_step,
    const std::vector<int>& indexToVertexID,
    const std::vector<int>& indexToTriangleID, double minBeta,
    const std::vector<double>& /* regulThk */,
    const std::vector<double>& levelsNormalizedThickness,
    const std::vector<double>& elevationData,
    const std::vector<double>& thicknessData,
    const std::vector<double>& betaData,
    const std::vector<double>& bedTopographyData,
    const std::vector<double>& smbData,
    const std::vector<double>& stiffeningFactorData,
    const std::vector<double>& effectivePressureData,
    const std::vector<double>& temperatureOnTetra,
    std::vector<double>& dissipationHeatOnTetra,
    std::vector<double>& velocityOnVertices,
    int& error,
    const double& deltat)
{
  int numVertices3D = (nLayers + 1) * indexToVertexID.size();
  int numPrisms = nLayers * indexToTriangleID.size();
  int vertexColumnShift = (ordering == 1) ? 1 : nGlobalVertices;
  int lVertexColumnShift = (ordering == 1) ? 1 : indexToVertexID.size();
  int vertexLayerShift = (ordering == 0) ? 1 : nLayers + 1;

  int elemColumnShift = (ordering == 1) ? 3 : 3 * nGlobalTriangles;
  int lElemColumnShift = (ordering == 1) ? 3 : 3 * indexToTriangleID.size();
  int elemLayerShift = (ordering == 0) ? 3 : 3 * nLayers;

  int neq = meshStruct->neq;

  const bool interleavedOrdering = meshStruct->getInterleavedOrdering();

  *MPAS_dt =  deltat;

  Teuchos::ArrayRCP<double>& layerThicknessRatio = meshStruct->layered_mesh_numbering->layers_ratio;
  for (int i = 0; i < nLayers; i++) {
    layerThicknessRatio[i] = levelsNormalizedThickness[i+1]-levelsNormalizedThickness[i];
  }

  typedef Albany::AbstractSTKFieldContainer::VectorFieldType VectorFieldType;
  typedef Albany::AbstractSTKFieldContainer::ScalarFieldType ScalarFieldType;

  VectorFieldType* solutionField;

  if (interleavedOrdering) {
    solutionField = Teuchos::rcp_dynamic_cast<
        Albany::OrdinarySTKFieldContainer<true> >(
        meshStruct->getFieldContainer())->getSolutionField();
  } else {
    solutionField = Teuchos::rcp_dynamic_cast<
        Albany::OrdinarySTKFieldContainer<false> >(
        meshStruct->getFieldContainer())->getSolutionField();
  }

  ScalarFieldType* surfaceHeightField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "surface_height");
  ScalarFieldType* thicknessField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "ice_thickness");
  ScalarFieldType* bedTopographyField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "bed_topography");
  ScalarFieldType* smbField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "surface_mass_balance");
  VectorFieldType* dirichletField = meshStruct->metaData->get_field <VectorFieldType> (stk::topology::NODE_RANK, "dirichlet_field");
  ScalarFieldType* basalFrictionField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "basal_friction");
  ScalarFieldType* stiffeningFactorField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "stiffening_factor");
  ScalarFieldType* effectivePressureField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "effective_pressure");

  bool nonEmptyEffectivePressure = effectivePressureData.size()>0;
  for (int j = 0; j < numVertices3D; ++j) {
    int ib = (ordering == 0) * (j % lVertexColumnShift)
        + (ordering == 1) * (j / vertexLayerShift);
    int il = (ordering == 0) * (j / lVertexColumnShift)
        + (ordering == 1) * (j % vertexLayerShift);
    int gId = il * vertexColumnShift + vertexLayerShift * indexToVertexID[ib];
    stk::mesh::Entity node = meshStruct->bulkData->get_entity(stk::topology::NODE_RANK, gId + 1);
    double* coord = stk::mesh::field_data(*meshStruct->getCoordinatesField(), node);
    coord[2] = elevationData[ib] - levelsNormalizedThickness[nLayers - il] * thicknessData[ib];


    double* thickness = stk::mesh::field_data(*thicknessField, node);
    thickness[0] = thicknessData[ib];
    double* sHeight = stk::mesh::field_data(*surfaceHeightField, node);
    sHeight[0] = elevationData[ib];
    double* bedTopography = stk::mesh::field_data(*bedTopographyField, node);
    bedTopography[0] = bedTopographyData[ib];
    double* stiffeningFactor = stk::mesh::field_data(*stiffeningFactorField, node);
    stiffeningFactor[0] = std::log(stiffeningFactorData[ib]);
    double* effectivePressure = stk::mesh::field_data(*effectivePressureField, node);

    if(nonEmptyEffectivePressure && (effectivePressure != NULL))
      effectivePressure[0] = effectivePressureData[ib];
    
    if(smbField != NULL) {
      double* smb = stk::mesh::field_data(*smbField, node);
      smb[0] = smbData[ib];
    }
    double* sol = stk::mesh::field_data(*solutionField, node);
    sol[0] = velocityOnVertices[j];
    sol[1] = velocityOnVertices[j + numVertices3D];
    if(neq==3) {
      sol[2] = thicknessData[ib];
    }
    double* dirichletVel = stk::mesh::field_data(*dirichletField, node);
    dirichletVel[0]=velocityOnVertices[j]; //velocityOnVertices stores initial guess and dirichlet velocities.
    dirichletVel[1]=velocityOnVertices[j + numVertices3D];
    if (il == 0) {
      double* beta = stk::mesh::field_data(*basalFrictionField, node);
      beta[0] = std::max(betaData[ib], minBeta);
    }
  }

  ScalarFieldType* temperature_field = meshStruct->metaData->get_field<ScalarFieldType>(stk::topology::ELEMENT_RANK, "temperature");

  for (int j = 0; j < numPrisms; ++j) {
    int ib = (ordering == 0) * (j % (lElemColumnShift / 3))
        + (ordering == 1) * (j / (elemLayerShift / 3));
    int il = (ordering == 0) * (j / (lElemColumnShift / 3))
        + (ordering == 1) * (j % (elemLayerShift / 3));
    int gId = il * elemColumnShift + elemLayerShift * indexToTriangleID[ib];
    int lId = il * lElemColumnShift + elemLayerShift * ib;
    for (int iTetra = 0; iTetra < 3; iTetra++) {
      stk::mesh::Entity elem = meshStruct->bulkData->get_entity(stk::topology::ELEMENT_RANK, ++gId);
      double* temperature = stk::mesh::field_data(*temperature_field, elem);
      temperature[0] = temperatureOnTetra[lId++];
    }
  }

  meshStruct->setHasRestartSolution(true);//!first_time_step);

  if (!first_time_step) {
    meshStruct->setRestartDataTime(
        paramList->sublist("Problem").get("Homotopy Restart Step", 1.));
    double homotopy =
        paramList->sublist("Problem").sublist("LandIce Viscosity").get(
            "Glen's Law Homotopy Parameter", 1.0);
    if (meshStruct->restartDataTime() == homotopy) {
      paramList->sublist("Problem").set("Solution Method", "Steady");
      paramList->sublist("Piro").set("Solver Type", "NOX");
    }
  }

  if(!keptMesh) {
    albanyApp->createDiscretization();
  } else {
    auto abs_disc = albanyApp->getDiscretization();
    auto stk_disc = Teuchos::rcp_dynamic_cast<Albany::STKDiscretization>(abs_disc);
    stk_disc->updateMesh();
  }
  albanyApp->finalSetUp(paramList);

  bool success = true;
  Teuchos::ArrayRCP<const ST> solution_constView;
  try {
    solver = slvrfctry->createAndGetAlbanyApp(albanyApp, mpiComm, mpiComm, Teuchos::null, false);

    Teuchos::ParameterList solveParams;
    solveParams.set("Compute Sensitivities", false);

    Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<double> > > thyraResponses;
    Teuchos::Array<
        Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<double> > > > thyraSensitivities;
    Piro::PerformSolveBase(*solver, solveParams, thyraResponses, thyraSensitivities);

    // Printing responses
    const int num_g = solver->Ng();
    for (int i=0; i<num_g-1; i++) {
      if (albanyApp->getResponse(i)->isScalarResponse()) {
        Thyra::ConstDetachedVectorView<double> g(thyraResponses[i]);
        std::cout << std::setprecision(15) << "\nResponse " << i << ": " << g[0] << std::endl;
      }
    }

    auto overlapVS = albanyApp->getDiscretization()->getOverlapVectorSpace();
    auto disc = albanyApp->getDiscretization();
    auto cas_manager = Albany::createCombineAndScatterManager(disc->getVectorSpace(), disc->getOverlapVectorSpace());
    Teuchos::RCP<Thyra_Vector> solution = Thyra::createMember(disc->getOverlapVectorSpace());
    cas_manager->scatter(*disc->getSolutionField(), *solution, Albany::CombineMode::INSERT);
    solution_constView = Albany::getLocalData(solution.getConst());
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, std::cerr, success);

  error = !success;

  auto overlapVS = albanyApp->getDiscretization()->getOverlapVectorSpace();
  for (int j = 0; j < numVertices3D; ++j) {
    int ib = (ordering == 0) * (j % lVertexColumnShift)
        + (ordering == 1) * (j / vertexLayerShift);
    int il = (ordering == 0) * (j / lVertexColumnShift)
        + (ordering == 1) * (j % vertexLayerShift);
    int gId = il * vertexColumnShift + vertexLayerShift * indexToVertexID[ib];

    int lId0, lId1;

    if (interleavedOrdering) {
      lId0 = Albany::getLocalElement(overlapVS,neq * gId);
      lId1 = lId0 + 1;
    } else {
      lId0 = Albany::getLocalElement(overlapVS,gId);
      lId1 = lId0 + numVertices3D;
    }
    velocityOnVertices[j] = solution_constView[lId0];
    velocityOnVertices[j + numVertices3D] = solution_constView[lId1];
  }

  ScalarFieldType* dissipationHeatField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::ELEMENT_RANK, "dissipation_heat");
  for (int j = 0; j < numPrisms; ++j) {
    int ib = (ordering == 0) * (j % (lElemColumnShift / 3))
        + (ordering == 1) * (j / (elemLayerShift / 3));
    int il = (ordering == 0) * (j / (lElemColumnShift / 3))
        + (ordering == 1) * (j % (elemLayerShift / 3));
    int gId = il * elemColumnShift + elemLayerShift * indexToTriangleID[ib];
    int lId = il * lElemColumnShift + elemLayerShift * ib;
    for (int iTetra = 0; iTetra < 3; iTetra++) {
      stk::mesh::Entity elem = meshStruct->bulkData->get_entity(stk::topology::ELEMENT_RANK, ++gId);
      double* dissipationHeat = stk::mesh::field_data(*dissipationHeatField, elem);
      dissipationHeatOnTetra[lId++] = dissipationHeat[0];
    }
  }

  keptMesh = true;
}

void velocity_solver_export_fo_velocity(MPI_Comm reducedComm) {
#ifdef ALBANY_SEACAS
  Teuchos::RCP<stk::io::StkMeshIoBroker> mesh_data = Teuchos::rcp(new stk::io::StkMeshIoBroker(reducedComm));
    mesh_data->set_bulk_data(*meshStruct->bulkData);
    size_t idx = mesh_data->create_output_mesh("IceSheet.exo", stk::io::WRITE_RESULTS);
    mesh_data->process_output_request(idx, 0.0);
#endif
}

int velocity_solver_init_mpi(int* /* fComm */) {
  Kokkos::initialize();
  return 0;
}

void velocity_solver_finalize() {
  meshStruct = Teuchos::null;
  albanyApp = Teuchos::null;
  paramList = Teuchos::null;
  mpiComm = Teuchos::null;
  discParams = Teuchos::null;
  slvrfctry = Teuchos::null;
  MPAS_dt = Teuchos::null;
  solver = Teuchos::null;
  Kokkos::finalize_all();
}

/*duality:
 *
 *   mpas(F) |  lifev
 *  ---------|---------
 *   cell    |  vertex
 *   vertex  |  triangle
 *   edge    |  edge
 *
 */

void velocity_solver_compute_2d_grid(MPI_Comm reducedComm) {
  keptMesh = false;
  mpiComm = Albany::createTeuchosCommFromMpiComm(reducedComm);
}

void velocity_solver_set_physical_parameters(double const& gravity, double const& ice_density, double const& ocean_density, double const& sea_level, double const& flowParamA, double const& flowLawExponent, double const& dynamic_thickness, bool const& use_GLP, double const& clausiusClapeyoronCoeff) {
  MPAS_gravity=gravity;
  MPAS_rho_ice = ice_density;
  MPAS_rho_seawater = ocean_density;
  MPAS_sea_level = sea_level;
  MPAS_flowParamA = flowParamA;
  MPAS_flowLawExponent = flowLawExponent;
  MPAS_dynamic_thickness = dynamic_thickness;
  MPAS_useGLP = use_GLP;
  MPAS_ClausiusClapeyoronCoeff = clausiusClapeyoronCoeff;
}

void velocity_solver_extrude_3d_grid(int nLayers, int nGlobalTriangles,
    int nGlobalVertices, int nGlobalEdges, int Ordering, MPI_Comm /* reducedComm */,
    const std::vector<int>& indexToVertexID,
    const std::vector<int>& mpasIndexToVertexID,
    const std::vector<double>& verticesCoords,
    const std::vector<bool>& isVertexBoundary,
    const std::vector<int>& verticesOnTria,
    const std::vector<bool>& isBoundaryEdge,
    const std::vector<int>& trianglesOnEdge,
    const std::vector<int>& trianglesPositionsOnEdge,
    const std::vector<int>& verticesOnEdge,
    const std::vector<int>& indexToEdgeID,
    const std::vector<int>& indexToTriangleID,
    const std::vector<int>& dirichletNodesIds,
    const std::vector<int>& floating2dEdgesIds) {

  slvrfctry = Teuchos::rcp(new Albany::SolverFactory("albany_input.yaml", mpiComm));
  paramList = Teuchos::rcp(&slvrfctry->getParameters(), false);

  auto& bt = paramList->get<std::string>("Build Type");
#ifdef ALBANY_EPETRA
  if(bt == "NONE") bt = "Epetra";
#else
  if(bt == "NONE") bt = "Tpetra";
  TEUCHOS_TEST_FOR_EXCEPTION(bt == "Epetra", Teuchos::Exceptions::InvalidArgument,
                             "Error! ALBANY_EPETRA must be defined in order to perform an Epetra run.\n");
#endif

  if (bt=="Tpetra") {
    // Set the static variable that denotes this as a Tpetra run
    static_cast<void>(Albany::build_type(Albany::BuildType::Tpetra));
  } else if (bt=="Epetra") {
    // Set the static variable that denotes this as a Epetra run
    static_cast<void>(Albany::build_type(Albany::BuildType::Epetra));
  } else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidArgument,
                               "Error! Invalid choice (" + bt + ") for 'Build Type'.\n"
                               "       Valid choices are 'Epetra', 'Tpetra'.\n");
  }

  paramList->set("Overwrite Nominal Values With Final Point", true);

  Teuchos::Array<std::string> arrayRequiredFields(9);
  arrayRequiredFields[0]="temperature";  arrayRequiredFields[1]="ice_thickness"; arrayRequiredFields[2]="surface_height"; arrayRequiredFields[3]="bed_topography";
  arrayRequiredFields[4]="basal_friction";  arrayRequiredFields[5]="surface_mass_balance"; arrayRequiredFields[6]="dirichlet_field", arrayRequiredFields[7]="stiffening_factor", arrayRequiredFields[8]="effective_pressure";

  paramList->sublist("Problem").set("Required Fields", arrayRequiredFields);

  //Physical Parameters
  if(paramList->sublist("Problem").isSublist("LandIce Physical Parameters")) {
    std::cout<<"\nWARNING: Using Physical Parameters (gravity, ice/ocean densities) provided in Albany input file. In order to use those provided by MPAS, remove \"LandIce Physical Parameters\" sublist from Albany input file.\n"<<std::endl;
  }
 
  Teuchos::ParameterList& physParamList = paramList->sublist("Problem").sublist("LandIce Physical Parameters");
 
  double rho_ice, rho_seawater; 
  physParamList.set("Gravity Acceleration", physParamList.get("Gravity Acceleration", MPAS_gravity));
  physParamList.set("Ice Density", rho_ice = physParamList.get("Ice Density", MPAS_rho_ice));
  physParamList.set("Water Density", rho_seawater = physParamList.get("Water Density", MPAS_rho_seawater));
  physParamList.set("Clausius-Clapeyron Coefficient", physParamList.get("Clausius-Clapeyron Coefficient", MPAS_ClausiusClapeyoronCoeff));
  physParamList.set<bool>("Use GLP", physParamList.get("Use GLP", MPAS_useGLP)); //use GLP (Grounding line parametrization) unless actively disabled
  
  paramList->sublist("Problem").set("Name", paramList->sublist("Problem").get("Name", "LandIce Stokes First Order 3D"));

  MPAS_dt = Teuchos::rcp(new double(0.0));
  if (paramList->sublist("Problem").get<std::string>("Name") == "LandIce Coupled FO H 3D") {
    auto& arr = paramList->sublist("Problem").get<Teuchos::Array<std::string>>("Required Fields");
    arr.push_back("surface_mass_balance");
    // paramList->sublist("Problem").sublist("Parameter Fields").set("Register Surface Mass Balance", 1);
    *MPAS_dt = paramList->sublist("Problem").get("Time Step", 0.0);
    paramList->sublist("Problem").set("Time Step Ptr", MPAS_dt); //if it is not there set it to zero.
  }

  if(paramList->sublist("Problem").isSublist("LandIce BCs"))
    std::cout<<"\nWARNING: Using LandIce BCs provided in Albany input file. In order to use boundary conditions provided by MPAS, remove \"LandIce BCs\" sublist from Albany input file.\n"<<std::endl;

  // ---- Setting parameters for LandIce BCs ---- //
  Teuchos::ParameterList& landiceBcList = paramList->sublist("Problem").sublist("LandIce BCs");
  landiceBcList.set<int>("Number",2);

  // Basal Friction BC
  auto& basalParams = landiceBcList.sublist("BC 0");
  int basal_cub_degree = physParamList.get<bool>("Use GLP") ? 8 : 3;
  basalParams.set<int>("Cubature Degree",basalParams.get<int>("Cubature Degree", basal_cub_degree));
  basalParams.set("Side Set Name", basalParams.get("Side Set Name", "basalside"));
  basalParams.set("Type", basalParams.get("Type", "Basal Friction"));
  auto& basalFrictionParams = basalParams.sublist("Basal Friction Coefficient");
  basalFrictionParams.set("Type",basalFrictionParams.get("Type","Given Field"));
  basalFrictionParams.set("Given Field Variable Name",basalFrictionParams.get("Given Field Variable Name","basal_friction"));
  basalFrictionParams.set<bool>("Zero Beta On Floating Ice", basalFrictionParams.get<bool>("Zero Beta On Floating Ice", true));

  //Lateral floating ice BCs
  int lateral_cub_degree = 3;
  double immersed_ratio =  rho_ice/rho_seawater;
  auto& lateralParams = landiceBcList.sublist("BC 1");
  lateralParams.set<int>("Cubature Degree",lateralParams.get<int>("Cubature Degree", lateral_cub_degree));
  lateralParams.set<double>("Immersed Ratio",lateralParams.get<double>("Immersed Ratio",immersed_ratio));
  lateralParams.set("Side Set Name", lateralParams.get("Side Set Name", "floatinglateralside"));
  lateralParams.set("Type", lateralParams.get("Type", "Lateral"));

  //Dirichlet BCs
  if(!paramList->sublist("Problem").isSublist("Dirichlet BCs")) {
    paramList->sublist("Problem").sublist("Dirichlet BCs").set("DBC on NS dirichlet for DOF U0 prescribe Field", "dirichlet_field");
    paramList->sublist("Problem").sublist("Dirichlet BCs").set("DBC on NS dirichlet for DOF U1 prescribe Field", "dirichlet_field");
  }
  else {
    std::cout<<"\nWARNING: Using Dirichlet BCs options provided in Albany input file. In order to use those provided by MPAS, remove \"Dirichlet BCs\" sublist from Albany input file.\n"<<std::endl;
  }

  if(paramList->sublist("Problem").isSublist("LandIce Field Norm") && paramList->sublist("Problem").sublist("LandIce Field Norm").isSublist("sliding_velocity_basalside"))
    std::cout<<"\nWARNING: Using options for Velocity Norm provided in Albany input file. In order to use those provided by MPAS, remove \"LandIce Velocity Norm\" sublist from Albany input file.\n"<<std::endl;

  Teuchos::ParameterList& fieldNormList =  paramList->sublist("Problem").sublist("LandIce Field Norm").sublist("sliding_velocity_basalside"); //empty list if LandIceViscosity not in input file.
  fieldNormList.set("Regularization Type", fieldNormList.get("Regularization Type", "Given Value"));
  double reg_value = 1e-6;
  fieldNormList.set("Regularization Value", fieldNormList.get("Regularization Value", reg_value));
  fieldNormList.set("Regularization Parameter Name", fieldNormList.get("Regularization Parameter Name","Glen's Law Homotopy Parameter"));


  if(paramList->sublist("Problem").isSublist("LandIce Viscosity"))
    std::cout<<"\nWARNING: Using Viscosity options provided in Albany input file. In order to use those provided by MPAS, remove \"LandIce Viscosity\" sublist from Albany input file.\n"<<std::endl;

  Teuchos::ParameterList& viscosityList =  paramList->sublist("Problem").sublist("LandIce Viscosity"); //empty list if LandIceViscosity not in input file.

  viscosityList.set("Type", viscosityList.get("Type", "Glen's Law"));
  double homotopy_param = (paramList->sublist("Problem").get("Solution Method", "Steady") == "Steady") ? 0.3 : 1e-6;
  viscosityList.set("Glen's Law Homotopy Parameter", viscosityList.get("Glen's Law Homotopy Parameter", homotopy_param));
  viscosityList.set("Glen's Law A", viscosityList.get("Glen's Law A", MPAS_flowParamA));
  viscosityList.set("Glen's Law n", viscosityList.get("Glen's Law n",  MPAS_flowLawExponent));
  viscosityList.set("Flow Rate Type", viscosityList.get("Flow Rate Type", "Temperature Based"));
  viscosityList.set("Use Stiffening Factor", viscosityList.get("Use Stiffening Factor", true));
  viscosityList.set("Extract Strain Rate Sq", viscosityList.get("Extract Strain Rate Sq", true)); //set true if not defined


  paramList->sublist("Problem").sublist("Body Force").set("Type", "FO INTERP SURF GRAD");
  

  Teuchos::ParameterList& discretizationList = paramList->sublist("Discretization");
  
  discretizationList.set("Method", discretizationList.get("Method", "Extruded")); //set to Extruded is not defined
  discretizationList.set("Cubature Degree", discretizationList.get("Cubature Degree", 1));  //set 1 if not defined
  discretizationList.set("Interleaved Ordering", discretizationList.get("Interleaved Ordering", true));  //set true if not define
  
  discretizationList.sublist("Required Fields Info").set<int>("Number Of Fields",9);
  Teuchos::ParameterList& field0 = discretizationList.sublist("Required Fields Info").sublist("Field 0");
  Teuchos::ParameterList& field1 = discretizationList.sublist("Required Fields Info").sublist("Field 1");
  Teuchos::ParameterList& field2 = discretizationList.sublist("Required Fields Info").sublist("Field 2");
  Teuchos::ParameterList& field3 = discretizationList.sublist("Required Fields Info").sublist("Field 3");
  Teuchos::ParameterList& field4 = discretizationList.sublist("Required Fields Info").sublist("Field 4");
  Teuchos::ParameterList& field5 = discretizationList.sublist("Required Fields Info").sublist("Field 5");
  Teuchos::ParameterList& field6 = discretizationList.sublist("Required Fields Info").sublist("Field 6");
  Teuchos::ParameterList& field7 = discretizationList.sublist("Required Fields Info").sublist("Field 7");
  Teuchos::ParameterList& field8 = discretizationList.sublist("Required Fields Info").sublist("Field 8");

  //set temperature
  field0.set<std::string>("Field Name", "temperature");
  field0.set<std::string>("Field Type", "Elem Scalar");
  field0.set<std::string>("Field Origin", "Mesh");

  //set ice thickness
  field1.set<std::string>("Field Name", "ice_thickness");
  field1.set<std::string>("Field Type", "Node Scalar");
  field1.set<std::string>("Field Origin", "Mesh");

  //set surface_height
  field2.set<std::string>("Field Name", "surface_height");
  field2.set<std::string>("Field Type", "Node Scalar");
  field2.set<std::string>("Field Origin", "Mesh");

  //set bed topography
  field3.set<std::string>("Field Name", "bed_topography");
  field3.set<std::string>("Field Type", "Node Scalar");
  field3.set<std::string>("Field Origin", "Mesh");

  //set basal friction
  field4.set<std::string>("Field Name", "basal_friction");
  field4.set<std::string>("Field Type", "Node Scalar");
  field4.set<std::string>("Field Origin", "Mesh");

  //set surface mass balance
  field5.set<std::string>("Field Name", "surface_mass_balance");
  field5.set<std::string>("Field Type", "Node Scalar");
  field5.set<std::string>("Field Origin", "Mesh");

  //set dirichlet field
  field6.set<std::string>("Field Name", "dirichlet_field");
  field6.set<std::string>("Field Type", "Node Vector");
  field6.set<std::string>("Field Origin", "Mesh");
  
  //set stiffening factor
  field7.set<std::string>("Field Name", "stiffening_factor");
  field7.set<std::string>("Field Type", "Node Scalar");
  field7.set<std::string>("Field Origin", "Mesh");
  
  //set effective pressure
  field8.set<std::string>("Field Name", "effective_pressure");
  field8.set<std::string>("Field Type", "Node Scalar");
  field8.set<std::string>("Field Origin", "Mesh");

  discParams = Teuchos::sublist(paramList, "Discretization", true);

  Albany::AbstractFieldContainer::FieldContainerRequirements req;
  albanyApp = Teuchos::rcp(new Albany::Application(mpiComm));
  albanyApp->initialSetUp(paramList);

  int neq = (paramList->sublist("Problem").get<std::string>("Name") == "LandIce Coupled FO H 3D") ? 3 : 2;

  //temporary fix, TODO: use GO for indexToTriangleID (need to synchronize with MPAS).
  std::vector<GO> indexToTriangleGOID;
  indexToTriangleGOID.assign(indexToTriangleID.begin(), indexToTriangleID.end());
  meshStruct = Teuchos::rcp(
      new Albany::MpasSTKMeshStruct(discParams, mpiComm, indexToTriangleGOID,
          nGlobalTriangles, nLayers, Ordering));
  albanyApp->createMeshSpecs(meshStruct);

  albanyApp->buildProblem();

  meshStruct->constructMesh(mpiComm, discParams, neq, req,
      albanyApp->getStateMgr().getStateInfoStruct(), indexToVertexID,
      mpasIndexToVertexID, verticesCoords, isVertexBoundary, nGlobalVertices,
      verticesOnTria, isBoundaryEdge, trianglesOnEdge, trianglesPositionsOnEdge,
      verticesOnEdge, indexToEdgeID, nGlobalEdges, indexToTriangleGOID,
      dirichletNodesIds, floating2dEdgesIds,
      meshStruct->getMeshSpecs()[0]->worksetSize, nLayers, Ordering);
}
