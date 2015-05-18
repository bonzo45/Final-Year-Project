#include "UncertaintyTextureGenerator.h"

#include "UncertaintySampler.h"
#include <mitkImageCast.h>
#include <itkRescaleIntensityImageFilter.h>

// Loading bar
#include "MitkLoadingBarCommand.h"
#include <mitkProgressBar.h>

/**
  * Sets the uncertainty to make a texture from.
  */
void UncertaintyTextureGenerator::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);
}

/**
  * Sets the dimensions of the texture.
  */
void UncertaintyTextureGenerator::setDimensions(unsigned int width, unsigned int height) {
  this->textureWidth = width;
  this->textureHeight = height;
}

/**
  * Sets whether to do linear scaling. No scaling if false.
  */
void UncertaintyTextureGenerator::setScalingLinear(bool scalingLinear) {
  this->scalingLinear = scalingLinear;
}

/**
  * Resets the sampling variables.
  */
void UncertaintyTextureGenerator::clearSampling() {
  this->samplingAverage = false;
  this->samplingMinimum = false;
  this->samplingMaximum = false;
}

/**
  * Sets sampling to average.
  */
void UncertaintyTextureGenerator::setSamplingAverage() {
  clearSampling();
  this->samplingAverage = true;
}

/**
  * Sets sampling to minimum.
  */
void UncertaintyTextureGenerator::setSamplingMinimum() {
  clearSampling();
  this->samplingMinimum = true;
}

/**
  * Sets sampling to maximum.
  */
void UncertaintyTextureGenerator::setSamplingMaximum() {
  clearSampling();
  this->samplingMaximum = true;
}

/**
  * Generates a texture that represents the uncertainty of the uncertainty volume.
  * It works by projecting a point in the center of the volume outwards, onto a sphere.
  */
mitk::Image::Pointer UncertaintyTextureGenerator::generateUncertaintyTextureGenerator() {
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

  TextureImageType::Pointer UncertaintyTextureGenerator = TextureImageType::New();
  UncertaintyTextureGenerator->SetRegions(region);
  UncertaintyTextureGenerator->Allocate();

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

      UncertaintyTextureGenerator->SetPixel(pixelIndex, pixelValue);
      mitk::ProgressBar::GetInstance()->Progress();
    }
  }
  delete sampler;

  // Scale the texture values to increase contrast.
  if (scalingLinear) {
    typedef itk::RescaleIntensityImageFilter<TextureImageType, TextureImageType> RescaleFilterType;
    typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(UncertaintyTextureGenerator);
    rescaleFilter->SetOutputMinimum(0);
    rescaleFilter->SetOutputMaximum(255);
    MitkLoadingBarCommand::Pointer command = MitkLoadingBarCommand::New();
    command->Initialize(100, false);
    rescaleFilter->AddObserver(itk::ProgressEvent(), command);
    rescaleFilter->Update();
    UncertaintyTextureGenerator = rescaleFilter->GetOutput();
    legendMinValue = rescaleFilter->GetInputMinimum() / 255.0;
    legendMaxValue = rescaleFilter->GetInputMaximum() / 255.0;
  }
  else {
    legendMinValue = 0.0;
    legendMaxValue = 1.0;
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer result;
  mitk::CastToMitkImage(UncertaintyTextureGenerator, result);
  return result;
}

/**
  * For legend support. Returns the minimum value.
  */
double UncertaintyTextureGenerator::getLegendMinValue() {
  return legendMinValue;
}

/**
  * For legend support. Returns the maximum value.
  */
double UncertaintyTextureGenerator::getLegendMaxValue() {
  return legendMaxValue;
}

/**
  * For legend support. Returns the minimum colour.
  */
void UncertaintyTextureGenerator::getLegendMinColour(char * colour) {
  colour[0] = 0;
  colour[1] = 0;
  colour[2] = 0;
}

/**
  * For legend support. Returns the maximum colour.
  */
void UncertaintyTextureGenerator::getLegendMaxColour(char * colour) {
  colour[0] = 255;
  colour[1] = 255;
  colour[2] = 255;
}