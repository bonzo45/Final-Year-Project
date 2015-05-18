#include "UncertaintyGenerator.h"
#include "Util.h"
#include <mitkImageCast.h>

// Loading bar
#include <mitkProgressBar.h>

/**
  * Generates uncertainty data (height * width * depth).
  * Each voxel is a random uncertainty value between 0 and 255.
  */
mitk::Image::Pointer UncertaintyGenerator::generateRandomUncertainty(vtkVector<float, 3> imageSize) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);
  
  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = imageSize[0];
  uncertaintySize[1] = imageSize[1];
  uncertaintySize[2] = imageSize[2];
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  UncertaintyImageType::Pointer randomUncertainty = UncertaintyImageType::New();
  randomUncertainty->SetRegions(region);
  randomUncertainty->Allocate();

  // Go through each voxel and set a random value.
  for (unsigned int r = 0; r < imageSize[0]; r++) {
    for (unsigned int c = 0; c < imageSize[1]; c++) {
      for (unsigned int d = 0; d < imageSize[2]; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        randomUncertainty->SetPixel(pixelIndex, rand() % 256);
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(randomUncertainty, result);
  mitk::ProgressBar::GetInstance()->Progress();
  return result;
}

/**
  * Generates uncertainty data (height * width * depth).
  * The cube, placed at the center with side length cubeSize, is totally uncertain (1) and everywhere else is completely certain (255).
  */
mitk::Image::Pointer UncertaintyGenerator::generateCubeUncertainty(vtkVector<float, 3> imageSize, unsigned int cubeSize) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);
  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = imageSize[0];
  uncertaintySize[1] = imageSize[1];
  uncertaintySize[2] = imageSize[2];
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  UncertaintyImageType::Pointer cubeUncertainty = UncertaintyImageType::New();
  cubeUncertainty->SetRegions(region);
  cubeUncertainty->Allocate();

  // Compute the cube center point.
  float cubeCenter0 = (imageSize[0] - 1) / 2.0f;
  float cubeCenter1 = (imageSize[1] - 1) / 2.0f;
  float cubeCenter2 = (imageSize[2] - 1) / 2.0f;

  // Work out which columns/rows/depths the cube is in.
  float halfCube = cubeSize / 2.0f;
  unsigned int cubeRowStart = cubeCenter0 - halfCube;
  unsigned int cubeRowEnd = cubeCenter0 + halfCube;
  unsigned int cubeColStart = cubeCenter1 - halfCube;
  unsigned int cubeColEnd = cubeCenter1 + halfCube;
  unsigned int cubeDepthStart = cubeCenter2 - halfCube;
  unsigned int cubeDepthEnd = cubeCenter2 + halfCube;

  // Go through each voxel and set uncertainty according to whether it's in the cube.
  for (unsigned int r = 0; r < imageSize[0]; r++) {
    for (unsigned int c = 0; c < imageSize[1]; c++) {
      for (unsigned int d = 0; d < imageSize[2]; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        // If this pixel is in the cube.
        if ((cubeRowStart <= r) && (r <= cubeRowEnd) &&
            (cubeColStart <= c) && (c <= cubeColEnd) &&
            (cubeDepthStart <= d) && (d <= cubeDepthEnd)) {
          cubeUncertainty->SetPixel(pixelIndex, 1);
        }
        // If it's not.
        else {
          cubeUncertainty->SetPixel(pixelIndex, 255);
        }
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(cubeUncertainty, result);
  mitk::ProgressBar::GetInstance()->Progress();
  return result;
}

/**
  * Generates uncertainty data (imageSize[0] * imageSize[1] * imageSize[2]).
  * It's zero everywhere, apart from a sphere of radius sphereRadius that has uncertainty 255 at the center and fades linearly to the edges.
  */
mitk::Image::Pointer UncertaintyGenerator::generateSphereUncertainty(vtkVector<float, 3> imageSize, unsigned int sphereRadius, vtkVector<float, 3> sphereCenter) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = imageSize[0];
  uncertaintySize[1] = imageSize[1];
  uncertaintySize[2] = imageSize[2];
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  UncertaintyImageType::Pointer sphereUncertainty = UncertaintyImageType::New();
  sphereUncertainty->SetRegions(region);
  sphereUncertainty->Allocate();

  // If the center is not specified (-1) in any dimension, make it the center.
  for (unsigned int i = 0; i < 3; i++) {
    if (sphereCenter[i] == -1.0f) {
      sphereCenter[i] = (float) (imageSize[i] - 1) / 2.0;
    }
  }

  // Go through each voxel and weight uncertainty by distance from center of sphere.
  for (unsigned int r = 0; r < imageSize[0]; r++) {
    for (unsigned int c = 0; c < imageSize[1]; c++) {
      for (unsigned int d = 0; d < imageSize[2]; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        vtkVector<float, 3> position = vtkVector<float, 3>();
        position[0] = r;
        position[1] = c;
        position[2] = d;

        // Compute distance from center.
        vtkVector<float, 3> difference = Util::vectorSubtract(position, sphereCenter);
        float distanceFromCenter = difference.Norm();

        // Get normalized 0-1 weighting.
        float uncertaintyValue = 1 - std::max(0.0f, ((float) sphereRadius - distanceFromCenter) / (float) sphereRadius);

        // Scale by 255. Don't allow 0 (undefined uncertainty).
        sphereUncertainty->SetPixel(pixelIndex, uncertaintyValue * 255);
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(sphereUncertainty, result);
  mitk::ProgressBar::GetInstance()->Progress();
  return result;
}