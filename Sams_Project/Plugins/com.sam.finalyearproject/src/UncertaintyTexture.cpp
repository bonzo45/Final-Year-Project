#include "UncertaintyTexture.h"

#include "UncertaintySampler.h"
#include <mitkImageCast.h>
#include <itkRescaleIntensityImageFilter.h>

// Loading bar
#include "MitkLoadingBarCommand.h"
#include <mitkProgressBar.h>

void UncertaintyTexture::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);
}

void UncertaintyTexture::setDimensions(unsigned int width, unsigned int height) {
  this->textureWidth = width;
  this->textureHeight = height;
}

void UncertaintyTexture::setScalingLinear(bool scalingLinear) {
  this->scalingLinear = scalingLinear;
}

void UncertaintyTexture::clearSampling() {
  this->samplingAverage = false;
  this->samplingMinimum = false;
  this->samplingMaximum = false;
}

void UncertaintyTexture::setSamplingAverage() {
  clearSampling();
  this->samplingAverage = true;
}

void UncertaintyTexture::setSamplingMinimum() {
  clearSampling();
  this->samplingMinimum = true;
}

void UncertaintyTexture::setSamplingMaximum() {
  clearSampling();
  this->samplingMaximum = true;
}

/**
  * Generates a texture that represents the uncertainty of the uncertainty volume.
  * It works by projecting a point in the center of the volume outwards, onto a sphere.
  */
mitk::Image::Pointer UncertaintyTexture::generateUncertaintyTexture() {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(textureWidth * textureHeight);
  if (scalingLinear) {
    mitk::ProgressBar::GetInstance()->AddStepsToDo(100);
  }

  // Create a blank ITK image.
  TextureImageType::RegionType region;
  TextureImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
 
  TextureImageType::SizeType size;
  size[0] = textureWidth;
  size[1] = textureHeight;
 
  region.SetSize(size);
  region.SetIndex(start);

  TextureImageType::Pointer uncertaintyTexture = TextureImageType::New();
  uncertaintyTexture->SetRegions(region);
  uncertaintyTexture->Allocate();

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

  // Compute center of uncertainty data.
  vtkVector<float, 3> center = vtkVector<float, 3>();
  center[0] = ((float) uncertaintyHeight - 1) / 2.0;
  center[1] = ((float) uncertaintyWidth - 1) / 2.0;
  center[2] = ((float) uncertaintyDepth - 1) / 2.0;

  // For each pixel in the texture, sample the uncertainty data.
  for (unsigned int r = 0; r < textureHeight; r++) {
    for (unsigned int c = 0; c < textureWidth; c++) {
      // Compute spherical coordinates: phi (longitude) & theta (latitude).
      float theta = ((float) r / (float) textureHeight) * M_PI;
      float phi = ((float) c / (float) textureWidth) * (2 * M_PI);
      
      // Compute point on sphere with radius 1. This is also the vector from the center of the sphere to the point.
      vtkVector<float, 3> direction = vtkVector<float, 3>();
      direction[0] = cos(phi) * sin(theta);
      direction[1] = cos(theta);
      direction[2] = sin(phi) * sin(theta);
      direction.Normalize();

      // Sample the uncertainty data.
      int pixelValue = sampler->sampleUncertainty(center, direction) * 255;
      
      // Set texture value.
      TextureImageType::IndexType pixelIndex;
      pixelIndex[0] = c;
      pixelIndex[1] = r;

      uncertaintyTexture->SetPixel(pixelIndex, pixelValue);
      mitk::ProgressBar::GetInstance()->Progress();
    }
  }
  delete sampler;

  // Scale the texture values to increase contrast.
  if (scalingLinear) {
    typedef itk::RescaleIntensityImageFilter<TextureImageType, TextureImageType> RescaleFilterType;
    typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(uncertaintyTexture);
    rescaleFilter->SetOutputMinimum(0);
    rescaleFilter->SetOutputMaximum(255);
    MitkLoadingBarCommand::Pointer command = MitkLoadingBarCommand::New();
    command->Initialize(100, false);
    rescaleFilter->AddObserver(itk::ProgressEvent(), command);
    rescaleFilter->Update();
    uncertaintyTexture = rescaleFilter->GetOutput();
    legendMinValue = rescaleFilter->GetInputMinimum() / 255.0;
    legendMaxValue = rescaleFilter->GetInputMaximum() / 255.0;
  }
  else {
    legendMinValue = 0.0;
    legendMaxValue = 1.0;
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(uncertaintyTexture, result);
  return result;
}

double UncertaintyTexture::getLegendMinValue() {
  return legendMinValue;
}

double UncertaintyTexture::getLegendMaxValue() {
  return legendMaxValue;
}

void UncertaintyTexture::getLegendMinColour(char * colour) {
  colour[0] = 0;
  colour[1] = 0;
  colour[2] = 0;
}

void UncertaintyTexture::getLegendMaxColour(char * colour) {
  colour[0] = 255;
  colour[1] = 255;
  colour[2] = 255;
}