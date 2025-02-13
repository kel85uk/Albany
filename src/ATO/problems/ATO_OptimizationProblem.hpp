//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ATO_OPTIMIZATION_PROBLEM_HPP
#define ATO_OPTIMIZATION_PROBLEM_HPP

// ATO includes
#include "ATO_Types.hpp"

// Albany includes
#include "Albany_AbstractProblem.hpp"

// System includes
#include <unordered_map>

namespace ATO {

// ATO forward declarations
class MeasureModel;
class Topology;

using BlockMeasureMap = std::unordered_map<std::string, Teuchos::RCP<MeasureModel>>;

class OptimizationProblem : public virtual Albany::AbstractProblem
{
public:
  OptimizationProblem( const Teuchos::RCP<Teuchos::ParameterList>& _params,
                       const Teuchos::RCP<ParamLib>& _paramLib,
                       const int _numDim);
  void ComputeVolume(double* p, const double* dfdp,
                     double& v, double threshhold, double minP);

  void ComputeMeasure (const std::string& measure, 
                       const std::vector<Teuchos::RCP<TopologyStruct> >& topologyStructs,
                       double& v, double* dvdp = nullptr, 
                       const std::string& strIntegrationMethod = "Gauss Quadrature");

  void computeMeasure (const std::string& measure, 
                       const std::vector<Teuchos::RCP<TopologyStruct>>& topologyStructs,
                       double& v, double* dvdp = nullptr);

  void computeConformalVolume (const std::vector<Teuchos::RCP<TopologyStruct>>& topologyStructs,
                               double& m, double* dmdp);

  void computeConformalMeasure (const std::string& measure, 
                                const std::vector<Teuchos::RCP<TopologyStruct>>& topologyStructs,
                                double& m, double* dmdp);

  void ComputeMeasure (const std::string& measure, double& v);

  void setDiscretization(Teuchos::RCP<Albany::AbstractDiscretization> _disc) {
     disc = _disc;
  }

  void setCommunicator(const Teuchos::RCP<const Teuchos_Comm>& _comm) {
    comm = _comm;
  }


  void InitTopOpt();

protected:

  void setupTopOpt( Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  _meshSpecs,
                    Albany::StateManager& _stateMgr);

  Teuchos::RCP<Albany::AbstractDiscretization> disc;
  Teuchos::RCP<const Teuchos_Comm> comm;

  Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> > meshSpecs;
  Albany::StateManager* stateMgr;

  std::vector<Teuchos::RCP<shards::CellTopology> >                                cellTypes;
  std::vector<Teuchos::RCP<Intrepid2::Cubature<PHX::Device> > >                   cubatures;
  std::vector<Teuchos::RCP<Intrepid2::Basis<PHX::Device, RealType, RealType> > >  intrepidBasis;


  std::vector<Kokkos::DynRankView<RealType, PHX::Device> > refPoints;
  std::vector<Kokkos::DynRankView<RealType, PHX::Device> > refWeights;
  std::vector<Kokkos::DynRankView<RealType, PHX::Device> > basisAtQPs;
  std::vector<Kokkos::DynRankView<RealType, PHX::Device> > weighted_measure;

  Teuchos::RCP<Thyra_Vector> overlapVec;
  Teuchos::RCP<Thyra_Vector> localVec;
  Teuchos::RCP<Albany::CombineAndScatterManager> cas_manager;

  Teuchos::Array<Teuchos::RCP<Thyra_Vector> > overlapVectors;

  Teuchos::RCP<const Thyra_VectorSpace> localNodeVs;
  Teuchos::RCP<const Thyra_VectorSpace> overlapNodeVs;


  std::unordered_map<std::string, Teuchos::RCP<BlockMeasureMap> > measureModels;

//  std::string strIntegrationMethod;

  int nTopologies;

  bool isNonconformal;
};

class MeasureModel
{
  public:
    MeasureModel(){};
    virtual ~MeasureModel(){};

    virtual double Evaluate(const Teuchos::Array<double>& inVals,
                            Teuchos::Array<Teuchos::RCP<Topology> >& topologies)=0;

    virtual void Gradient(const Teuchos::Array<double>& inVals, 
                          Teuchos::Array<Teuchos::RCP<Topology> >& topologies,
                          Teuchos::Array<double>& outVals)=0;

    // temporary compromise (aka, hack) until comformal integration is handled correctly for mixtures
    int getMaterialTopologyIndex(){return materialTopologyIndex;}
  protected:
    int materialTopologyIndex;
    int materialFunctionIndex;
};

class TopologyWeightedIntegral_Mixture : public MeasureModel
{
  public:
    TopologyWeightedIntegral_Mixture(const Teuchos::ParameterList& blockParams,
                                     const Teuchos::ParameterList& measureParams );

    double Evaluate(const Teuchos::Array<double>& inVals,
                    Teuchos::Array<Teuchos::RCP<Topology> >& topologies);
    void Gradient(const Teuchos::Array<double>& inVals, 
                  Teuchos::Array<Teuchos::RCP<Topology> >& topologies,
                  Teuchos::Array<double>& outVals);
 
  private:
    Teuchos::Array<double> parameterValues;
    Teuchos::Array<int> materialIndices;
    Teuchos::Array<int> mixtureTopologyIndices;
    Teuchos::Array<int> mixtureFunctionIndices;
};

class TopologyWeightedIntegral_Material : public MeasureModel
{
  public:
    TopologyWeightedIntegral_Material(const Teuchos::ParameterList& blockParams,
                                      const Teuchos::ParameterList& measureParams );

    double Evaluate(const Teuchos::Array<double>& inVals,
                    Teuchos::Array<Teuchos::RCP<Topology> >& topologies);
    void Gradient(const Teuchos::Array<double>& inVals, 
                  Teuchos::Array<Teuchos::RCP<Topology> >& topologies,
                  Teuchos::Array<double>& outVals);
 
  private:
    double parameterValue;
};

class VolumeMeasure : public MeasureModel
{
  public:
    VolumeMeasure(const Teuchos::ParameterList& blockParams,
                  const Teuchos::ParameterList& measureParams );

    double Evaluate(const Teuchos::Array<double>& inVals,
                    Teuchos::Array<Teuchos::RCP<Topology> >& topologies);
    void Gradient(const Teuchos::Array<double>& inVals, 
                  Teuchos::Array<Teuchos::RCP<Topology> >& topologies,
                  Teuchos::Array<double>& outVals);
};

class TopologyBasedMixture : public MeasureModel
{
  public:
    TopologyBasedMixture(const Teuchos::ParameterList& blockParams );
    double Evaluate(const Teuchos::Array<double>& inVals,
                    Teuchos::Array<Teuchos::RCP<Topology> >& topologies);
    void Gradient(const Teuchos::Array<double>& inVals, 
                  Teuchos::Array<Teuchos::RCP<Topology> >& topologies,
                  Teuchos::Array<double>& outVals);
    
};

class TopologyBasedMaterial : public MeasureModel
{
  public:
    TopologyBasedMaterial(const Teuchos::ParameterList& blockParams );
    double Evaluate(const Teuchos::Array<double>& inVals,
                    Teuchos::Array<Teuchos::RCP<Topology> >& topologies);
    void Gradient(const Teuchos::Array<double>& inVals, 
                  Teuchos::Array<Teuchos::RCP<Topology> >& topologies,
                  Teuchos::Array<double>& outVals);
    
};

class MeasureModelFactory 
{ 
  public:
    MeasureModelFactory( Teuchos::ParameterList _configParams );

    Teuchos::RCP<BlockMeasureMap> create(const Teuchos::ParameterList& measureParams );
 
  private:
    Teuchos::ParameterList configParams;
    
};

} // namespace ATO

#endif // ATO_OPTIMIZATION_PROBLEM_HPP
