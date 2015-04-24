#include "ColourLegendOverlay.h"

#include "Util.h"

#include <vtkCubeSource.h>

ColourLegendOverlay::LocalStorage::LocalStorage() {
  legendActor = vtkSmartPointer<vtkLegendBoxActor>::New();
}

ColourLegendOverlay::LocalStorage::~LocalStorage() {

}

void ColourLegendOverlay::setValue1(double value) {
  this->value1 = value;
  somethingChanged = true;
}

void ColourLegendOverlay::setValue2(double value) {
  this->value2 = value;
  somethingChanged = true;
}

void ColourLegendOverlay::setColour1(unsigned char red, unsigned char green, unsigned char blue) {
  colour1[0] = red / 255.0;
  colour1[1] = green / 255.0;
  colour1[2] = blue / 255.0;
  somethingChanged = true;
}

void ColourLegendOverlay::setColour2(unsigned char red, unsigned char green, unsigned char blue) {
  colour2[0] = red / 255.0;
  colour2[1] = green / 255.0;
  colour2[2] = blue / 255.0;
  somethingChanged = true;
}

void ColourLegendOverlay::UpdateVtkOverlay2D(mitk::BaseRenderer *renderer) {
  LocalStorage * ls = this->localStorageHandler.GetLocalStorage(renderer);

  // Check if we need to create the legend prop.
  if (ls->IsGenerateDataRequired(renderer, this) || somethingChanged) {
    somethingChanged = false;
    
    // Create legend.
    vtkSmartPointer<vtkLegendBoxActor> legend = ls->legendActor;
    legend->SetNumberOfEntries(2);
      
    // Entry 1.
    vtkSmartPointer<vtkCubeSource> legendBox1 = vtkSmartPointer<vtkCubeSource>::New();
    legendBox1->SetCenter(-0.5, 0.5, 0.0);
    legendBox1->SetXLength(2.0);
    legendBox1->Update();
    legend->SetEntry(0, legendBox1->GetOutput(), Util::StringFromDouble(value1).c_str(), colour1);
   
    // Entry 2.
    vtkSmartPointer<vtkCubeSource> legendBox2 = vtkSmartPointer<vtkCubeSource>::New();
    legendBox2->SetCenter(-0.5, 0.5, 0.0);
    legendBox2->SetXLength(2.0);
    legendBox2->Update();
    legend->SetEntry(1, legendBox2->GetOutput(), Util::StringFromDouble(value2).c_str(), colour2);
   
    // // Position. (place legend in lower right)
    // legend->GetPositionCoordinate()->SetCoordinateSystemToView();
    // legend->GetPositionCoordinate()->SetValue(0.5, -1.0);
   
    // legend->GetPosition2Coordinate()->SetCoordinateSystemToView();
    // legend->GetPosition2Coordinate()->SetValue(1.0, -0.5);
   
    // Background/Border/Padding.
    legend->UseBackgroundOn();
    legend->BorderOff();
    double background[3] = {0.2, 0.2, 0.2};
    legend->SetBackgroundColor(background);
    legend->SetBackgroundOpacity(0.5);
    legend->SetPadding(10);

    ls->UpdateGenerateDataTime();
  }
}

vtkActor2D * ColourLegendOverlay::GetVtkActor2D(mitk::BaseRenderer * renderer) const {
  LocalStorage * localStorage = this->localStorageHandler.GetLocalStorage(renderer);
  return localStorage->legendActor;
}