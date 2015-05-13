#include "UncertaintyThresholder.h"

#include <cfloat> // for DBL_MIN
#include <mitkImageAccessByItk.h>
#include <mitkImageCast.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImageToHistogramFilter.h>

// Loading bar
#include "MitkLoadingBarCommand.h"
#include <mitkProgressBar.h>

UncertaintyThresholder::UncertaintyThresholder() {
  this->ignoreZeros = false;
  this->measurementComponents = 1; // Grayscale
  this->binsPerDimension = 1000;
  this->histogram = NULL;
  this->totalPixels = 0;
}

UncertaintyThresholder::~UncertaintyThresholder() {
  if (histogram) {
    delete histogram;
  }
}

void UncertaintyThresholder::setUncertainty(mitk::Image::Pointer uncertainty) {
  if (this->uncertainty == uncertainty) {
    return;
  }
  this->uncertainty = uncertainty;
  if (histogram) {
    delete histogram;
    histogram = NULL;
    totalPixels = 0;
  }
}

void UncertaintyThresholder::setIgnoreZeros(bool ignoreZeros) {
	this->ignoreZeros = ignoreZeros;
}

mitk::Image::Pointer UncertaintyThresholder::thresholdUncertainty(double min, double max) {
	mitk::Image::Pointer thresholdedImage;
	AccessByItk_3(this->uncertainty, ItkThresholdUncertainty, min, max, thresholdedImage);
	return thresholdedImage;
}

void UncertaintyThresholder::getTopXPercentThreshold(double percentage, double & min, double & max) {
  // If we've not previously generated the histogram, generate it.
  if (!histogram) {
    histogram = new unsigned int[binsPerDimension];
    AccessByItk_2(this->uncertainty, ItkComputePercentages, histogram, totalPixels);
  }

  // Work out the number of pixels we need to get to reach percentage.
  unsigned int goalPixels;
  unsigned int i;
  // If we're ignoring zeros the goal will be less.
  if (ignoreZeros) {
    goalPixels = (totalPixels - histogram[0]) * percentage;
    i = 1;
  }
  else {
    goalPixels = totalPixels * percentage;
    i = 0;
  }

  if (DEBUGGING) {
    std::cout << "Total Pixels: " << totalPixels << std::endl << 
                 "Goal Pixels: " << goalPixels << std::endl;
  }

  // Go through the histogram (effectively comuting the CDF) until we reach our goal.
  unsigned int pixelCount = 0;
  while ((pixelCount < goalPixels) && (i < binsPerDimension)) {
    pixelCount += histogram[i];
    i++;
  }

  // 'Return' the threshold values.
  min = 0.0;
  max = (double) i / (double) binsPerDimension;
}

/**
  * Uses ITK to do the thresholding.
  */
template <typename TPixel, unsigned int VImageDimension>
void UncertaintyThresholder::ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, double min, double max, mitk::Image::Pointer & result) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::BinaryThresholdImageFilter<ImageType, ImageType> BinaryThresholdImageFilterType;
  
  // Check if we're ignoring zeros.
  if (ignoreZeros) {
    double epsilon = DBL_MIN;
    min = std::max(epsilon, min);
    max = std::max(epsilon, max);
  }

  // Create a thresholder.
  typename BinaryThresholdImageFilterType::Pointer thresholdFilter = BinaryThresholdImageFilterType::New();
  thresholdFilter->AddObserver(itk::ProgressEvent(), MitkLoadingBarCommand::New());
  thresholdFilter->SetInput(itkImage);
  thresholdFilter->SetInsideValue(1);
  thresholdFilter->SetOutsideValue(0);
  thresholdFilter->SetLowerThreshold(min);
  thresholdFilter->SetUpperThreshold(max);

  // Compute result.
  thresholdFilter->Update();
  ImageType * thresholdedImage = thresholdFilter->GetOutput();
  mitk::CastToMitkImage(thresholdedImage, result);
  mitk::ProgressBar::GetInstance()->Progress();
}

template <typename TPixel, unsigned int VImageDimension>
void UncertaintyThresholder::ItkComputePercentages(itk::Image<TPixel, VImageDimension>* itkImage, unsigned int * histogram, unsigned int & totalPixels) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::Statistics::ImageToHistogramFilter<ImageType> ImageToHistogramFilterType;

  // Customise the filter.
  typename ImageToHistogramFilterType::HistogramType::MeasurementVectorType lowerBound(binsPerDimension);
  lowerBound.Fill(0.0);
 
  typename ImageToHistogramFilterType::HistogramType::MeasurementVectorType upperBound(binsPerDimension);
  upperBound.Fill(1.0);
 
  typename ImageToHistogramFilterType::HistogramType::SizeType size(measurementComponents);
  size.Fill(binsPerDimension);

  // Create the filter.
  typename ImageToHistogramFilterType::Pointer imageToHistogramFilter = ImageToHistogramFilterType::New();
  imageToHistogramFilter->SetInput(itkImage);
  imageToHistogramFilter->SetHistogramBinMinimum(lowerBound);
  imageToHistogramFilter->SetHistogramBinMaximum(upperBound);
  imageToHistogramFilter->SetHistogramSize(size);
  imageToHistogramFilter->AddObserver(itk::ProgressEvent(), MitkLoadingBarCommand::New());
  imageToHistogramFilter->Update();

  // Get the resultant histogram. It has binsPerDimension buckets.
  typename ImageToHistogramFilterType::HistogramType::Pointer computedHistogram = imageToHistogramFilter->GetOutput();

  // We know that in total there are uncertaintyX * uncertaintyY * uncertaintyZ pixels.
  typename ImageType::RegionType region = itkImage->GetLargestPossibleRegion();
  typename ImageType::SizeType regionSize = region.GetSize();
  totalPixels = regionSize[0] * regionSize[1] * regionSize[2];  

  unsigned int pixelCount = 0;
  for (unsigned int i = 0; i < binsPerDimension; i++) {
    histogram[i] = computedHistogram->GetFrequency(i);
  }

  mitk::ProgressBar::GetInstance()->Progress();
}