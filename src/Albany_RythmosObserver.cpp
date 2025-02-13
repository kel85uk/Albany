//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_RythmosObserver.hpp"

#include "Albany_Application.hpp"
#include "Albany_ObserverImpl.hpp"
#include "Albany_EpetraThyraUtils.hpp"

#include "Rythmos_StepperBase.hpp"

#include "Thyra_DefaultProductVector.hpp"
#include "Thyra_VectorBase.hpp"
#include "Thyra_EpetraThyraWrappers.hpp"

#include "Epetra_Vector.h"

namespace Albany
{

RythmosObserver::RythmosObserver(const Teuchos::RCP<Application>& app_)
 : impl(new ObserverImpl(app_))
 , initial_step(true)
{
  // Nothing to do
}

void RythmosObserver::
observeStartTimeStep(const Rythmos::StepperBase<ScalarType>& stepper,
                     const Rythmos::StepControlInfo<ScalarType>& /* stepCtrlInfo */,
                     const int /* timeStepIter */)
{
  if(initial_step) {
    initial_step = false;
  } else {
    return;
  }

 //IK, 7/18/14: commented out the next line.  It was causing the 1st solution (initial condition) 
 //to not be written to the Exodus file.  The hack was in place b/c of the return in the end of the line.
//std::cout << "AGS:: Hack do not observe first step, time = "  << time << std::endl; return;

  // Print the initial condition

  Teuchos::RCP<const Thyra::DefaultProductVector<ScalarType> > solnandsens = 
    Teuchos::rcp_dynamic_cast<const Thyra::DefaultProductVector<ScalarType> >
      (stepper.getStepStatus().solution);
  Teuchos::RCP<const Thyra::DefaultProductVector<ScalarType> > solnandsens_dot = 
    Teuchos::rcp_dynamic_cast<const Thyra::DefaultProductVector<ScalarType> >
      (stepper.getStepStatus().solutionDot);

  Teuchos::RCP<const Thyra::VectorBase<ScalarType> > solution;
  Teuchos::RCP<const Thyra::VectorBase<ScalarType> > solution_dot;
  if (solnandsens != Teuchos::null) {
    solution = solnandsens->getVectorBlock(0);
    solution_dot = solnandsens_dot->getVectorBlock(0);
  }
  else {
    solution = stepper.getStepStatus().solution;
    solution_dot = stepper.getStepStatus().solutionDot;
  }

  // Time should be zero unless we are restarting
  const ScalarType time = impl->getTimeParamValueOrDefault(stepper.getStepStatus().time);

  if(solution_dot != Teuchos::null){
    impl->observeSolution(time, *solution, solution_dot.ptr(), Teuchos::null);
  } else {
    impl->observeSolution(time, *solution, Teuchos::null, Teuchos::null);
  }
}

void RythmosObserver::
observeCompletedTimeStep(const Rythmos::StepperBase<ScalarType>& stepper,
                         const Rythmos::StepControlInfo<ScalarType>& /* stepCtrlInfo */,
                         const int /* timeStepIter */)
{
  //cout << "ALBANY OBSERVER CALLED step=" <<  timeStepIter 
  //     << ",  time=" << stepper.getStepStatus().time << endl;

  Teuchos::RCP<const Thyra::DefaultProductVector<ScalarType> > solnandsens = 
    Teuchos::rcp_dynamic_cast<const Thyra::DefaultProductVector<ScalarType> >
      (stepper.getStepStatus().solution);
  Teuchos::RCP<const Thyra::DefaultProductVector<ScalarType> > solnandsens_dot = 
    Teuchos::rcp_dynamic_cast<const Thyra::DefaultProductVector<ScalarType> >
      (stepper.getStepStatus().solutionDot);

  Teuchos::RCP<const Thyra::VectorBase<ScalarType> > solution;
  Teuchos::RCP<const Thyra::VectorBase<ScalarType> > solution_dot;
  if (solnandsens != Teuchos::null) {
    solution = solnandsens->getVectorBlock(0);
// Next line bombs with BDF
    solution_dot = solnandsens_dot->getVectorBlock(0);
  }
  else {
    solution = stepper.getStepStatus().solution;
    solution_dot = stepper.getStepStatus().solutionDot;
  }

  const ScalarType time = impl->getTimeParamValueOrDefault(stepper.getStepStatus().time);

  // Comment out this section for BDF
  if(solution_dot != Teuchos::null) {
    impl->observeSolution(time, *solution, solution_dot.ptr(), Teuchos::null);
  } else {
    impl->observeSolution(time, *solution, Teuchos::null, Teuchos::null);
  }

  // Add this section for output of sensitivities instead of soution
  /*
  if (solnandsens != Teuchos::null) {
    std::cout << "AGS: Albany_RythmosObserver writing out sensitivity vectors: " 
              << solnandsens->productSpace()->numBlocks()-1 << std::endl;
    Teuchos::RCP<const Thyra::VectorBase<ScalarType> > sensvec;
    for (int i=1; i < solnandsens->productSpace()->numBlocks(); i++) {
      sensvec = solnandsens->getVectorBlock(i);
      const Epetra_Vector sens= *(Thyra::get_Epetra_Vector(impl.getNonOverlappedMap(), sensvec));
      // tweak time a little bit
      impl.observeSolution(time*(1.0 + 1.0e-4*i), sens, Teuchos::null);
    }
  }
  else  impl.observeSolution(time, soln, Teuchos::null);
  */
}

} // namespace Albany
