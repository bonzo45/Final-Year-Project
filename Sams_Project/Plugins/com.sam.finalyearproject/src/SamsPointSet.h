#ifndef Sams_Point_Set_h
#define Sams_Point_Set_h

#include <mitkPointSet.h>
#include <mitkOperation.h>
#include "internal/Sams_View.h"

class SamsPointSet : public mitk::PointSet {
  public:
    mitkClassMacro(SamsPointSet, mitk::PointSet);
    itkFactorylessNewMacro(Self)

    void SetSamsView(Sams_View * samsView);

    virtual void ExecuteOperation(mitk::Operation * operation);
    virtual void OnPointSetChange();

  private:
    Sams_View * samsView;

    static const bool DEBUGGING = true;
};

#endif