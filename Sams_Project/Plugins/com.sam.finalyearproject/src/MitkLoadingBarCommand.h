#include <itkCommand.h>
#include <itkSmartPointer.h>

class MitkLoadingBarCommand : public itk::Command {
  public:

    typedef MitkLoadingBarCommand           Self;
    typedef itk::Command                    Superclass;
    typedef itk::SmartPointer<Self>         Pointer;
    typedef itk::SmartPointer<const Self>   ConstPointer;
    itkNewMacro(MitkLoadingBarCommand);
    itkTypeMacro(MitkLoadingBarCommand, itk::Command)
 
  public:
    void Initialize(unsigned int stepsToDo, bool addSteps);
    void Execute(itk::Object *caller, const itk::EventObject & event);
    void Execute(const itk::Object * object, const itk::EventObject & event);

  protected:
    MitkLoadingBarCommand();

    unsigned int totalSteps;
    unsigned int stepsRemaining;
    bool initialized;
    static const bool DEBUGGING = false;
};