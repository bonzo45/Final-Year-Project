#ifndef Scan_Plane_Calculator_h
#define Scan_Plane_Calculator_h

#include <vtkSmartPointer.h>
#include <vtkPlane.h>
#include <vtkVector.h>
#include <mitkImage.h>

class ScanPlaneCalculator {
  public:
    ScanPlaneCalculator();
    void setUncertainty(mitk::Image::Pointer uncertainty);
    void setGoodnessThreshold(double goodness);
    void setMaximumIterations(unsigned int iterations);
    void setPlaneThickness(double thickness);
    vtkSmartPointer<vtkPlane> calculateBestScanPlane();

  private:
    mitk::Image::Pointer uncertainty;
    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;
    itk::Image<double, 3>::Pointer uncertaintyItk;
    double distanceFromPointToPlane(unsigned int x, unsigned int y, unsigned int z, vtkSmartPointer<vtkPlane> plane);
    vtkSmartPointer<vtkPlane> planeFromPoints(vtkVector<float, 3> point1, vtkVector<float, 3> point2, vtkVector<float, 3> point3);
    vtkSmartPointer<vtkPlane> generatePotentialPlane();
    double evaluateScanPlaneGoodness(vtkSmartPointer<vtkPlane> plane);

    const bool DEBUGGING = true;

    double totalUncertainty;

    double goodnessThreshold;
    unsigned int maxIterations;
    double planeThickness;
};

#endif