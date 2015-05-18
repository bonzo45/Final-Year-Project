#ifndef Uncertainty_Generator_h
#define Uncertainty_Generator_h

#include <itkImage.h>
#include <mitkImage.h>
#include <vtkVector.h>

typedef itk::Image<unsigned char, 3>  UncertaintyImageType;

class UncertaintyGenerator {
  public:
    static mitk::Image::Pointer generateRandomUncertainty(vtkVector<float, 3> imageSize);
    static mitk::Image::Pointer generateCubeUncertainty(vtkVector<float, 3> imageSize, unsigned int cubeSize);
    static mitk::Image::Pointer generateSphereUncertainty(vtkVector<float, 3> imageSize, unsigned int sphereRadius, vtkVector<float, 3> sphereCenter = vtkVector<float, 3>(-1.0f));
};

#endif