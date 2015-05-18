#ifndef Scan_Plane_Calculator_h
#define Scan_Plane_Calculator_h

#include <vtkSmartPointer.h>
#include <vtkPlane.h>
#include <vtkVector.h>
#include <mitkImage.h>

/**
  * Uses RANSAC to pick the next best plane.
  * NOTE: This class is NOT used in the plugin. SVDScanPlaneGenerator is used instead.
  */
class RANSACScanPlaneGenerator {
  public:
    RANSACScanPlaneGenerator();
    void setUncertainty(mitk::Image::Pointer uncertainty);
    void setGoodnessThreshold(double goodness);
    void setMaximumIterations(unsigned int iterations);
    void setPlaneThickness(double thickness);
    vtkSmartPointer<vtkPlane> calculateBestScanPlane();

  private:
    mitk::Image::Pointer uncertainty;
    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;
    itk::Image<double, 3>::Pointer uncertaintyItk;
    vtkSmartPointer<vtkPlane> generatePotentialPlane();
    double evaluateScanPlaneGoodness(vtkSmartPointer<vtkPlane> plane);

    static const bool DEBUGGING = true;

    double totalUncertainty;

    double goodnessThreshold;
    unsigned int maxIterations;
    double planeThickness;
};

#endif