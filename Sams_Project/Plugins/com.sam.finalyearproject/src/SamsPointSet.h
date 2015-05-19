#ifndef Sams_Point_Set_h
#define Sams_Point_Set_h

#include <mitkPointSet.h>

class SamsPointSet : public mitk::PointSet {
  public:
    mitkClassMacro(SamsPointSet, mitk::PointSet);
    itkFactorylessNewMacro(Self)

    virtual void OnPointSetChange();
};

#endif