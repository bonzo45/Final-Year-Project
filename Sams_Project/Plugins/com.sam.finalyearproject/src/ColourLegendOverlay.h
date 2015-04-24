#ifndef Colour_Legend_Overlay_h
#define Colour_Legend_Overlay_h

#include <mitkLocalStorageHandler.h>
#include <mitkBaseRenderer.h>
#include <mitkVtkOverlay2D.h>
#include <vtkLegendBoxActor.h>

class ColourLegendOverlay : public mitk::VtkOverlay2D {
  public:
    // An inner class to store all the actors needed for the legend.
    class LocalStorage : public mitk::Overlay::BaseLocalStorage {
      public:
        LocalStorage();
        ~LocalStorage();

        vtkSmartPointer<vtkLegendBoxActor> legendActor;
        // vtkSmartPointer<vtkImageData> m_textImage;
        // vtkSmartPointer<vtkImageMapper> m_imageMapper;
    };

  protected:
    mutable mitk::LocalStorageHandler<LocalStorage> localStorageHandler;

    virtual void UpdateVtkOverlay2D(mitk::BaseRenderer *renderer);
    virtual vtkActor2D * GetVtkActor2D(mitk::BaseRenderer *renderer) const;
};

#endif