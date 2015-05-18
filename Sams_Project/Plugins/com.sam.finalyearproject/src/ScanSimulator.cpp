#include "ScanSimulator.h"
#include "Util.h"

#include <mitkImageAccessByItk.h>
#include <mitkImageCast.h>

// Loading bar
#include <mitkProgressBar.h>

/**
  * Sets the volume to scan.
  */
void ScanSimulator::setVolume(mitk::Image::Pointer volume) {
  this->volume = volume;
  this->volumeHeight = volume->GetDimension(0);
  this->volumeWidth = volume->GetDimension(1);
  this->volumeDepth = volume->GetDimension(2);
}

/**
  * Sets the origin of the scan.
  *   i.e. where pixel (0, 0, 0) in the scan is.
  */
void ScanSimulator::setScanOrigin(vtkVector<float, 3> origin) {
  this->scanOrigin = origin;
}

/**
  * Instead of specifying the origin you can specify the center instead.
  * NOTE: You must call:
  *   setScanAxes(...)
  *   setScanResolution(...)
  *   setScanSize(...)
  * BEFORE calling this method as it relies on those values being set.
  */
void ScanSimulator::setScanCenter(vtkVector<float, 3> center) {
  this->scanOrigin = center;
  this->scanOrigin = Util::vectorSubtract(scanOrigin, Util::vectorScale(xDirection, (scanWidth * xResolution) / 2.0));
  this->scanOrigin = Util::vectorSubtract(scanOrigin, Util::vectorScale(yDirection, (scanHeight * yResolution) / 2.0));
  this->scanOrigin = Util::vectorSubtract(scanOrigin, Util::vectorScale(zDirection, (numSlices * zResolution) / 2.0));
}

/**
  * Sets the orientation of the scan.
  *   xDirection is the 'horizontal' direction.
  *   yDirection is the 'vertical' direction.
  *   zDirection is the 'slice' direction.
  */
void ScanSimulator::setScanAxes(vtkVector<float, 3> xDirection, vtkVector<float, 3> yDirection, vtkVector<float, 3> zDirection) {
  this->xDirection = xDirection;
  this->yDirection = yDirection;
  this->zDirection = zDirection;

  this->xDirection.Normalize();
  this->yDirection.Normalize();
  this->zDirection.Normalize();
};

/**
  * Sets the resolution of the scan.
  *   xResolution is resolution of the 'horizontal' pixels.
  *   yResolution is resolution of the 'vertical' pixels.
  *   zResolution is resolution of the slices.
  * e.g. (1, 1, 2) would sample at the same resolution as the original volume within a slice
  *   but give slices that are twice as thick as this.
  */
void ScanSimulator::setScanResolution(double xResolution, double yResolution, double zResolution) {
  this->xResolution = xResolution;
  this->yResolution = yResolution;
  this->zResolution = zResolution;
}

/**
  * Sets the size of the scan.
  *   width is the number of 'horizontal' pixels.
  *   heiht is the number of 'vertical' pixels.
  *   slices is the number of slices.
  */
void ScanSimulator::setScanSize(unsigned int width, unsigned int height, unsigned int slices) {
  this->scanWidth = width;
  this->scanHeight = height;
  this->numSlices = slices;
}

/**
  * Sets whether motion corruption is enabled.
  * This will randomly rotate each slice up to a maximum of the angle 
  * set in setMotionCorruptionMaxAngle().
  */
void ScanSimulator::setMotionCorruption(bool corruption) {
  this->motionCorruptionOn = corruption;
}

/**
  * Sets the maximum rotation angle to corrupt each slice by.
  */
void ScanSimulator::setMotionCorruptionMaxAngle(double angle) {
  this->motionCorruptionMaxAngle = angle;
}

/**
  * Converts from a pixel address in the scan to the position in the target volume.
  */
vtkVector<float, 3> ScanSimulator::scanToVolumePosition(unsigned int w, unsigned int h, unsigned int s) {
  vtkVector<float, 3> position = scanOrigin;

  // Move w in xDirection, h in yDirection, s in zDirection
  position = Util::vectorAdd(position, Util::vectorScale(xDirection, w * xResolution));
  position = Util::vectorAdd(position, Util::vectorScale(yDirection, h * yResolution));
  position = Util::vectorAdd(position, Util::vectorScale(zDirection, s * zResolution));

  if (DEBUGGING) {
    std::cout << "(" << w << ", " << h << ", " << s << ") became (" << position[0] << ", " << position[1] << ", " << position[2] << ")" << std::endl;
  }

  return position;
}

/**
  * Scans the volume.
  */
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

  mitk::ProgressBar::GetInstance()->AddStepsToDo(scanSize[2]*scanSize[0] + 2);

  region.SetSize(scanSize);
  region.SetIndex(start);

  ScanImageType::Pointer scan = ScanImageType::New();
  scan->SetRegions(region);
  scan->Allocate();
  mitk::ProgressBar::GetInstance()->Progress();

  // Compute motion corruption.
  std::list<vtkSmartPointer<vtkTransform> > * motion;
  std::list<vtkSmartPointer<vtkTransform> >::iterator motionIterator;
  if (motionCorruptionOn) {
    motion = generateRandomMotionSequence(numSlices);
    motionIterator = motion->begin();
    if (DEBUGGING) {
      std::cout << "Motion: " << *motionIterator << endl;
      for (std::list<vtkSmartPointer<vtkTransform> >::iterator it = motion->begin(); it != motion->end(); ++it) {
        std::cout << ' ' << *it;
      }
      std::cout << endl;
    }
  }

  mitk::ProgressBar::GetInstance()->Progress();

  // Go through each slice.
  for (unsigned int s = 0; s < scanSize[2]; s++) {
    if (DEBUGGING) {
      cout << "Slice (" << s << ")" << endl;
    }
    vtkSmartPointer<vtkTransform> movement;
    if (motionCorruptionOn) {
      movement = *motionIterator;
      motionIterator++;
    }
    // Go through pixels within a slice.
    for (unsigned int w = 0; w < scanSize[0]; w++) {
      for (unsigned int h = 0; h < scanSize[1]; h++) {
        // Convert the slice pixel to coordinates in the volume.
        vtkVector<float, 3> volumePosition = scanToVolumePosition(h, w, s);

        if (motionCorruptionOn) {
          movement->TransformPoint(&(volumePosition[0]), &(volumePosition[0]));
        }

        double volumeValue = -1.0;
        if (volumePosition[0] >= 0 && volumePosition[1] >= 0 && volumePosition[2] >= 0 &&
            volumePosition[0] < volumeHeight && volumePosition[1] < volumeWidth && volumePosition[2] < volumeDepth) {
          AccessByItk_2(this->volume, Util::ItkInterpolateValue, volumePosition, volumeValue);
        }

        ScanImageType::IndexType pixelIndex;
        pixelIndex[0] = w;
        pixelIndex[1] = h;
        pixelIndex[2] = s;

        scan->SetPixel(pixelIndex, volumeValue);
      }
      mitk::ProgressBar::GetInstance()->Progress();
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(scan, result);
  return result;
}

/**
  * Generates a vtkTransform representing random motion.
  */
vtkSmartPointer<vtkTransform> ScanSimulator::generateRandomMotion() {
  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->RotateWXYZ(
    ((double)rand() / RAND_MAX) * motionCorruptionMaxAngle,   // Random value between 0 and motionCorruptionMaxAngle
    rand() % 100,         // Pick a random axis to rotate about.
    rand() % 100,
    rand() % 100
  );

  return transform;
}

/**
  * Generates a series of vtkTransforms representing motion.
  * Currently each transform is completely independent of every other but this could be tweaked
  * make it more realistic.
  */
std::list<vtkSmartPointer<vtkTransform> > * ScanSimulator::generateRandomMotionSequence(unsigned int steps) {
  std::list<vtkSmartPointer<vtkTransform> > * list = new std::list<vtkSmartPointer<vtkTransform> >();
  for (unsigned int i = 0; i < steps; i++) {
    vtkSmartPointer<vtkTransform> movement = generateRandomMotion();
    list->push_back(movement);
  }

  return list;
}