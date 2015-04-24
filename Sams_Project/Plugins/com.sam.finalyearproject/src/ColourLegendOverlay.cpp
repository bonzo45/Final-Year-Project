#include <ColourLegendOverlay.h>

#include <vtkCubeSource.h>

ColourLegendOverlay::LocalStorage::LocalStorage() {
  legendActor = vtkSmartPointer<vtkLegendBoxActor>::New();
}

ColourLegendOverlay::LocalStorage::~LocalStorage() {

}

void ColourLegendOverlay::UpdateVtkOverlay2D(mitk::BaseRenderer *renderer) {
  LocalStorage * ls = this->localStorageHandler.GetLocalStorage(renderer);

  // Check if we need to create the legend prop.
  if (ls->IsGenerateDataRequired(renderer, this)) {
    // Create legend.
    vtkSmartPointer<vtkLegendBoxActor> legend = ls->legendActor;
    legend->SetNumberOfEntries(2);
      
    // Entry 1.
    double color1[4] = {1.0, 0.0, 1.0, 1.0};
    vtkSmartPointer<vtkCubeSource> legendBox1 = vtkSmartPointer<vtkCubeSource>::New();
    legendBox1->Update();
    legend->SetEntry(0, legendBox1->GetOutput(), "First", color1);
   
    // Entry 2.
    double color2[4] = {1.0, 0.5, 0.5, 1.0};
    vtkSmartPointer<vtkCubeSource> legendBox2 = vtkSmartPointer<vtkCubeSource>::New();
    legendBox2->Update();
    legend->SetEntry(1, legendBox2->GetOutput(), "Second", color2);
   
    // Position. (place legend in lower right)
    legend->GetPositionCoordinate()->SetCoordinateSystemToView();
    legend->GetPositionCoordinate()->SetValue(0.5, -1.0);
   
    legend->GetPosition2Coordinate()->SetCoordinateSystemToView();
    legend->GetPosition2Coordinate()->SetValue(1.0, -0.5);
   
    // Background.
    legend->UseBackgroundOn();
    double background[4] = {0.2, 0.2, 0.2, 0.5};
    legend->SetBackgroundColor(background);

    ls->UpdateGenerateDataTime();
  }
}

vtkActor2D * ColourLegendOverlay::GetVtkActor2D(mitk::BaseRenderer * renderer) const {
  LocalStorage * localStorage = this->localStorageHandler.GetLocalStorage(renderer);
  return localStorage->legendActor;
}