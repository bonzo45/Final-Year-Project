#ifndef SVD_Scan_Plane_Generator_h
#define SVD_Scan_Plane_Generator_h

#include <vtkSmartPointer.h>
#include <vtkPlane.h>
#include <mitkImage.h>
#include <mitkPointSet.h>

class SVDScanPlaneGenerator {
  public:
    SVDScanPlaneGenerator();
    void setUncertainty(mitk::Image::Pointer uncertainty);
    void setThreshold(double threshold);
    vtkSmartPointer<vtkPlane> calculateBestScanPlane();

  private:
    mitk::Image::Pointer uncertainty;
    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;

    double threshold;

    mitk::PointSet::Pointer pointsBelowThreshold(double threshold);
    void calculateCentroid(mitk::PointSet::Pointer pointSet, mitk::Point3D & centroid);
};

#endif