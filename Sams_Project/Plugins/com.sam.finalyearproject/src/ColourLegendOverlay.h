#ifndef Colour_Legend_Overlay_h
#define Colour_Legend_Overlay_h

#include <mitkLocalStorageHandler.h>
#include <mitkBaseRenderer.h>
#include <mitkVtkOverlay2D.h>
#include <vtkLegendBoxActor.h>

class ColourLegendOverlay : public mitk::VtkOverlay2D {
  public:
    // An inner class to store the actors needed for the legend.
    class LocalStorage : public mitk::Overlay::BaseLocalStorage {
      public:
        LocalStorage();
        ~LocalStorage();

        vtkSmartPointer<vtkLegendBoxActor> legendActor;
    };

  void setValue1(double value);
  void setValue2(double value);
  void setColour1(unsigned char red, unsigned char green, unsigned char blue);
  void setColour2(unsigned char red, unsigned char green, unsigned char blue);

  protected:
    mutable mitk::LocalStorageHandler<LocalStorage> localStorageHandler;
    virtual void UpdateVtkOverlay2D(mitk::BaseRenderer *renderer);
    virtual vtkActor2D * GetVtkActor2D(mitk::BaseRenderer *renderer) const;

  private:
    double value1;
    double value2;
    double colour1[3];
    double colour2[3];
};

#endif