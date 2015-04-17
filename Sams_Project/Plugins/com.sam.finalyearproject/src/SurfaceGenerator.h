#ifndef Surface_Generator_h
#define Surface_Generator_h

#include <mitkSurface.h>

class SurfaceGenerator {
  public:
    static mitk::Surface::Pointer generateSphere(unsigned int resolution = 100, unsigned int radius = 20);
};

#endif