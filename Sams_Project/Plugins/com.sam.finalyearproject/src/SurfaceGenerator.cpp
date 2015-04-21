#include "SurfaceGenerator.h"

#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTextureMapToSphere.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>

// Loading bar
#include <mitkProgressBar.h>

mitk::Surface::Pointer SurfaceGenerator::generateSphere(unsigned int resolution, unsigned int radius) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);
  
  // Create a sphere.
  vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
  sphere->SetThetaResolution(resolution);
  sphere->SetPhiResolution(resolution);
  sphere->SetRadius(radius);
  sphere->SetCenter(0, 0, 0);

  // Create a VTK texture map.
  vtkSmartPointer<vtkTextureMapToSphere> mapToSphere = vtkSmartPointer<vtkTextureMapToSphere>::New();
  mapToSphere->SetInputConnection(sphere->GetOutputPort());
  mapToSphere->PreventSeamOff();
  mapToSphere->Update();

  // Create an MITK surface from the texture map.
  mitk::Surface::Pointer surface = mitk::Surface::New();
  surface->SetVtkPolyData(static_cast<vtkPolyData*>(mapToSphere->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return surface;
}

mitk::Surface::Pointer SurfaceGenerator::generateCube(unsigned int length) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // Create a simple (VTK) cube.
  vtkSmartPointer<vtkCubeSource> cube = vtkSmartPointer<vtkCubeSource>::New();
  cube->SetXLength(length);
  cube->SetYLength(length);
  cube->SetZLength(length);
  cube->SetCenter(0.0, 0.0, 0.0);
  cube->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer cubeSurface = mitk::Surface::New();
  cubeSurface->SetVtkPolyData(static_cast<vtkPolyData*>(cube->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return cubeSurface;
}

mitk::Surface::Pointer SurfaceGenerator::generateCylinder(unsigned int radius, unsigned int height, unsigned int resolution) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);
  
  // Create a simple (VTK) cylinder.
  vtkSmartPointer<vtkCylinderSource> cylinder = vtkSmartPointer<vtkCylinderSource>::New();
  cylinder->SetRadius(radius);
  cylinder->SetHeight(height);
  cylinder->SetResolution(resolution);
  cylinder->SetCenter(0.0, 0.0, 0.0);
  cylinder->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer cylinderSurface = mitk::Surface::New();
  cylinderSurface->SetVtkPolyData(static_cast<vtkPolyData*>(cylinder->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return cylinderSurface;
}