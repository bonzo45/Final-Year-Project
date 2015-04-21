#include "UncertaintySurfaceMapper.h"

#include "UncertaintySampler.h"

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

// Loading bar
#include <mitkProgressBar.h>

UncertaintySurfaceMapper::UncertaintySurfaceMapper() {
  setSamplingFull();
  setBlackAndWhite();
  setScalingNone();
  setSamplingAverage();
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

void UncertaintySurfaceMapper::setSamplingFull() {
  this->samplingFull = true;
  this->samplingHalf = false;
}

void UncertaintySurfaceMapper::setSamplingHalf() {
  this->samplingFull = false;
  this->samplingHalf = true;
}

void UncertaintySurfaceMapper::setScalingNone() {
  clearScaling();
  this->scalingNone = true;
}

void UncertaintySurfaceMapper::setScalingLinear() {
  clearScaling();
  this->scalingLinear = true;
}

void UncertaintySurfaceMapper::setScalingHistogram() {
  clearScaling();
  this->scalingHistogram = true;
}

void UncertaintySurfaceMapper::clearScaling() {
  this->scalingNone = false;
  this->scalingLinear = false;
  this->scalingHistogram = false;
}

void UncertaintySurfaceMapper::setBlackAndWhite() {
  this->blackAndWhite = true;
  this->colour = false;
}

void UncertaintySurfaceMapper::setColour() {
  this->blackAndWhite = false;
  this->colour = true;
}

void UncertaintySurfaceMapper::setSamplingAverage() {
  clearSampling();
  this->samplingAverage = true;
}

void UncertaintySurfaceMapper::setSamplingMinimum() {
  clearSampling();
  this->samplingMinimum = true;
}

void UncertaintySurfaceMapper::setSamplingMaximum() {
  clearSampling();
  this->samplingMaximum = true;
}

void UncertaintySurfaceMapper::clearSampling() {
  this->samplingAverage = false;
  this->samplingMinimum = false;
  this->samplingMaximum = false;
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
  if (samplingAverage) {
    sampler->setAverage();
  }
  else if (samplingMinimum) {
    sampler->setMin();
  }
  else if (samplingMaximum) {
    sampler->setMax();
  }   

  mitk::ProgressBar::GetInstance()->Progress();

  for (unsigned int i = 0; i < numberOfPoints; i++) {
    // Get the position of point i
    double positionOfPoint[3];
    surfacePolyData->GetPoint(i, positionOfPoint);

    // TODO: Better registration step. Unfortunately MITK appears not to be able to do pointwise registration between surface and image.
    // Simple scaling. Normalise the point to 0-1 based on the bounding box of the surface. Scale by uncertainty volume size.
    vtkVector<float, 3> position = vtkVector<float, 3>();
    position[0] = ((positionOfPoint[0] - xMin) / xRange) * (uncertaintyHeight - 1);
    position[1] = ((positionOfPoint[1] - yMin) / yRange) * (uncertaintyWidth - 1);
    position[2] = ((positionOfPoint[2] - zMin) / zRange) * (uncertaintyDepth - 1);

    // Get the normal of point i
    double normalAtPoint[3];
    normals->GetTuple(i, normalAtPoint);
    vtkVector<float, 3> normal = vtkVector<float, 3>();
    normal[0] = -normalAtPoint[0];
    normal[1] = -normalAtPoint[1];
    normal[2] = -normalAtPoint[2];

    // Use the position and normal to sample the uncertainty data.
    if (samplingFull) {
      intensityArray[i] = sampler->sampleUncertainty(position, normal);
    }
    else if (samplingHalf) {
      intensityArray[i] = sampler->sampleUncertainty(position, normal, 50);
    }
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
  // No scaling.
  if (scalingNone) {
    scaled = importFilter->GetOutput();
  }

  // Linear scaling. Map {min-max} to {0-1.}.
  else if (scalingLinear) {
    typedef itk::RescaleIntensityImageFilter<UncertaintyListType, UncertaintyListType> RescaleFilterType;
    typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(importFilter->GetOutput());
    rescaleFilter->SetOutputMinimum(0.0);
    rescaleFilter->SetOutputMaximum(1.0);
    rescaleFilter->Update();
    scaled = rescaleFilter->GetOutput();
  }

  // Histogram equalization.
  else if (scalingHistogram) {
    typedef itk::AdaptiveHistogramEqualizationImageFilter<UncertaintyListType> AdaptiveHistogramEqualizationImageFilterType;
    AdaptiveHistogramEqualizationImageFilterType::Pointer histogramEqualizationFilter = AdaptiveHistogramEqualizationImageFilterType::New();
    histogramEqualizationFilter->SetInput(importFilter->GetOutput());
    histogramEqualizationFilter->SetAlpha(0);
    histogramEqualizationFilter->SetBeta(0);
    histogramEqualizationFilter->SetRadius(1);
    histogramEqualizationFilter->Update();
    scaled = histogramEqualizationFilter->GetOutput();
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
    // Black and White
    if (blackAndWhite) {
      normalColour[0] = intensity;
      normalColour[1] = intensity;
      normalColour[2] = intensity;
    }
    // Colour
    else if (colour) {
      normalColour[0] = 255 - intensity;
      normalColour[1] = 0;
      normalColour[2] = 0;
    }
    colors->InsertNextTupleValue(normalColour);
  }
  
  // Set the colours to be the scalar value of each point.
  surfacePolyData->GetPointData()->SetScalars(colors);

  mitk::ProgressBar::GetInstance()->Progress();
}