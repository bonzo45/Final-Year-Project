#include "UncertaintyPreprocessor.h"

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

// Loading bar
#include "MitkLoadingBarCommand.h"
#include <mitkProgressBar.h>

/**
  * Set the scan corresponding to the uncertainty.
  * This is used to align the uncertainty to it (if enabled).
  */
void UncertaintyPreprocessor::setScan(mitk::Image::Pointer scan) {
  this->scan = scan;
}

/**
  * Set the uncertainty to preprocess.
  */
void UncertaintyPreprocessor::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
}

/**
  * Set the range to normalize the uncertainty to.
  */
void UncertaintyPreprocessor::setNormalizationParams(double min, double max) {
  this->normalizationMin = min;
  this->normalizationMax = max;
}

/**
  * Configure the erosion. 
  * erodeThickness - number of pixels to erode.
  */
void UncertaintyPreprocessor::setErodeParams(int erodeThickness) {
  this->erodeErodeThickness = erodeThickness;
}

mitk::Image::Pointer UncertaintyPreprocessor::preprocessUncertainty(bool invert, bool erode, bool align) {
  // We always normalize, everything else is optional.
  unsigned int stepsToDo = 100;
  if (invert) {
    stepsToDo += 100;
  }
  if (erode) {
    stepsToDo += 100;
  }
  if (align) {
    stepsToDo += 1;
  }
  mitk::ProgressBar::GetInstance()->AddStepsToDo(stepsToDo);

  // ------------------- //
  // ---- Normalize ---- //
  // ------------------- //
  mitk::Image::Pointer normalizedMitkImage;
  AccessByItk_1(this->uncertainty, ItkNormalizeUncertainty, normalizedMitkImage);

  // ------------------- //
  // ------ Invert ----- //
  // ------------------- //
  // (if enabled)
  mitk::Image::Pointer invertedMitkImage = normalizedMitkImage;
  if (invert) {
    AccessByItk_1(normalizedMitkImage, ItkInvertUncertainty, invertedMitkImage);
  }
  
  // ------------------- //
  // ------ Erode ------ //
  // ------------------- //
  // (if enabled)
  mitk::Image::Pointer erodedMitkImage = invertedMitkImage;
  if (erode) {
    AccessByItk_1(invertedMitkImage, ItkErodeUncertainty, erodedMitkImage);
  }

  // ------------------- //
  // ------ Align ------ //
  // ------------------- //
  // (if enabled)
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

    mitk::ProgressBar::GetInstance()->Progress();
  }

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
    MitkLoadingBarCommand::Pointer command = MitkLoadingBarCommand::New();
    command->Initialize(100, false);
    windowFilter->AddObserver(itk::ProgressEvent(), command);
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
    MitkLoadingBarCommand::Pointer command = MitkLoadingBarCommand::New();
    command->Initialize(100, false);
    rescaleFilter->AddObserver(itk::ProgressEvent(), command);
    rescaleFilter->Update();

    // Convert to MITK
    ResultType * scaledImage = rescaleFilter->GetOutput();
    mitk::CastToMitkImage(scaledImage, result);
  }
}

/**
  * Inverts an ITK Image.
  */
template <typename TPixel, unsigned int VImageDimension>
void UncertaintyPreprocessor::ItkInvertUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::InvertIntensityImageFilter <ImageType> InvertIntensityImageFilterType;
 
  // Invert the image.
  typename InvertIntensityImageFilterType::Pointer invertIntensityFilter = InvertIntensityImageFilterType::New();
  invertIntensityFilter->SetInput(itkImage);
  invertIntensityFilter->SetMaximum(normalizationMax);
  MitkLoadingBarCommand::Pointer command = MitkLoadingBarCommand::New();
  command->Initialize(100, false);
  invertIntensityFilter->AddObserver(itk::ProgressEvent(), command);
  invertIntensityFilter->Update();

  // Convert to MITK
  ImageType * invertedImage = invertIntensityFilter->GetOutput();
  mitk::CastToMitkImage(invertedImage, result);
}

/**
  * Erodes an ITK Image.
  * See setErodeParams for explanation of parameters.
  */
template <typename TPixel, unsigned int VImageDimension>
void UncertaintyPreprocessor::ItkErodeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;

  // -------------------------------------- //
  // ---- Threshold to find background ---- //
  // -------------------------------------- //
  // Create a thresholder.
  typedef itk::BinaryThresholdImageFilter<ImageType, ImageType> BinaryThresholdImageFilterType;
  typename BinaryThresholdImageFilterType::Pointer thresholdFilter = BinaryThresholdImageFilterType::New();
  thresholdFilter->SetInput(itkImage);
  thresholdFilter->SetInsideValue(1);
  thresholdFilter->SetOutsideValue(0);
  thresholdFilter->SetLowerThreshold(0.0);
  thresholdFilter->SetUpperThreshold(0.0);
  MitkLoadingBarCommand::Pointer command = MitkLoadingBarCommand::New();
  command->Initialize(30, false);
  thresholdFilter->AddObserver(itk::ProgressEvent(), command);
  thresholdFilter->Update();

  // ------------------------------ //
  // -------- Growing mask -------- //
  // ------------------------------ //
  // Grow the threshold to remove background.
  typedef itk::BinaryBallStructuringElement<TPixel, VImageDimension> StructuringElementType;
  StructuringElementType dilationStructuringElement;
  dilationStructuringElement.SetRadius(erodeErodeThickness);
  dilationStructuringElement.CreateStructuringElement();

  typedef itk::GrayscaleDilateImageFilter<ImageType, ImageType, StructuringElementType> GrayscaleDilateImageFilterType;
  typename GrayscaleDilateImageFilterType::Pointer dilateFilter = GrayscaleDilateImageFilterType::New();
  dilateFilter->SetInput(thresholdFilter->GetOutput());
  dilateFilter->SetKernel(dilationStructuringElement);
  command = MitkLoadingBarCommand::New();
  command->Initialize(40, false);
  dilateFilter->AddObserver(itk::ProgressEvent(), command);
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
  command = MitkLoadingBarCommand::New();
  command->Initialize(30, false);
  masker->AddObserver(itk::ProgressEvent(), command);
  masker->Update();

  // ------------------------- //
  // -------- Return --------- //
  // ------------------------- //
  // Convert to MITK
  mitk::CastToMitkImage(masker->GetOutput(), result);
}