#ifndef Surface_Generator_h
#define Surface_Generator_h

#include <vtkVector.h>
#include <mitkSurface.h>

class SurfaceGenerator {
  public:
    static mitk::Surface::Pointer generateSphere(unsigned int resolution = 100, unsigned int radius = 20);
    static mitk::Surface::Pointer generateCube(unsigned int length = 20);
    static mitk::Surface::Pointer generateCylinder(unsigned int radius = 20, unsigned int height = 20, unsigned int resolution = 10);
    static mitk::Surface::Pointer generatePlane(
      unsigned int scanWidth, 
      unsigned int scanHeight,
      vtkVector<float, 3> center,
      vtkVector<float, 3> normal
    );
    static mitk::Surface::Pointer generateCuboid(
      unsigned int height,
      unsigned int width,
      unsigned int depth,
      vtkVector<float, 3> center = vtkVector<float, 3>(0.0f),
      vtkVector<float, 3> newXAxis = vtkVector<float, 3>(0.0f),
      vtkVector<float, 3> newYAxis = vtkVector<float, 3>(0.0f),
      vtkVector<float, 3> newZAxis = vtkVector<float, 3>(0.0f)
    );

  private:
    static const bool DEBUGGING = false;
};

#endif