#ifndef Surface_Generator_h
#define Surface_Generator_h

#include <vtkVector.h>
#include <mitkSurface.h>

class SurfaceGenerator {
  public:
    static mitk::Surface::Pointer generateSphere(unsigned int resolution = 100, unsigned int radius = 20);
    static mitk::Surface::Pointer generateCube(unsigned int length = 20);
    static mitk::Surface::Pointer generateCylinder(unsigned int radius = 20, unsigned int height = 20, unsigned int resolution = 10);
    static mitk::Surface::Pointer generatePlane(vtkVector<float, 3> point, vtkVector<float, 3> normal, unsigned int scanWidth = 100, unsigned int scanHeight = 50);
};

#endif