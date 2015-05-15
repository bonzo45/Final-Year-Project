#include "ScanSimulator.h"
#include "Util.h"

#include <mitkImageAccessByItk.h>
#include <mitkImageCast.h>

// Loading bar
#include <mitkProgressBar.h>

void ScanSimulator::setVolume(mitk::Image::Pointer volume) {
  this->volume = volume;
  this->volumeHeight = volume->GetDimension(0);
  this->volumeWidth = volume->GetDimension(1);
  this->volumeDepth = volume->GetDimension(2);
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

void ScanSimulator::setMotionCorruptionMaxAngle(double angle) {
  this->motionCorruptionMaxAngle = angle;
}

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

std::list<vtkSmartPointer<vtkTransform> > * ScanSimulator::generateRandomMotionSequence(unsigned int steps) {
  std::list<vtkSmartPointer<vtkTransform> > * list = new std::list<vtkSmartPointer<vtkTransform> >();
  for (unsigned int i = 0; i < steps; i++) {
    vtkSmartPointer<vtkTransform> movement = generateRandomMotion();
    list->push_back(movement);
  }

  return list;
}