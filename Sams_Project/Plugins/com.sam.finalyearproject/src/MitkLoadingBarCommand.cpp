#include "MitkLoadingBarCommand.h"

#include <itkProcessObject.h>

// Loading bar
#include <mitkProgressBar.h>

MitkLoadingBarCommand::MitkLoadingBarCommand() {
  initialized = false;
}

/**
  * Use initialize to configure the instance.
  *   stepsToDo - how many steps should this filter be. (default 100)
  *     this maps the progress 0-100% into stepsToDo steps.
  *   addSteps - should the filter add the steps to the loading bar (default true)
  *     e.g. should this class call
  *       mitk::ProgressBar::GetInstance()->AddStepsToDo(stepsToDo);
  *     If this filter is part of a large operation with many filters then set this to false and add
  *     the combined steps for all of the filters in one go externally.
  */
void MitkLoadingBarCommand::Initialize(unsigned int stepsToDo, bool addSteps) {
  initialized = true;
  this->totalSteps = stepsToDo;
  this->stepsRemaining = stepsToDo;
  if (addSteps) {
    mitk::ProgressBar::GetInstance()->AddStepsToDo(stepsToDo);
  }
}

/**
  * Called when an event (some progress has been made) happens.
  */
void MitkLoadingBarCommand::Execute(itk::Object *caller, const itk::EventObject & event) {
  Execute((const itk::Object *)caller, event);
}

/**
  * The const version of the above method.
  * I'm not sure if this method is actually required but if it ain't broken -> don't fix it.
  */
void MitkLoadingBarCommand::Execute(const itk::Object * object, const itk::EventObject & event) {      
  // If initialize hasn't been called, call the default version.
  if (!initialized) {
    Initialize();
  }

  itk::ProcessObject * processObject = (itk::ProcessObject *) object;

  // Calculate how many steps we've done since last time and progress the loading bar.
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
    std::cerr << "MitkLoadingBarCommand is watching for ProgressEvent but received something else." << std::endl;
  }
}