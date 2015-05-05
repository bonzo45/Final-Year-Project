#include "ScanSimulator.h"
#include "Util.h"

#include <mitkImageAccessByItk.h>
#include <mitkImageCast.h>
#include <itkLinearInterpolateImageFunction.h>

void ScanSimulator::setVolume(mitk::Image::Pointer volume) {
  this->volume = volume;
}

void ScanSimulator::setScanOrigin(vtkVector<float, 3> origin) {
  this->scanOrigin = origin;
}

void ScanSimulator::setScanCenter(vtkVector<float, 3> center) {
  this->scanOrigin = center;
  this->scanOrigin = Util::vectorSubtract(scanOrigin, Util::vectorScale(xDirection, (scanWidth * xResolution) / 2.0));
  this->scanOrigin = Util::vectorSubtract(scanOrigin, Util::vectorScale(yDirection, (scanHeight * yResolution) / 2.0));
  this->scanOrigin = Util::vectorSubtract(scanOrigin, Util::vectorScale(zDirection, (numSlices * zResolution) / 2.0));
}

void ScanSimulator::setScanAxes(vtkVector<float, 3> xDirection, vtkVector<float, 3> yDirection, vtkVector<float, 3> zDirection) {
  this->xDirection = xDirection;
  this->yDirection = yDirection;
  this->zDirection = zDirection;

  this->xDirection.Normalize();
  this->yDirection.Normalize();
  this->zDirection.Normalize();
};

void ScanSimulator::setScanResolution(double xResolution, double yResolution, double zResolution) {
  this->xResolution = xResolution;
  this->yResolution = yResolution;
  this->zResolution = zResolution;
}

void ScanSimulator::setScanSize(unsigned int width, unsigned int height, unsigned int slices) {
  this->scanWidth = width;
  this->scanHeight = height;
  this->numSlices = slices;
}

void ScanSimulator::setMotionCorruption(bool corruption) {
  this->motionCorruptionOn = corruption;
}

vtkVector<float, 3> ScanSimulator::scanToVolumePosition(unsigned int w, unsigned int h, unsigned int s) {
  vtkVector<float, 3> position = scanOrigin;

  // Move w in xDirection, h in yDirection, s in zDirection
  position = Util::vectorAdd(position, Util::vectorScale(xDirection, w * xResolution));
  position = Util::vectorAdd(position, Util::vectorScale(yDirection, h * yResolution));
  position = Util::vectorAdd(position, Util::vectorScale(zDirection, s * zResolution));

  return position;
}

mitk::Image::Pointer ScanSimulator::scan() {
  // Create a blank ITK image.
  ScanImageType::RegionType region;
  ScanImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0; 

  ScanImageType::SizeType scanSize;
  scanSize[0] = scanWidth;
  scanSize[1] = scanHeight;
  scanSize[2] = numSlices;

  region.SetSize(scanSize);
  region.SetIndex(start);

  ScanImageType::Pointer scan = ScanImageType::New();
  scan->SetRegions(region);
  scan->Allocate();

  // Go through each slice.
  for (unsigned int s = 0; s < scanSize[2]; s++) {
    cout << "Slice (" << s << ")" << endl;
    // Go through pixels within a slice.
    for (unsigned int w = 0; w < scanSize[0]; w++) {
      for (unsigned int h = 0; h < scanSize[1]; h++) {
        // cout << " - (" << w << ", " << h << ")" << endl;
        // Convert the slice pixel to coordinates in the volume.
        vtkVector<float, 3> volumePosition = scanToVolumePosition(w, h, s);
        double volumeValue;
        AccessByItk_2(this->volume, ItkInterpolateValue, volumePosition, volumeValue);

        ScanImageType::IndexType pixelIndex;
        pixelIndex[0] = w;
        pixelIndex[1] = h;
        pixelIndex[2] = s;

        scan->SetPixel(pixelIndex, volumeValue);
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(scan, result);
  return result;
}

vtkSmartPointer<vtkTransform> generateRandomMotion() {
  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->RotateWXYZ(
    (rand() % 90) * (rand() % 90) / 90,   // Angle to rotate by. Between 0 and 90. Biased towards lower values?
    rand() % 100,                         // Pick a random axis to rotate about.
    rand() % 100,
    rand() % 100
  );

  return transform;
}

std::list<vtkSmartPointer<vtkTransform> > * generateRandomMotionSequence(unsigned int steps) {
  std::list<vtkSmartPointer<vtkTransform> > * list = new std::list<vtkSmartPointer<vtkTransform> >();
  for (unsigned int i = 0; i < steps; i++) {
    list->push_back(generateRandomMotion());
  }

  return list;
}

template <typename TPixel, unsigned int VImageDimension>
void ScanSimulator::ItkInterpolateValue(itk::Image<TPixel, VImageDimension>* itkImage, vtkVector<float, 3> position, double & value) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  
  itk::ContinuousIndex<double, VImageDimension> pixel;
  for (unsigned int i = 0; i < 3; i++) {
    pixel[i] = position[i];
  }
 
  typename itk::LinearInterpolateImageFunction<ImageType, double>::Pointer interpolator = itk::LinearInterpolateImageFunction<ImageType, double>::New();
  interpolator->SetInputImage(itkImage);
 
  value = interpolator->EvaluateAtContinuousIndex(pixel);
}