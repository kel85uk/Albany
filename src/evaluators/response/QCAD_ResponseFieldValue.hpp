//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef QCAD_RESPONSEFIELDVALUE_HPP
#define QCAD_RESPONSEFIELDVALUE_HPP

#include "QCAD_MeshRegion.hpp"
#include "Albany_MaterialDatabase.hpp"
#include "PHAL_ScatterScalarResponse.hpp"

namespace QCAD {

  template<typename EvalT, typename Traits>
  class FieldValueScatterScalarResponse :
    public PHAL::ScatterScalarResponse<EvalT,Traits>  {

  public:

    FieldValueScatterScalarResponse(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
      PHAL::ScatterScalarResponse<EvalT,Traits>(p,dl) {}

  protected:

    // Default constructor for child classes
    FieldValueScatterScalarResponse() :
      PHAL::ScatterScalarResponse<EvalT,Traits>() {}

    // Child classes should call setup once p is filled out
    void setup(const Teuchos::ParameterList& p,
               const Teuchos::RCP<Albany::Layouts>& dl) {
      PHAL::ScatterScalarResponse<EvalT,Traits>::setup(p,dl);
    }

    // Set NodeID structure for cell corrsponding to max/min
    void setMaxCell(const int) {}

    Teuchos::Array<int> field_components;
  };


  template<typename Traits>
  class FieldValueScatterScalarResponse<PHAL::AlbanyTraits::Jacobian,Traits> :
    public PHAL::ScatterScalarResponseBase<PHAL::AlbanyTraits::Jacobian,Traits> {

  public:
    typedef PHAL::AlbanyTraits::Jacobian EvalT;
    typedef typename EvalT::ScalarT ScalarT;

    FieldValueScatterScalarResponse(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
      PHAL::ScatterScalarResponseBase<EvalT,Traits>(p,dl) {}

    void preEvaluate(typename Traits::PreEvalData d) {}
    void evaluateFields(typename Traits::EvalData d) {}
    void postEvaluate(typename Traits::PostEvalData d);

  protected:

    // Default constructor for child classes
    FieldValueScatterScalarResponse() :
      PHAL::ScatterScalarResponseBase<EvalT,Traits>() {}

    // Child classes should call setup once p is filled out
    void setup(const Teuchos::ParameterList& p,
               const Teuchos::RCP<Albany::Layouts>& dl) {
      PHAL::ScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
      numNodes = dl->node_scalar->extent(1);
    }

    // Set NodeID structure for cell corrsponding to max/min
    void setMaxCell(const int max_cell_) {
      max_cell = max_cell_;
    }

    Teuchos::Array<int> field_components;

  private:

    int max_cell = -1;
    int numNodes;
  };

  template<typename Traits>
  class FieldValueScatterScalarResponse<PHAL::AlbanyTraits::DistParamDeriv,Traits> :
    public PHAL::ScatterScalarResponseBase<PHAL::AlbanyTraits::DistParamDeriv,Traits> {

  public:
    typedef PHAL::AlbanyTraits::DistParamDeriv EvalT;
    typedef typename EvalT::ScalarT ScalarT;

    FieldValueScatterScalarResponse(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
      PHAL::ScatterScalarResponseBase<EvalT,Traits>(p,dl) {}

    void preEvaluate(typename Traits::PreEvalData d) {}
    void evaluateFields(typename Traits::EvalData d) {}
    void postEvaluate(typename Traits::PostEvalData d);

  protected:

    // Default constructor for child classes
    FieldValueScatterScalarResponse() :
      PHAL::ScatterScalarResponseBase<EvalT,Traits>() {}

    // Child classes should call setup once p is filled out
    void setup(const Teuchos::ParameterList& p,
               const Teuchos::RCP<Albany::Layouts>& dl) {
      PHAL::ScatterScalarResponseBase<EvalT,Traits>::setup(p,dl);
      numNodes = dl->node_scalar->extent(1);
    }

    // Set NodeID structure for cell corrsponding to max/min
    void setMaxCell(const int max_cell_) {
      max_cell = max_cell_;
    }

    Teuchos::Array<int> field_components;

  private:

    int max_cell = -1;
    int numNodes;
  };

/**
 * \brief Response Description
 */
  template<typename EvalT, typename Traits>
  class ResponseFieldValue :
    public FieldValueScatterScalarResponse<EvalT, Traits>
  {
  public:
    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;

    ResponseFieldValue(Teuchos::ParameterList& p,
                       const Teuchos::RCP<Albany::Layouts>& dl);

    void postRegistrationSetup(typename Traits::SetupData d,
                               PHX::FieldManager<Traits>& vm);

    void preEvaluate(typename Traits::PreEvalData d);

    void evaluateFields(typename Traits::EvalData d);

    void postEvaluate(typename Traits::PostEvalData d);

  private:
    Teuchos::RCP<const Teuchos::ParameterList> getValidResponseParameters() const;

    std::size_t numQPs;
    std::size_t numDims;

    PHX::MDField<const ScalarT> opField;
    PHX::MDField<const ScalarT> retField;
    PHX::MDField<const MeshScalarT,Cell,QuadPoint,Dim> coordVec;
    PHX::MDField<const MeshScalarT,Cell,QuadPoint> weights;
    int max_cell;
    //Teuchos::Array<int> field_components;

    bool bOpFieldIsVector, bRetFieldIsVector;

    std::string operation;
    std::string opFieldName;
    std::string retFieldName;

    bool bReturnOpField;
    bool opX, opY, opZ;

    Teuchos::RCP< MeshRegion<EvalT, Traits> > opRegion;

    Teuchos::Array<double> initVals;

    //! Material database
    Teuchos::RCP<Albany::MaterialDatabase> materialDB;
  };

}

#endif
