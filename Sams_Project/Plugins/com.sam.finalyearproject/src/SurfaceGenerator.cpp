#include "SurfaceGenerator.h"

#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTextureMapToSphere.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkPlaneSource.h>

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

mitk::Surface::Pointer SurfaceGenerator::generatePlane(vtkVector<float, 3> center, vtkVector<float, 3> normal, unsigned int scanWidth, unsigned int scanHeight) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // Create a plane.
  vtkSmartPointer<vtkPlaneSource> plane = vtkSmartPointer<vtkPlaneSource>::New();
  
  cout << "Creating Scan Plane:" << endl;
  cout << "Origin: (0, 0, 0)" << endl;
  cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
  cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;

  // Get the size sorted.
  plane->SetOrigin(0, 0, 0);
  plane->SetPoint1(scanWidth, 0, 0);
  plane->SetPoint2(0, scanHeight, 0);

  cout << "Initial Plane:" << endl;
  cout << "Origin: (0, 0, 0)" << endl;
  cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
  cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;

  // Then shift it to be in the right place.
  plane->SetCenter(center[0], center[1], center[2]);

  cout << "After Center: (" << center[0] << ", " << center[1] << ", " << center[2] << ")" << endl;
  cout << "Origin: (0, 0, 0)" << endl;
  cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
  cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;

  plane->SetNormal(normal[0], normal[1], normal[2]);
  
  cout << "After Normal:" << endl;
  cout << "Origin: (0, 0, 0)" << endl;
  cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
  cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;

  plane->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer planeSurface = mitk::Surface::New();
  planeSurface->SetVtkPolyData(static_cast<vtkPolyData*>(plane->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return planeSurface;
}