#include "SurfaceGenerator.h"

#include "Util.h"

#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTextureMapToSphere.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkPlaneSource.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

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

mitk::Surface::Pointer SurfaceGenerator::generatePlane(unsigned int scanWidth, unsigned int scanHeight, vtkVector<float, 3> center, vtkVector<float, 3> normal, vtkVector<float, 3> & newXAxis, vtkVector<float, 3> & newYAxis) {
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

  plane->SetNormal(normal[0], normal[1], normal[2]);
  
  cout << "After Normal: (" << normal[0] << ", " << normal[1] << ", " << normal[2] << ")" << endl;
  cout << "Origin: (0, 0, 0)" << endl;
  cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
  cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;

  vtkVector<float, 3> oldNormal = vtkVector<float, 3>();
  oldNormal[0] = 0;
  oldNormal[1] = 0;
  oldNormal[2] = 1;

  vtkVector<float, 3> rotationVector = Util::vectorCross(oldNormal, normal);
  newXAxis[0] = rotationVector[0];
  newXAxis[1] = rotationVector[1];
  newXAxis[2] = rotationVector[2];

  vtkVector<float, 3> hmmmm = Util::vectorCross(normal, rotationVector);
  newYAxis[0] = hmmmm[0];
  newYAxis[1] = hmmmm[1];
  newYAxis[2] = hmmmm[2];

  newYAxis.Normalize();
  newXAxis.Normalize();

  // Then shift it to be in the right place.
  plane->SetCenter(center[0], center[1], center[2]);

  cout << "After Center: (" << center[0] << ", " << center[1] << ", " << center[2] << ")" << endl;
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

mitk::Surface::Pointer SurfaceGenerator::generateCuboid(unsigned int height, unsigned int width, unsigned int depth, vtkVector<float, 3> center, vtkVector<float, 3> newXAxis, vtkVector<float, 3> newYAxis, vtkVector<float, 3> newZAxis) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // Create a simple (VTK) box.
  vtkSmartPointer<vtkCubeSource> cube = vtkSmartPointer<vtkCubeSource>::New();
  cube->SetXLength(height);
  cube->SetYLength(width);
  cube->SetZLength(depth);
  cube->SetCenter(0, 0, 0);
  cube->Update();

  // Transform it to be centered at center and be aligned by new axes given.
  vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
  // Row 1
  matrix->SetElement(0, 0, newXAxis[0]);
  matrix->SetElement(0, 1, newXAxis[1]);
  matrix->SetElement(0, 2, newXAxis[2]);
  matrix->SetElement(0, 3, center[0]);
  // Row 2
  matrix->SetElement(1, 0, newYAxis[0]);
  matrix->SetElement(1, 1, newYAxis[1]);
  matrix->SetElement(1, 2, newYAxis[2]);
  matrix->SetElement(1, 3, center[1]);
  // Row 3
  matrix->SetElement(2, 0, newZAxis[0]);
  matrix->SetElement(2, 1, newZAxis[1]);
  matrix->SetElement(2, 2, newZAxis[2]);
  matrix->SetElement(2, 3, center[2]);
  // Row 4
  matrix->SetElement(3, 0, 0);
  matrix->SetElement(3, 1, 0);
  matrix->SetElement(3, 2, 0);
  matrix->SetElement(3, 3, 1);

  cout << "Matrix:" << endl;
  for (unsigned int i = 0; i < 4; i++) {
    cout << "(";
    for (unsigned int j = 0; j < 4; j++) {
      cout << matrix->GetElement(i, j) << ", ";
    }
    cout << ")" << endl;
  }

  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->SetMatrix(matrix);

  vtkSmartPointer<vtkTransformPolyDataFilter> transformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  transformFilter->SetInputData(cube->GetOutput());
  transformFilter->SetTransform(transform);
  transformFilter->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer cubeSurface = mitk::Surface::New();
  cubeSurface->SetVtkPolyData(static_cast<vtkPolyData*>(transformFilter->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return cubeSurface;
}