#ifndef Scan_Simulator_h
#define Scan_Simulator_h

#include <mitkImage.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkVector.h>
#include <list>

typedef itk::Image<double, 3>  ScanImageType;

class ScanSimulator {
  public:
    void setVolume(mitk::Image::Pointer volume);
    void setScanOrigin(vtkVector<float, 3> origin);
    void setScanCenter(vtkVector<float, 3> center);
    void setScanAxes(vtkVector<float, 3> xDirection, vtkVector<float, 3> yDirection, vtkVector<float, 3> zDirection);
    void setScanResolution(double xResolution, double yResolution, double zResolution);
    void setScanSize(unsigned int width, unsigned int height, unsigned int slices);
    void setMotionCorruption(bool corruption);
    void setMotionCorruptionMaxAngle(double angle);
    mitk::Image::Pointer scan();

  private:
    mitk::Image::Pointer volume;
    unsigned int volumeWidth, volumeHeight, volumeDepth;
    vtkVector<float, 3> scanOrigin;
    vtkVector<float, 3> xDirection, yDirection, zDirection;
    double xResolution, yResolution, zResolution;
    unsigned int scanWidth, scanHeight, numSlices;
    bool motionCorruptionOn;
    double motionCorruptionMaxAngle;

    vtkVector<float, 3> scanToVolumePosition(unsigned int w, unsigned int h, unsigned int s);
    vtkSmartPointer<vtkTransform> generateRandomMotion();
    std::list<vtkSmartPointer<vtkTransform> > * generateRandomMotionSequence(unsigned int steps);

    template <typename TPixel, unsigned int VImageDimension>
    void ItkInterpolateValue(itk::Image<TPixel, VImageDimension>* itkImage, vtkVector<float, 3> position, double & value);
};

#endif