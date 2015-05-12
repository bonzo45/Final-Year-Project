#include "MitkLoadingBarCommand.h"

#include <itkProcessObject.h>

// Loading bar
#include <mitkProgressBar.h>

MitkLoadingBarCommand::MitkLoadingBarCommand() {
  initialized = false;
}

void MitkLoadingBarCommand::Initialize(unsigned int stepsToDo, bool addSteps) {
  initialized = true;
  this->totalSteps = stepsToDo;
  this->stepsRemaining = stepsToDo;
  if (addSteps) {
    mitk::ProgressBar::GetInstance()->AddStepsToDo(stepsToDo);
  }
}

void MitkLoadingBarCommand::Execute(itk::Object *caller, const itk::EventObject & event) {
  Execute((const itk::Object *)caller, event);
}

void MitkLoadingBarCommand::Execute(const itk::Object * object, const itk::EventObject & event) {      
  if (!initialized) {
    Initialize(100, true);
  }

  itk::ProcessObject * processObject = (itk::ProcessObject *) object;

  try {
    const itk::ProgressEvent & progressEvent = dynamic_cast<const itk::ProgressEvent &>(event);
    int newStepsRemaining = totalSteps - (totalSteps * processObject->GetProgress());
    int stepsCompleted = stepsRemaining - newStepsRemaining;
    mitk::ProgressBar::GetInstance()->Progress(stepsCompleted);
    if (DEBUGGING) {
      std::cout << 
                "stepsRemaining: " << stepsRemaining <<
                ", progress: " << totalSteps * processObject->GetProgress() <<
                ", stepsCompleted: " << stepsCompleted <<
                ", newStepsRemaining: " << newStepsRemaining << std::endl;
    }
    stepsRemaining = newStepsRemaining; 
  }
  catch(std::bad_cast exp) {
    std::cout << "It's not a ProgressEvent..." << std::endl;
  }
}