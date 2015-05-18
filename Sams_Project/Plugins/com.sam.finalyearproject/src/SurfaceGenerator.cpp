#include "SurfaceGenerator.h"

#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTextureMapToSphere.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkPlaneSource.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkRegularPolygonSource.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkTubeFilter.h>

// Loading bar
#include <mitkProgressBar.h>

/**
  * Generates a sphere surface.
  *   thetaResolution - the number of points used horizontally to create the sphere.
  *   phiResolution - the number of points used vertically to create the sphere.
  *   radius - the width of the sphere.
  */
mitk::Surface::Pointer SurfaceGenerator::generateSphere(unsigned int thetaResolution, unsigned int phiResolution, unsigned int radius) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);
  
  // Create a sphere.
  vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
  sphere->SetThetaResolution(thetaResolution);
  sphere->SetPhiResolution(phiResolution);
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

/**
  * Creates a cube, centered at (0, 0, 0) with specified side length.
  */
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

/**
  * Creates a cylinder with specified radius, height and resolution.
  * (resolution equivalent to thetaResolution for spheres)
  */
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

/**
  * Creates a plane with specified width and height at center - default (0, 0, 0) - facing normal - default (0, 0, 1).
  * There x and y axes of the plane could be anything. The z-axis of the plane will be the normal.
  */
mitk::Surface::Pointer SurfaceGenerator::generatePlane(unsigned int width, unsigned int height, vtkVector<float, 3> center, vtkVector<float, 3> normal) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // Create a plane.
  vtkSmartPointer<vtkPlaneSource> plane = vtkSmartPointer<vtkPlaneSource>::New();
  
  if (DEBUGGING) {
    cout << "Creating Scan Plane:" << endl;
    cout << "Origin: (0, 0, 0)" << endl;
    cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
    cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;
  }

  // Set the plane size.
  plane->SetOrigin(0, 0, 0);
  plane->SetPoint1(width, 0, 0);
  plane->SetPoint2(0, height, 0);

  if (DEBUGGING) {
    cout << "Initial Plane:" << endl;
    cout << "Origin: (0, 0, 0)" << endl;
    cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
    cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;
  }

  // If the normal isn't set then make it (0, 0, 1).
  if (normal[0] == 0.0f && normal[1] == 0.0f && normal[2] == 0.0f) {
    normal[2] = 1.0;
  }

  // Point it in the direction of the normal.
  plane->SetNormal(normal[0], normal[1], normal[2]);
  
  if (DEBUGGING) {
    cout << "After Setting Normal: (" << normal[0] << ", " << normal[1] << ", " << normal[2] << ")" << endl;
    cout << "Origin: (0, 0, 0)" << endl;
    cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
    cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;
  }

  // Then shift it to be in the right place.
  plane->SetCenter(center[0], center[1], center[2]);

  if (DEBUGGING) {
    cout << "After Setting Center: (" << center[0] << ", " << center[1] << ", " << center[2] << ")" << endl;
    cout << "Origin: (0, 0, 0)" << endl;
    cout << "Point 1: (" << plane->GetPoint1()[0] << ", " << plane->GetPoint1()[1] << ", " << plane->GetPoint1()[2] << ")" << endl;
    cout << "Point 2: (" << plane->GetPoint2()[0] << ", " << plane->GetPoint2()[1] << ", " << plane->GetPoint2()[2] << ")" << endl;
  }

  plane->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer planeSurface = mitk::Surface::New();
  planeSurface->SetVtkPolyData(static_cast<vtkPolyData*>(plane->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return planeSurface;
}

/**
  * Generates a cuboid with specified width, height and depth.
  * Cuboid will be centered at center - default (0, 0, 0).
  * Cuboid will be rotated to specified axes - defaults to standard (1, 0, 0), (0, 1, 0), (0, 0, 1).
  */
mitk::Surface::Pointer SurfaceGenerator::generateCuboid(unsigned int width, unsigned int height, unsigned int depth, vtkVector<float, 3> center, vtkVector<float, 3> newXAxis, vtkVector<float, 3> newYAxis, vtkVector<float, 3> newZAxis) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // Create a simple (VTK) box.
  vtkSmartPointer<vtkCubeSource> cube = vtkSmartPointer<vtkCubeSource>::New();
  cube->SetXLength(width);
  cube->SetYLength(height);
  cube->SetZLength(depth);
  cube->SetCenter(0, 0, 0);
  cube->Update();

  // Convert default axes (0, 0, 0) to standard (1, 0, 0), (0, 1, 0), (0, 0, 1).
  if (newXAxis[0] == 0.0f && newXAxis[1] == 0.0f && newXAxis[2] == 0.0f) {
    newXAxis[0] = 1;
    newXAxis[1] = 0;
    newXAxis[2] = 0;
  }

  if (newYAxis[0] == 0.0f && newYAxis[1] == 0.0f && newYAxis[2] == 0.0f) {
    newYAxis[0] = 0;
    newYAxis[1] = 1;
    newYAxis[2] = 0;
  }

  if (newZAxis[0] == 0.0f && newZAxis[1] == 0.0f && newZAxis[2] == 0.0f) {
    newZAxis[0] = 0;
    newZAxis[1] = 0;
    newZAxis[2] = 1;
  }

  // Transform it to be centered at center and be aligned by new axes given.
  vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
  // Col 1
  matrix->SetElement(0, 0, newXAxis[0]);
  matrix->SetElement(1, 0, newXAxis[1]);
  matrix->SetElement(2, 0, newXAxis[2]);
  matrix->SetElement(3, 0, 0);
  // Col 2
  matrix->SetElement(0, 1, newYAxis[0]);
  matrix->SetElement(1, 1, newYAxis[1]);
  matrix->SetElement(2, 1, newYAxis[2]);
  matrix->SetElement(3, 1, 0);
  // Col 3
  matrix->SetElement(0, 2, newZAxis[0]);
  matrix->SetElement(1, 2, newZAxis[1]);
  matrix->SetElement(2, 2, newZAxis[2]);
  matrix->SetElement(3, 2, 0);
  // Col 4
  matrix->SetElement(0, 3, center[0]);
  matrix->SetElement(1, 3, center[1]);
  matrix->SetElement(2, 3, center[2]);
  matrix->SetElement(3, 3, 1);

  if (DEBUGGING) {
    cout << "Matrix:" << endl;
    for (unsigned int i = 0; i < 4; i++) {
      cout << "(";
      for (unsigned int j = 0; j < 4; j++) {
        cout << matrix->GetElement(i, j);
        if (j < 3) {
          cout << ", ";
        }
      }
      cout << ")" << endl;
    }
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

/**
  * Generates a circle with give radius, center and normal.
  */
mitk::Surface::Pointer SurfaceGenerator::generateCircle(unsigned int radius, vtkVector<float, 3> center, vtkVector<float, 3> normal) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // If normal isn't set then make it (0, 0, 1);
  if (normal[0] == 0.0f && normal[1] == 0.0f && normal[2] == 0.0f) {
    normal[0] = 0.0f;
    normal[1] = 0.0f;
    normal[2] = 1.0f;
  }

  vtkSmartPointer<vtkRegularPolygonSource> polygonSource = vtkSmartPointer<vtkRegularPolygonSource>::New();
  polygonSource->SetNumberOfSides(50);
  polygonSource->SetRadius(radius);
  polygonSource->SetCenter(center[0], center[1], center[2]);
  polygonSource->SetNormal(normal[0], normal[1], normal[2]);
  polygonSource->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer circleSurface = mitk::Surface::New();
  circleSurface->SetVtkPolyData(static_cast<vtkPolyData*>(polygonSource->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return circleSurface;
}

mitk::Surface::Pointer SurfaceGenerator::generateCylinder2(unsigned int radius, vtkVector<float, 3> startPoint, vtkVector<float, 3> endPoint) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(3);

  // Create a line
  vtkSmartPointer<vtkLineSource> lineSource = vtkSmartPointer<vtkLineSource>::New();
  lineSource->SetPoint1(startPoint[0], startPoint[1], startPoint[2]);
  lineSource->SetPoint2(endPoint[0], endPoint[1], endPoint[2]);
  lineSource->Update();

  mitk::ProgressBar::GetInstance()->Progress();
  
  // Create a tube (cylinder) around the line
  vtkSmartPointer<vtkTubeFilter> tubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  tubeFilter->SetInputConnection(lineSource->GetOutputPort());
  tubeFilter->SetRadius(radius); 
  tubeFilter->SetNumberOfSides(50);
  tubeFilter->Update();

  mitk::ProgressBar::GetInstance()->Progress();

  // Wrap it in some MITK.
  mitk::Surface::Pointer cylinderSurface = mitk::Surface::New();
  cylinderSurface->SetVtkPolyData(static_cast<vtkPolyData*>(tubeFilter->GetOutput()));
  mitk::ProgressBar::GetInstance()->Progress();
  return cylinderSurface;
}