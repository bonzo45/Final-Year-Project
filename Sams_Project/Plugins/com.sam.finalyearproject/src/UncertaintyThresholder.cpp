#include "UncertaintyThresholder.h"

#include <cfloat> // for DBL_MIN
#include <mitkImageAccessByItk.h>
#include <mitkImageCast.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImageToHistogramFilter.h>

UncertaintyThresholder::UncertaintyThresholder() {
  this->ignoreZeros = false;
} 

void UncertaintyThresholder::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
}

void UncertaintyThresholder::setIgnoreZeros(bool ignoreZeros) {
	this->ignoreZeros = ignoreZeros;
}

mitk::Image::Pointer UncertaintyThresholder::thresholdUncertainty(double min, double max) {
	mitk::Image::Pointer thresholdedImage;
	AccessByItk_3(this->uncertainty, ItkThresholdUncertainty, min, max, thresholdedImage);
	return thresholdedImage;
}

void UncertaintyThresholder::getTopXPercentThreshold(int percentage, double & min, double & max) {
  // Get the thresholds corresponding to percentage%
  AccessByItk_3(this->uncertainty, ItkTopXPercentThreshold, percentage / 100.0, min, max);
}

/**
  * Uses ITK to do the thresholding.
  */
template <typename TPixel, unsigned int VImageDimension>
void UncertaintyThresholder::ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, double min, double max, mitk::Image::Pointer & result) {
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
  thresholdFilter->SetInput(itkImage);
  thresholdFilter->SetInsideValue(1);
  thresholdFilter->SetOutsideValue(0);
  thresholdFilter->SetLowerThreshold(min);
  thresholdFilter->SetUpperThreshold(max);

  // Compute result.
  thresholdFilter->Update();
  ImageType * thresholdedImage = thresholdFilter->GetOutput();
  mitk::CastToMitkImage(thresholdedImage, result);
}

template <typename TPixel, unsigned int VImageDimension>
void UncertaintyThresholder::ItkTopXPercentThreshold(itk::Image<TPixel, VImageDimension>* itkImage, double percentage, double & lowerThreshold, double & upperThreshold) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::Statistics::ImageToHistogramFilter<ImageType> ImageToHistogramFilterType;

  const unsigned int measurementComponents = 1; // Grayscale
  const unsigned int binsPerDimension = 1000;

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
  imageToHistogramFilter->Update();

  // Get the resultant histogram. It has binsPerDimension buckets.
  typename ImageToHistogramFilterType::HistogramType * histogram = imageToHistogramFilter->GetOutput();

  // We know that in total there are uncertaintyX * uncertaintyY * uncertaintyZ pixels.
  typename ImageType::RegionType region = itkImage->GetLargestPossibleRegion();
  typename ImageType::SizeType regionSize = region.GetSize();
  unsigned int totalPixels = regionSize[0] * regionSize[1] * regionSize[2];  

  // If we're ignoring zero values then the total gets reduced. The histogram entry gets zeroed as well.
  if (ignoreZeros) {
    totalPixels -= histogram->GetFrequency(0);
    histogram->SetFrequency(0, 0);
  }

  // The 10% threshold is therefore at 0.1 * total. Go through the histogram (effectively compute CDF) until we reach this.
  unsigned int pixelCount = 0;
  unsigned int i = 0;
  while ((pixelCount < totalPixels * percentage) && (i < binsPerDimension)) {
    pixelCount += histogram->GetFrequency(i);
    i++;
  }

  // 'Return' the threshold values.
  lowerThreshold = 0.0;
  upperThreshold = (double) i / (double) binsPerDimension;
}