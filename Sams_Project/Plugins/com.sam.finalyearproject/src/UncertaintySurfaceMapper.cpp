#include "UncertaintySurfaceMapper.h"

#include "UncertaintySampler.h"
#include "Util.h"

#include <cstdio>

#include <vtkSmartPointer.h>
#include <vtkFloatArray.h>
#include <vtkPolyData.h>
#include <vtkVector.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointData.h>

#include <itkImportImageFilter.h>
#include <itkRescaleIntensityImageFilter.h>
#include <itkAdaptiveHistogramEqualizationImageFilter.h>

#include <mitkImagePixelWriteAccessor.h>
#include <mitkPlaneGeometry.h>
#include <mitkLine.h>

// Loading bar
#include <mitkProgressBar.h>

UncertaintySurfaceMapper::UncertaintySurfaceMapper() {
  setSamplingDistance(HALF);  
  setColour(BLACK_AND_WHITE);
  setScaling(NONE);
  setSamplingAccumulator(AVERAGE);
  setRegistration(SIMPLE);
  setDebugRegistration(false);
}

void UncertaintySurfaceMapper::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);
}

void UncertaintySurfaceMapper::setSurface(mitk::Surface::Pointer surface) {
  this->surface = surface;
}

void UncertaintySurfaceMapper::setSamplingDistance(SAMPLING_DISTANCE samplingDistance) {
  this->samplingDistance = samplingDistance;
}

void UncertaintySurfaceMapper::setScaling(SCALING scaling) {
  this->scaling = scaling;
}

void UncertaintySurfaceMapper::setColour(COLOUR colour) {
  this->colour = colour;
}

void UncertaintySurfaceMapper::setSamplingAccumulator(SAMPLING_ACCUMULATOR samplingAccumulator) {
  this->samplingAccumulator = samplingAccumulator;
}

void UncertaintySurfaceMapper::setRegistration(REGISTRATION registration) {
  this->registration = registration;
}

void UncertaintySurfaceMapper::setInvertNormals(bool invertNormals) {
  this->invertNormals = invertNormals;
}

void UncertaintySurfaceMapper::setDebugRegistration(bool debugRegistration) {
  this->debugRegistration = debugRegistration;
}

void UncertaintySurfaceMapper::map() {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(7);

  // Extract the vtkPolyData.
  vtkPolyData * surfacePolyData = this->surface->GetVtkPolyData();

  // From this we can get the array containing all the normals.
  vtkSmartPointer<vtkFloatArray> normals = vtkFloatArray::SafeDownCast(surfacePolyData->GetPointData()->GetNormals());
  if (!normals) {
    cerr << "Couldn't seem to find any normals." << endl;
    return;
  }

  mitk::ProgressBar::GetInstance()->Progress();

  // Compute the bounding box of the surface (for simple registration between surface and uncertainty volume)
  double bounds[6];
  surfacePolyData->GetBounds(bounds); // NOTE: Apparently this isn't thread safe.
  double xMin = bounds[0];
  double xMax = bounds[1];
  double xRange = xMax - xMin;
  double yMin = bounds[2];
  double yMax = bounds[3];
  double yRange = yMax - yMin;
  double zMin = bounds[4];
  double zMax = bounds[5];
  double zRange = zMax - zMin;
  if (DEBUGGING) {
    cout << "Surface Bounds:" << endl;
    cout << xMin << "<= x <=" << xMax << " (" << xRange << ")" << endl;
    cout << yMin << "<= y <=" << yMax << " (" << yRange << ")" << endl;
    cout << zMin << "<= z <=" << zMax << " (" << zRange << ")" << endl;
    cout << "Uncertainty Size:" << endl;
    cout << "(" << uncertaintyHeight << ", " << uncertaintyWidth << ", " << uncertaintyDepth << ")" << endl;
  }

  mitk::ProgressBar::GetInstance()->Progress();

  // ----------------------------------------- //
  // ---- Compute Uncertainty Intensities ---- //
  // ----------------------------------------- //
  // Generate a list of intensities, one for each point.
  unsigned int numberOfPoints = surfacePolyData->GetNumberOfPoints();
  double * intensityArray;
  intensityArray  = new double[numberOfPoints];

  // Create an uncertainty sampler.
  UncertaintySampler * sampler = new UncertaintySampler();
  sampler->setUncertainty(this->uncertainty);
  
  switch (samplingAccumulator) {
    case AVERAGE: sampler->setAverage(); break;
    case MINIMUM: sampler->setMin(); break;
    case MAXIMUM: sampler->setMax(); break;
  }

  mitk::ProgressBar::GetInstance()->Progress();

  mitk::ProgressBar::GetInstance()->AddStepsToDo(numberOfPoints);

  for (unsigned int i = 0; i < numberOfPoints; i++) {
    // Get the position of point i
    double positionOfPoint[3];
    surfacePolyData->GetPoint(i, positionOfPoint);

    // TODO: Better registration step. Unfortunately MITK appears not to be able to do pointwise registration between surface and image.
    vtkVector<float, 3> position = vtkVector<float, 3>();
    switch (registration) {
      // No registration.
      case IDENTITY:
      {
        position[0] = positionOfPoint[0];
        position[1] = positionOfPoint[1];
        position[2] = positionOfPoint[2];
      }
      break;
    
      // Simple scaling. Normalise the point to 0-1 based on the bounding box of the surface. Scale by uncertainty volume size.
      case SIMPLE:
      {
        position[0] = ((positionOfPoint[0] - xMin) / xRange) * (uncertaintyHeight - 1);
        position[1] = ((positionOfPoint[1] - yMin) / yRange) * (uncertaintyWidth - 1);
        position[2] = ((positionOfPoint[2] - zMin) / zRange) * (uncertaintyDepth - 1);
      }
      break;

      // Bodge scaling. Position of point holds world coordinate? Negate it. Then use inverse world transform on uncertainty.
      case BODGE:
      {
        mitk::Point3D surfaceWorld;
        surfaceWorld[0] = -positionOfPoint[0];
        surfaceWorld[1] = -positionOfPoint[1];
        surfaceWorld[2] = positionOfPoint[2];

        mitk::Point3D uncertaintyIndex;
        this->uncertainty->GetGeometry()->WorldToIndex(surfaceWorld, uncertaintyIndex);

        position[0] = uncertaintyIndex[0];
        position[1] = uncertaintyIndex[1];
        position[2] = uncertaintyIndex[2];
      }
      break;

      // Sphere scaling. Assumes that the point is on a sphere with center (0, 0, 0) and finds the
      // voxel in the uncertainty furthest along the vector from the center to the point.
      case SPHERE:
      {
        // Create a line from the center, going in direction positionOfPoint
        mitk::Point3D volumeCenter;
        volumeCenter[0] = (uncertaintyHeight - 1) / 2;
        volumeCenter[1] = (uncertaintyWidth - 1) / 2;
        volumeCenter[2] = (uncertaintyDepth - 1) / 2;

        mitk::Vector3D direction;
        direction[0] = positionOfPoint[0];
        direction[1] = positionOfPoint[1];
        direction[2] = positionOfPoint[2];

        mitk::Line3D line;
        line.SetPoint(volumeCenter);
        line.SetDirection(direction);

        // Create planes to represent each face of the cuboid representing the uncertainty.
        mitk::PlaneGeometry::Pointer plane[3];
        // The 'right' side of the volume.
        mitk::Point3D xOrigin;
        xOrigin[0] = uncertaintyHeight - 1;
        xOrigin[1] = 0;
        xOrigin[2] = 0;
        mitk::Vector3D xNormal;
        xNormal[0] = -1;
        xNormal[1] = 0;
        xNormal[2] = 0;

        // The 'top' side of the volume.
        mitk::Point3D yOrigin;
        yOrigin[0] = 0;
        yOrigin[1] = uncertaintyWidth - 1;
        yOrigin[2] = 0;
        mitk::Vector3D yNormal;
        yNormal[0] = 0;
        yNormal[1] = -1;
        yNormal[2] = 0;

        // The 'back' side of the volume.
        mitk::Point3D zOrigin;
        zOrigin[0] = 0;
        zOrigin[1] = 0;
        zOrigin[2] = uncertaintyDepth - 1;
        mitk::Vector3D zNormal;
        zNormal[0] = 0;
        zNormal[1] = 0;
        zNormal[2] = -1;

        plane[0] = mitk::PlaneGeometry::New();
        plane[0]->InitializePlane(xOrigin, xNormal);
        plane[1] = mitk::PlaneGeometry::New();
        plane[1]->InitializePlane(yOrigin, yNormal);
        plane[2] = mitk::PlaneGeometry::New();
        plane[2]->InitializePlane(zOrigin, zNormal);

        // For each plane, see if the line intersects it within the boundaries of the uncertainty.
        position[0] = -1;
        position[1] = -1;
        position[2] = -1;
        for (int i = 0; i < 3; i++) {
          mitk::Point3D intersectionPoint;
          plane[i]->IntersectionPoint(line, intersectionPoint);
          if (
              (intersectionPoint[0] >= -0.5) && (intersectionPoint[0] <= (uncertaintyHeight - 0.5)) &&
              (intersectionPoint[1] >= -0.5) && (intersectionPoint[1] <= (uncertaintyWidth - 0.5)) &&
              (intersectionPoint[2] >= -0.5) && (intersectionPoint[2] <= (uncertaintyDepth - 0.5))
          ) {
            // If we're within the bounds then it's either the plane we intersected with.
            if (
               ((intersectionPoint[0] > 0) && (direction[0] > 0)) ||
               ((intersectionPoint[1] > 0) && (direction[1] > 0)) ||
               ((intersectionPoint[2] > 0) && (direction[2] > 0))
            ) {
              position[0] = intersectionPoint[0];
              position[1] = intersectionPoint[1];
              position[2] = intersectionPoint[2];
            }
            // Or it's opposite.
            else {
              position[0] = (uncertaintyHeight - 0.5) - intersectionPoint[0];
              position[1] = (uncertaintyWidth - 0.5) -intersectionPoint[1];
              position[2] = (uncertaintyDepth - 0.5) -intersectionPoint[2];
            }
            break;
          }
        }
        if (position[0] == -1 && position[1] == -1 && position[2] == -1) {
          std::cout << "Yep... no planes..." << std::endl;
        }
      }
      break;
    }
    
    // Mark on the uncertainty the point we're registered to.
    if (debugRegistration) {
        try  {
          // See if the uncertainty data is available to be written to.
          mitk::ImagePixelWriteAccessor<double, 3> writeAccess(this->uncertainty);
          itk::Index<3> index;
          index[0] = round(position[0]);
          index[1] = round(position[1]);
          index[2] = round(position[2]);
          writeAccess.SetPixelByIndexSafe(index, 1.0);
        }
        catch (mitk::Exception & e) {
          std::cerr << "Hmmm... it appears we can't get read access to the uncertainty image. Maybe it's gone? Maybe it's type isn't double? (I've assumed it is)" << e << std::endl;
          std::cerr << "Continuing without marking registered point in uncertainty." << std::endl;
        }
    }

    // Get the normal of point i
    double normalAtPoint[3];
    normals->GetTuple(i, normalAtPoint);
    vtkVector<float, 3> normal = vtkVector<float, 3>();
    normal[0] = normalAtPoint[0];
    normal[1] = normalAtPoint[1];
    normal[2] = normalAtPoint[2];

    if (invertNormals) {
      normal[0] = -normal[0];
      normal[1] = -normal[1];
      normal[2] = -normal[2];
    }

    // Use the position and normal to sample the uncertainty data.
    switch(samplingDistance) {
     case FULL: intensityArray[i] = sampler->sampleUncertainty(position, normal); break;
     case HALF: intensityArray[i] = sampler->sampleUncertainty(position, normal, 50); break;
    }

    mitk::ProgressBar::GetInstance()->Progress();
  }
  
  mitk::ProgressBar::GetInstance()->Progress();

  // --------------------------------- //
  // ---- Map Uncertanties to ITK ---- //
  // --------------------------------- //
  // First convert to the uncertainty array to an ITK image so we can filter it. (list of doubles)
  typedef itk::Image<double, 1> UncertaintyListType;
  typedef itk::ImportImageFilter<double, 1>   ImportFilterType;
  ImportFilterType::Pointer importFilter = ImportFilterType::New(); 
  
  ImportFilterType::SizeType  size; 
  size[0] = numberOfPoints;
  
  ImportFilterType::IndexType start;
  start[0] = 0;
  
  ImportFilterType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);
  importFilter->SetRegion(region);
 
  double origin[1];
  origin[0] = 0.0;
  importFilter->SetOrigin(origin);
 
  double spacing[1];
  spacing[0] = 1.0;
  importFilter->SetSpacing(spacing);

  const bool importImageFilterWillOwnTheBuffer = true;
  importFilter->SetImportPointer(intensityArray, numberOfPoints, importImageFilterWillOwnTheBuffer);
  importFilter->Update();

  mitk::ProgressBar::GetInstance()->Progress();

  // -------------------------------- //
  // ---- Scale the Uncertanties ---- //
  // -------------------------------- //
  UncertaintyListType::Pointer scaled;
  switch (scaling) {
    // No scaling.
    case NONE:
    {
      scaled = importFilter->GetOutput();
      legendMinValue = 0.0;
      legendMaxValue = 1.0;
    }
    break;
    
    // Linear scaling. Map {min-max} to {0-1.}.
    case LINEAR: 
    {
      typedef itk::RescaleIntensityImageFilter<UncertaintyListType, UncertaintyListType> RescaleFilterType;
      typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
      rescaleFilter->SetInput(importFilter->GetOutput());
      rescaleFilter->SetOutputMinimum(0.0);
      rescaleFilter->SetOutputMaximum(1.0);
      rescaleFilter->Update();
      scaled = rescaleFilter->GetOutput();
      legendMinValue = rescaleFilter->GetInputMinimum();
      legendMaxValue = rescaleFilter->GetInputMaximum();
    }
    break;

    // Histogram equalization.
    case HISTOGRAM:
    {
      typedef itk::AdaptiveHistogramEqualizationImageFilter<UncertaintyListType> AdaptiveHistogramEqualizationImageFilterType;
      AdaptiveHistogramEqualizationImageFilterType::Pointer histogramEqualizationFilter = AdaptiveHistogramEqualizationImageFilterType::New();
      histogramEqualizationFilter->SetInput(importFilter->GetOutput());
      histogramEqualizationFilter->SetAlpha(0);
      histogramEqualizationFilter->SetBeta(0);
      histogramEqualizationFilter->SetRadius(1);
      histogramEqualizationFilter->Update();
      scaled = histogramEqualizationFilter->GetOutput();
      legendMinValue = -1.0;
      legendMaxValue = -1.0;
    }
    break;
  }

  mitk::ProgressBar::GetInstance()->Progress();

  // ----------------------------- //
  // ---- Map them to Colours ---- //
  // ----------------------------- //
  // Generate a list of colours, one for each point.
  vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
  colors->SetNumberOfComponents(3);
  colors->SetName ("Colors");

  double * scaledValues = scaled->GetBufferPointer();

  // Set the colour to be a grayscale version of this sampling.
  for (unsigned int i = 0; i < numberOfPoints; i++) {
    unsigned char intensity = static_cast<unsigned char>(round(scaledValues[i] * 255));
    //cout << scaledValues[i] << " -> " << scaledValues[i] * 255  << " -> " << round(scaledValues[i] * 255) << " -> " << intensity << " -> " << (int) intensity << endl;
    unsigned char normalColour[3];
    switch (colour) {
      // Black and White
      case BLACK_AND_WHITE:
        normalColour[0] = intensity;
        normalColour[1] = intensity;
        normalColour[2] = intensity;
        break;

      // Colour
      case BLACK_AND_RED:
        normalColour[0] = Util::IntensityToRed(intensity);
        normalColour[1] = Util::IntensityToGreen(intensity);
        normalColour[2] = Util::IntensityToBlue(intensity);
        break;
    }
    colors->InsertNextTupleValue(normalColour);
  }
  
  // Set the colours to be the scalar value of each point.
  surfacePolyData->GetPointData()->SetScalars(colors);

  mitk::ProgressBar::GetInstance()->Progress();
}

double UncertaintySurfaceMapper::getLegendMinValue() {
  return legendMinValue;
}

double UncertaintySurfaceMapper::getLegendMaxValue() {
  return legendMaxValue;
}

void UncertaintySurfaceMapper::getLegendMinColour(char * colour) {
  if (this->colour) {
    colour[0] = 0;
    colour[1] = 0;
    colour[2] = 0;
  }
  else {
    colour[0] = 0;
    colour[1] = 0;
    colour[2] = 0;
  }
}

void UncertaintySurfaceMapper::getLegendMaxColour(char * colour) {
  if (this->colour) {
    colour[0] = 255;
    colour[1] = 0;
    colour[2] = 0;
  }
  else {
    colour[0] = 255;
    colour[1] = 255;
    colour[2] = 255;
  }
}