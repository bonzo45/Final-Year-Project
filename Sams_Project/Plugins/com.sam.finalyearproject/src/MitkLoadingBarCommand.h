#include "itkCommand.h"

// Loading bar
#include <mitkProgressBar.h>

class MitkLoadingBarCommand : public itk::Command {
  public:
    itkNewMacro(MitkLoadingBarCommand);
 
  public:

    void Execute(itk::Object *caller, const itk::EventObject & event) {
      Execute((const itk::Object *)caller, event);
    }
 
    void Execute(const itk::Object * object, const itk::EventObject & event) {      
      itk::ProcessObject * processObject = (itk::ProcessObject *) object;

      try {
        const itk::ProgressEvent & progressEvent = dynamic_cast<const itk::ProgressEvent &>(event);
        int newStepsRemaining = 100 - (100.0 * processObject->GetProgress());
        int stepsCompleted = stepsRemaining - newStepsRemaining;
        mitk::ProgressBar::GetInstance()->Progress(stepsCompleted);
        if (DEBUGGING) {
          std::cout << 
                    "stepsRemaining: " << stepsRemaining <<
                    ", progress: " << 100.0 * processObject->GetProgress() <<
                    ", stepsCompleted: " << stepsCompleted <<
                    ", newStepsRemaining: " << newStepsRemaining << std::endl;
        }
        stepsRemaining = newStepsRemaining;
      }
      catch(std::bad_cast exp) {
        std::cout << "It's not a ProgressEvent..." << std::endl;
      }
    }

  protected:
    MitkLoadingBarCommand() {
      mitk::ProgressBar::GetInstance()->AddStepsToDo(100);
      this->stepsRemaining = 100;
    }

    int stepsRemaining;
    static const bool DEBUGGING = false;
};