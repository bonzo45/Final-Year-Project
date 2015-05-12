#include "itkCommand.h"

class MitkLoadingBarCommand : public itk::Command {
  public:
    itkNewMacro(MitkLoadingBarCommand);
 
  public:
    void Execute(itk::Object *caller, const itk::EventObject & event);
    void Execute(const itk::Object * object, const itk::EventObject & event);

  protected:
    MitkLoadingBarCommand();
    int stepsRemaining;
    static const bool DEBUGGING = false;
};