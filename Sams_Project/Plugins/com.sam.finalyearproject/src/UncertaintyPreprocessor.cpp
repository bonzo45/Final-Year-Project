#include "UncertaintyPreprocessor.h"

// ----------------- //
// ---- Filters ---- //
// ----------------- //
#include <mitkImageAccessByItk.h>
#include <mitkImageCast.h>
// Normalize
#include <itkRescaleIntensityImageFilter.h>
#include <itkIntensityWindowingImageFilter.h>
// Invert
#include <itkInvertIntensityImageFilter.h>
// Erode
#include <itkBinaryBallStructuringElement.h>
#include <itkGrayscaleErodeImageFilter.h>
#include <itkSubtractImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkGrayscaleDilateImageFilter.h>
#include <itkMaskImageFilter.h>

// -------------- //
// ---- MITK ---- //
// -------------- //
// Loading bar
#include <mitkProgressBar.h>

// ------------------------ //
// ---- Implementation ---- //
// ------------------------ //

void UncertaintyPreprocessor::setScan(mitk::Image::Pointer scan) {
  this->scan = scan;
}

void UncertaintyPreprocessor::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
}

void UncertaintyPreprocessor::setNormalizationParams(double min, double max) {
  this->normalizationMin = min;
  this->normalizationMax = max;
}

void UncertaintyPreprocessor::setErodeParams(int erodeThickness, double erodeThreshold, int dilateThickness) {
  this->erodeErodeThickness = erodeThickness;
  this->erodeThreshold = erodeThreshold;
  this->erodeDilateThickness = dilateThickness;
}

mitk::Image::Pointer UncertaintyPreprocessor::preprocessUncertainty(bool invert, bool erode, bool align) {
  mitk::ProgressBar::GetInstance()->AddStepsToDo(4);

  // ------------------- //
  // ---- Normalize ---- //
  // ------------------- //
  mitk::Image::Pointer normalizedMitkImage;
  AccessByItk_1(this->uncertainty, ItkNormalizeUncertainty, normalizedMitkImage);
  
  mitk::ProgressBar::GetInstance()->Progress();
  
  // ------------------- //
  // ------ Invert ----- //
  // ------------------- //
  // (if enabled)
  mitk::Image::Pointer invertedMitkImage = normalizedMitkImage;
  if (invert) {
    AccessByItk_1(normalizedMitkImage, ItkInvertUncertainty, invertedMitkImage);
  }
  mitk::ProgressBar::GetInstance()->Progress();
  
  // ------------------- //
  // ------ Erode ------ //
  // ------------------- //
  // (if enabled)
  mitk::Image::Pointer erodedMitkImage = invertedMitkImage;
  if (erode) {
    AccessByItk_1(invertedMitkImage, ItkErodeUncertainty, erodedMitkImage);
  }

  mitk::ProgressBar::GetInstance()->Progress();

  // ------------------- //
  // ------ Align ------ //
  // ------------------- //
  mitk::Image::Pointer fullyProcessedMitkImage = erodedMitkImage;
  if (align) {
    // Align the scan and uncertainty.
    // Get the origin and index to world transform of the scan.
    mitk::SlicedGeometry3D * scanSlicedGeometry = this->scan->GetSlicedGeometry();
    mitk::Point3D scanOrigin = scanSlicedGeometry->GetOrigin();
    mitk::AffineTransform3D * scanTransform = scanSlicedGeometry->GetIndexToWorldTransform();

    // Set the origin and index to world transform of the uncertainty to be the same.
    // This effectively lines up pixel (x, y, z) in the scan with pixel (x, y, z) in the uncertainty.
    mitk::SlicedGeometry3D * uncertaintySlicedGeometry = fullyProcessedMitkImage->GetSlicedGeometry();
    uncertaintySlicedGeometry->SetOrigin(scanOrigin);
    uncertaintySlicedGeometry->SetIndexToWorldTransform(scanTransform);
  }

  mitk::ProgressBar::GetInstance()->Progress();

  return fullyProcessedMitkImage;
}

/**
  * Case 1: If itkImage contains characters (0-255) then map the range (0-255) to (0.0-1.0).
  * Case 2: If itkImage contains anything else just map (min-max) to (0.0-1.0).
  */
template <typename TPixel, unsigned int VImageDimension>
void UncertaintyPreprocessor::ItkNormalizeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::Image<double, 3> ResultType;
  
  itk::Image<unsigned char, 3> * charImage = dynamic_cast<itk::Image<unsigned char, 3>* >(itkImage);
  // Case 1
  if (charImage) {
    typedef itk::IntensityWindowingImageFilter< ImageType, ResultType> IntensityWindowingFilterType;

    typename IntensityWindowingFilterType::Pointer windowFilter = IntensityWindowingFilterType::New();
    windowFilter->SetInput(itkImage);
    windowFilter->SetOutputMinimum(0.0);
    windowFilter->SetOutputMaximum(1.0);
    windowFilter->SetWindowMinimum(0);
    windowFilter->SetWindowMaximum(255);
    windowFilter->Update();

    // Convert to MITK
    ResultType * scaledImage = windowFilter->GetOutput();
    mitk::CastToMitkImage(scaledImage, result);
  }
  // Case 2
  else {
    typedef itk::RescaleIntensityImageFilter<ImageType, ResultType> RescaleFilterType;

    // Scale all the values.
    typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(itkImage);
    rescaleFilter->SetOutputMinimum(normalizationMin);
    rescaleFilter->SetOutputMaximum(normalizationMax);
    rescaleFilter->Update();

    // Convert to MITK
    ResultType * scaledImage = rescaleFilter->GetOutput();
    mitk::CastToMitkImage(scaledImage, result);
  }
}

template <typename TPixel, unsigned int VImageDimension>
void UncertaintyPreprocessor::ItkInvertUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::InvertIntensityImageFilter <ImageType> InvertIntensityImageFilterType;
 
  // Invert the image.
  typename InvertIntensityImageFilterType::Pointer invertIntensityFilter = InvertIntensityImageFilterType::New();
  invertIntensityFilter->SetInput(itkImage);
  invertIntensityFilter->SetMaximum(normalizationMax);
  invertIntensityFilter->Update();

  // Convert to MITK
  ImageType * invertedImage = invertIntensityFilter->GetOutput();
  mitk::CastToMitkImage(invertedImage, result);
}

template <typename TPixel, unsigned int VImageDimension>
void UncertaintyPreprocessor::ItkErodeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;

  // ------------------------- //
  // ---- Initial Erosion ---- //
  // ------------------------- //
  // Create the erosion kernel, this describes how the data is eroded.
  typedef itk::BinaryBallStructuringElement<TPixel, VImageDimension> StructuringElementType;
  StructuringElementType structuringElement;
  structuringElement.SetRadius(erodeErodeThickness);
  structuringElement.CreateStructuringElement();
 
  // Create an erosion filter, using the kernel.
  typedef itk::GrayscaleErodeImageFilter<ImageType, ImageType, StructuringElementType> GrayscaleErodeImageFilterType;
  typename GrayscaleErodeImageFilterType::Pointer erodeFilter = GrayscaleErodeImageFilterType::New();
  erodeFilter->SetInput(itkImage);
  erodeFilter->SetKernel(structuringElement);
  erodeFilter->Update();

  // ------------------------- //
  // ------ Subtraction ------ //
  // ------------------------- //
  // Then we use a subtract filter to get the pixels that changed in the erosion.
  typedef itk::SubtractImageFilter<ImageType> SubtractType;
  typename SubtractType::Pointer diff = SubtractType::New();
  diff->SetInput1(itkImage);
  diff->SetInput2(erodeFilter->GetOutput());
  diff->Update();

  // ------------------------- //
  // ----- Thresholding ------ //
  // ------------------------- //
  // We only really want to remove the edges. These will have changed more significantly than the rest.
  typedef itk::BinaryThresholdImageFilter<ImageType, ImageType> BinaryThresholdImageFilterType;
  typename BinaryThresholdImageFilterType::Pointer thresholdFilter = BinaryThresholdImageFilterType::New();
  thresholdFilter->SetInput(diff->GetOutput());
  thresholdFilter->SetInsideValue(1.0);
  thresholdFilter->SetOutsideValue(0.0);
  thresholdFilter->SetLowerThreshold(erodeThreshold);
  thresholdFilter->SetUpperThreshold(1.0);
  thresholdFilter->Update();

  // ------------------------- //
  // -------- Growing -------- //
  // ------------------------- //
  // Then, because we'll still be left with a bit of a ring around the edge, we grow the mask.
  StructuringElementType dilationStructuringElement;
  dilationStructuringElement.SetRadius(erodeDilateThickness);
  dilationStructuringElement.CreateStructuringElement();

  typedef itk::GrayscaleDilateImageFilter<ImageType, ImageType, StructuringElementType> GrayscaleDilateImageFilterType;
  typename GrayscaleDilateImageFilterType::Pointer dilateFilter = GrayscaleDilateImageFilterType::New();
  dilateFilter->SetInput(thresholdFilter->GetOutput());
  dilateFilter->SetKernel(dilationStructuringElement);
  dilateFilter->Update();

  // ------------------------- //
  // -------- Masking -------- //
  // ------------------------- //
  typedef itk::MaskImageFilter<ImageType, ImageType, ImageType> MaskImageFilterType;
  typename MaskImageFilterType::Pointer masker = MaskImageFilterType::New();
  masker->SetInput(itkImage);
  masker->SetMaskImage(dilateFilter->GetOutput());
  masker->SetMaskingValue(1.0);
  masker->SetOutsideValue(0.0);
  masker->Update();

  // ------------------------- //
  // -------- Return --------- //
  // ------------------------- //
  // Convert to MITK
  mitk::CastToMitkImage(masker->GetOutput(), result);
}