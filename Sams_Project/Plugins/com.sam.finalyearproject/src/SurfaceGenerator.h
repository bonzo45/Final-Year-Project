#ifndef Surface_Generator_h
#define Surface_Generator_h

#include <mitkSurface.h>

class SurfaceGenerator {
  public:
    static mitk::Surface::Pointer generateSphere(unsigned int resolution = 100, unsigned int radius = 20);
    static mitk::Surface::Pointer generateCube(unsigned int length = 20);
    static mitk::Surface::Pointer generateCylinder(unsigned int radius = 20, unsigned int height = 20, unsigned int resolution = 10);
};

#endif