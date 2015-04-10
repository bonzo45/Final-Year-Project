/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date$
Version:   $Revision$ 
 
Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "Sams_View.h"

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qt
#include <QMessageBox>

// General
#include <mitkBaseProperty.h>
#include <mitkIRenderWindowPart.h>
#include <mitkILinkedRenderWindowPart.h>
#include <mitkIRenderingManager.h>
#include <mitkNodePredicateProperty.h>

#include <cmath> // for abs
#include <algorithm> // for min/max
#include <cstdlib>

// 1
#include "itkRescaleIntensityImageFilter.h"
#include "itkInvertIntensityImageFilter.h"

// 2
//  a. Thresholding
#include <mitkImageAccessByItk.h>
#include <itkBinaryThresholdImageFilter.h>
#include <mitkImageCast.h>
#include <itkMinimumMaximumImageCalculator.h>
#include "itkImageToHistogramFilter.h"

//  a. Volume Rendering
#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>

//  b. Sphere Texture
#include <vtkSphereSource.h>
#include <vtkTextureMapToSphere.h>
#include <mitkSurface.h>
#include <mitkSmartPointerProperty.h>
#include <mitkNodePredicateDataType.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <itkImage.h>
#include <mitkITKImageImport.h>
#include <mitkShaderProperty.h>
#include <mitkImagePixelReadAccessor.h>

//  c. Surface Mapping
#include <vtkIdList.h>
#include <vtkCellArray.h>
#include <vtkMath.h>
#include <vtkFloatArray.h>

// 4
// - Overlay
#include <mitkBaseRenderer.h>
#include <QmitkRenderWindow.h>
#include <mitkOverlayManager.h>
#include <mitkTextOverlay2D.h>

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

// ------------------ //
// ---- TYPEDEFS ---- //
// ------------------ //
typedef itk::Image<unsigned char, 2>  TextureImageType;
typedef itk::Image<unsigned char, 3>  UncertaintyImageType;

// ------------------------- //
// ---- GLOBAL VARIABLES --- //
// ------------------------- //

// 1. Select Data & Uncertainty
//  - Scan
mitk::DataNode::Pointer scan;

//  - Uncertainty
mitk::DataNode::Pointer uncertainty;
mitk::DataNode::Pointer preprocessedUncertainty;

unsigned int uncertaintyHeight;
unsigned int uncertaintyWidth;
unsigned int uncertaintyDepth;

// 2. Visualisation
//  a. Uncertainty Thresholding
mitk::DataNode::Pointer thresholdedUncertainty = 0;
bool thresholdingEnabled = false;
double lowerThreshold = 0;
double upperThreshold = 0;

//  b. Uncertainty Sphere
TextureImageType::Pointer uncertaintyTexture = TextureImageType::New();

// 5. Test Uncertainties
UncertaintyImageType::Pointer cubeUncertainty;
UncertaintyImageType::Pointer randomUncertainty;
UncertaintyImageType::Pointer sphereUncertainty;

// ----------------------- //
// ---- IMPLEMENTATION --- //
// ----------------------- //

/**
  * Create the UI, connects up Signals and Slots.
  */
void Sams_View::CreateQtPartControl(QWidget *parent) {
  // Create Qt UI
  UI.setupUi(parent);

  // Add event handlers.
  // 1. Select Scan & Uncertainty
  connect(UI.buttonConfirmSelection, SIGNAL(clicked()), this, SLOT(ConfirmSelection()));
  connect(UI.buttonSwapScanUncertainty, SIGNAL(clicked()), this, SLOT(SwapScanUncertainty()));

  // 2.
  //  a. Thresholding
  connect(UI.checkBoxEnableThreshold, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyThresholding(bool)));
  connect(UI.sliderMinThreshold, SIGNAL(sliderMoved (int)), this, SLOT(LowerThresholdChanged(int)));
  connect(UI.sliderMaxThreshold, SIGNAL(sliderMoved (int)), this, SLOT(UpperThresholdChanged(int)));
  connect(UI.spinBoxTopXPercent, SIGNAL(valueChanged(int)), this, SLOT(TopXPercent(int)));
  connect(UI.buttonTop1Percent, SIGNAL(clicked()), this, SLOT(TopOnePercent()));
  connect(UI.buttonTop5Percent, SIGNAL(clicked()), this, SLOT(TopFivePercent()));
  connect(UI.buttonTop10Percent, SIGNAL(clicked()), this, SLOT(TopTenPercent()));
  connect(UI.checkBoxIgnoreZeros, SIGNAL(stateChanged(int)), this, SLOT(ThresholdUncertainty()));

  //  b. Texture Mapping
  connect(UI.buttonSphere, SIGNAL(clicked()), this, SLOT(GenerateUncertaintySphere()));

  //  c. Surface Mapping
  connect(UI.buttonSphereSurface, SIGNAL(clicked()), this, SLOT(GenerateSphereSurface()));
  connect(UI.buttonSurfaceMapping, SIGNAL(clicked()), this, SLOT(SurfaceMapping()));

  // 3. Options
  connect(UI.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(UI.buttonResetViews, SIGNAL(clicked()), this, SLOT(ResetViews()));

  // 4. Random
  connect(UI.buttonSetLayers, SIGNAL(clicked()), this, SLOT(SetLayers()));
  connect(UI.buttonOverlayText, SIGNAL(clicked()), this, SLOT(ShowTextOverlay()));

  // 5. Test Uncertainties
  connect(UI.buttonRandomUncertainty, SIGNAL(clicked()), this, SLOT(GenerateRandomUncertainty()));
  connect(UI.buttonCubeUncertainty, SIGNAL(clicked()), this, SLOT(GenerateCubeUncertainty()));
  connect(UI.buttonSphereUncertainty, SIGNAL(clicked()), this, SLOT(GenerateSphereUncertainty()));
  connect(UI.buttonQuadrantSphereUncertainty, SIGNAL(clicked()), this, SLOT(GenerateQuadrantSphereUncertainty()));

  SetNumberOfImagesSelected(0);
}

/**
  * What to do when the plugin window is selected.
  */
void Sams_View::SetFocus() {
  // Focus on something useful?
  //    e.g. UI.buttonOverlayText->setFocus();
}

// ------------------------------------------------------- //
// ---- I really shouldn't have had to write these... ---- //
// ------------------------------------------------------- //

vtkVector<float, 3> vectorAdd(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] + b[0];
  c[1] = a[1] + b[1];
  c[2] = a[2] + b[2];
  return c;
}

vtkVector<float, 3> vectorSubtract(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  c[2] = a[2] - b[2];
  return c;
}

// ----------- //
// ---- 1 ---- //
// ----------- //

/**
  * What to do when a data node or selection changes.
  */
void Sams_View::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList<mitk::DataNode::Pointer>& nodes) { 
  int imagesSelected = 0;

  // Go through all of the nodes that have been selected.
  foreach(mitk::DataNode::Pointer node, nodes) {
    // If it's null, ignore it.
    if (node.IsNull()) {
      continue;
    }

    // If it's an image.
    mitk::Image::Pointer image = dynamic_cast<mitk::Image*>(node->GetData());
    if(image) {
      imagesSelected++;

      // Set the first image selected to be the scan.
      if (imagesSelected == 1) {
        SelectScan(node);
      }
      // Set the second one to be the uncertainty.
      else if (imagesSelected == 2) {
        SelectUncertainty(node);
      }
    }
  }

  SetNumberOfImagesSelected(imagesSelected);
}

/**
  * Sets the scan to work with.
  */
void Sams_View::SelectScan(mitk::DataNode::Pointer scanNode) {
  scan = scanNode;

  // Update name label.
  mitk::BaseProperty * nameProperty = scan->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();
  UI.labelScanName->setText(QString::fromStdString(name));
}

/**
  * Sets the uncertainty to work with.
  */
void Sams_View::SelectUncertainty(mitk::DataNode::Pointer uncertaintyNode) {
  // Store a reference to the uncertainty DataNode.
  uncertainty = uncertaintyNode;

  // Update name label.
  mitk::BaseProperty * nameProperty = uncertainty->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();
  UI.labelUncertaintyName->setText(QString::fromStdString(name));
}

/**
  * Once the scan and uncertainty have been picked and the user confirms they are correct, we do some pre-processing on the data.
  */
void Sams_View::ConfirmSelection() {
  // Calculate dimensions.
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertainty->GetData());
  uncertaintyHeight = uncertaintyImage->GetDimension(0);
  uncertaintyWidth = uncertaintyImage->GetDimension(1);
  uncertaintyDepth = uncertaintyImage->GetDimension(2);

  // Preprocess uncertainty.
  PreprocessNode(uncertainty);

  // 2a. Thresholding
  // Update min/max values.
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << 0.0;
  UI.labelSliderLeftLimit->setText(ss.str().c_str());
  ss.str("");
  ss << std::setprecision(2) << std::fixed << 1.0;
  UI.labelSliderRightLimit->setText(ss.str().c_str());

  // Move sliders to start/end.
  SetLowerThreshold(0.0);
  SetUpperThreshold(1.0);

  // Enable visualisations.
  UI.tab2a->setEnabled(true);
  UI.tab2b->setEnabled(true);
  UI.tab2c->setEnabled(true);
  UI.buttonSetLayers->setEnabled(true);
}

/**
  * Swaps the scan and the uncertainty.
  */
void Sams_View::SwapScanUncertainty() {
  mitk::DataNode::Pointer temp = scan;
  this->SelectScan(uncertainty);
  this->SelectUncertainty(temp);
}

/**
  * Normalizes and Inverts (if enabled) a node.
  * Saves intermediate steps and result as a child node of the one given.
  */
void Sams_View::PreprocessNode(mitk::DataNode::Pointer node) {
  // Get image from node.
  mitk::Image::Pointer mitkImage = dynamic_cast<mitk::Image*>(node->GetData());

  // Normalize it.
  mitk::Image::Pointer normalizedMitkImage;
  AccessByItk_1(mitkImage, ItkNormalizeUncertainty, normalizedMitkImage);

  // Save normalized version. (if it already exists, replace it.)
  mitk::DataNode::Pointer normalizedNode = this->GetDataStorage()->GetNamedDerivedNode("Normalized", node);
  if (normalizedNode) {
    this->GetDataStorage()->Remove(normalizedNode);
  }
  normalizedNode = mitk::DataNode::New();
  normalizedNode->SetData(normalizedMitkImage);
  normalizedNode->SetProperty("name", mitk::StringProperty::New("Normalized"));
  normalizedNode->SetProperty("volumerendering", mitk::BoolProperty::New(true));
  this->GetDataStorage()->Add(normalizedNode, node);

  // Invert it. (if enabled)
  mitk::Image::Pointer invertedMitkImage;
  if (UI.checkBoxInversionEnabled->isChecked()) {
    AccessByItk_1(normalizedMitkImage, ItkInvertUncertainty, invertedMitkImage);
  }
  else {
    invertedMitkImage = normalizedMitkImage;
  }

  // Save completed version. (if it already exists, replace it.)
  preprocessedUncertainty = this->GetDataStorage()->GetNamedDerivedNode("Preprocessed", node);
  if (preprocessedUncertainty) {
    this->GetDataStorage()->Remove(preprocessedUncertainty);
  }
  preprocessedUncertainty = mitk::DataNode::New();
  preprocessedUncertainty->SetData(invertedMitkImage);
  preprocessedUncertainty->SetProperty("name", mitk::StringProperty::New("Preprocessed"));
  preprocessedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));
  this->GetDataStorage()->Add(preprocessedUncertainty, node);
}

template <typename TPixel, unsigned int VImageDimension>
void Sams_View::ItkNormalizeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::Image<double, 3> ResultType;
  typedef itk::RescaleIntensityImageFilter<ImageType, ResultType> RescaleFilterType;

  // Scale all the values.
  typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
  rescaleFilter->SetInput(itkImage);
  rescaleFilter->SetOutputMinimum(0.0);
  rescaleFilter->SetOutputMaximum(1.0);
  rescaleFilter->Update();

  // Convert to MITK
  ResultType * scaledImage = rescaleFilter->GetOutput();
  mitk::CastToMitkImage(scaledImage, result);
}

template <typename TPixel, unsigned int VImageDimension>
void Sams_View::ItkInvertUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::InvertIntensityImageFilter <ImageType> InvertIntensityImageFilterType;
 
  // Invert the image.
  typename InvertIntensityImageFilterType::Pointer invertIntensityFilter = InvertIntensityImageFilterType::New();
  invertIntensityFilter->SetInput(itkImage);
  invertIntensityFilter->SetMaximum(1.0);
  invertIntensityFilter->Update();

  // Convert to MITK
  ImageType * invertedImage = invertIntensityFilter->GetOutput();
  mitk::CastToMitkImage(invertedImage, result);
}

/**
  * Updates the UI to reflect the number of images selected.
  * Shows messages, disables buttons etc.
  */
void Sams_View::SetNumberOfImagesSelected(int imagesSelected) {
  if (imagesSelected == 0) {
    ScanSelected(false);
    UncertaintySelected(false);
    BothSelected(false);
  }
  else if (imagesSelected == 1) {
    ScanSelected(true);
    UncertaintySelected(false);
    BothSelected(false);
  }
  else {
    ScanSelected(true);
    UncertaintySelected(true);
    BothSelected(true);
  }
}

/**
  * Enables/Disables parts of the UI based on whether we have selected a scan.
  */
void Sams_View::ScanSelected(bool picked) {
  if (picked) {

  }
  else {
    UI.labelScanName->setText("Pick a Scan (Ctrl + Click)");
  }
}

/**
  * Enables/Disables parts of the UI based on whether we have selected an uncertainty.
  */
void Sams_View::UncertaintySelected(bool picked) {
  if (picked) {
    UI.checkBoxInversionEnabled->setVisible(true);
  }
  else {
    UI.labelUncertaintyName->setText("Pick an Uncertainty (Ctrl + Click)");
    UI.checkBoxInversionEnabled->setVisible(false);
  }
}

/**
  * Enables/Disables parts of the UI based on whether we have selected both a scan and an uncertainty.
  */
void Sams_View::BothSelected(bool picked) {
  if (picked) {
    UI.buttonConfirmSelection->setEnabled(true);
    UI.buttonSwapScanUncertainty->setEnabled(true);
  }
  else {
    UI.buttonConfirmSelection->setEnabled(false);
    UI.buttonSwapScanUncertainty->setEnabled(false);
    UI.tab2a->setEnabled(false);
    UI.tab2b->setEnabled(false);
    UI.tab2c->setEnabled(false);
    UI.buttonSetLayers->setEnabled(false);
  }
}

// ------------ //
// ---- 2a ---- //
// ------------ //

/**
  * Toggles thresholding
  */
void Sams_View::ToggleUncertaintyThresholding(bool checked) {
  if (checked) {
    thresholdingEnabled = true;
    ThresholdUncertainty();
  }
  else {
    thresholdingEnabled = false;
  }
}

/**
  * Creates a copy of the uncertainty data with values not between lowerThreshold and upperThreshold removed.
  */
void Sams_View::ThresholdUncertainty() {
  // Get the uncertainty image.
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(preprocessedUncertainty->GetData());

  // Threshold it.
  mitk::Image::Pointer thresholdedImage;
  AccessByItk_3(uncertaintyImage, ItkThresholdUncertainty, lowerThreshold, upperThreshold, thresholdedImage);

  // Save it. (replace if it already exists)
  if (thresholdedUncertainty) {
    this->GetDataStorage()->Remove(thresholdedUncertainty);
  }
  thresholdedUncertainty = mitk::DataNode::New();
  thresholdedUncertainty->SetData(thresholdedImage);
  thresholdedUncertainty->SetProperty("binary", mitk::BoolProperty::New(true));
  thresholdedUncertainty->SetProperty("name", mitk::StringProperty::New("Thresholded"));
  thresholdedUncertainty->SetProperty("color", mitk::ColorProperty::New(1.0, 0.0, 0.0));
  thresholdedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));
  thresholdedUncertainty->SetProperty("layer", mitk::IntProperty::New(1));
  thresholdedUncertainty->SetProperty("opacity", mitk::FloatProperty::New(0.5));
  this->GetDataStorage()->Add(thresholdedUncertainty, preprocessedUncertainty);

  this->RequestRenderWindowUpdate();
}

/**
  * Uses ITK to do the thresholding.
  */
template <typename TPixel, unsigned int VImageDimension>
void Sams_View::ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, double min, double max, mitk::Image::Pointer & result) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::BinaryThresholdImageFilter<ImageType, ImageType> BinaryThresholdImageFilterType;
  
  // Check if we're ignoring zeros.
  if (UI.checkBoxIgnoreZeros->isChecked()) {
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

/**
  * Set Lower Threshold (called by sliders)
  * - lower is between 0 and 1000
  */
void Sams_View::LowerThresholdChanged(int lower) {
  float temp = (1.0 / 1000) * lower;
  if (temp > upperThreshold) {
    UI.sliderMinThreshold->setValue(UI.sliderMaxThreshold->value());
    return;
  }
  lowerThreshold = temp;
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << lowerThreshold;
  UI.labelSliderLeft->setText(ss.str().c_str());

  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
}

/**
  * Set Upper Threshold (called by sliders)
    * - upper is between 0 and 1000
  */
void Sams_View::UpperThresholdChanged(int upper) {
  float temp = (1.0 / 1000) * upper;
  if (temp < lowerThreshold) {
    UI.sliderMaxThreshold->setValue(UI.sliderMinThreshold->value());
    return;
  }
  upperThreshold = temp;
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << upperThreshold;
  UI.labelSliderRight->setText(ss.str().c_str());

  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
}

/**
  * Sets the lower threshold to be a float value (between 0.0 and 1.0)
  */
void Sams_View::SetLowerThreshold(double lower) {
  int sliderValue = round(lower * 1000);
  sliderValue = std::min(sliderValue, 1000);
  sliderValue = std::max(sliderValue, 0);

  UI.sliderMinThreshold->setValue(sliderValue);
  LowerThresholdChanged(sliderValue);
}

/**
  * Sets the upper threshold to be a float value (between 0.0 and 1.0)
  */
void Sams_View::SetUpperThreshold(double upper) {
  int sliderValue = round(upper * 1000);
  sliderValue = std::min(sliderValue, 1000);
  sliderValue = std::max(sliderValue, 0);

  UI.sliderMaxThreshold->setValue(sliderValue);
  UpperThresholdChanged(sliderValue);
}

void Sams_View::TopOnePercent() {
  int percentage = 1;
  TopXPercent(percentage);
  UI.spinBoxTopXPercent->setValue(percentage);
}

void Sams_View::TopFivePercent() {
  int percentage = 5;
  TopXPercent(percentage);
  UI.spinBoxTopXPercent->setValue(percentage);
}

void Sams_View::TopTenPercent() {
  int percentage = 10;
  TopXPercent(percentage);
  UI.spinBoxTopXPercent->setValue(percentage);
}

/**
  * Shows the the worst uncertainty.
  *   percentage - between 0 and 100
  *   e.g. percentage = 10 will create a threshold to show the worst 10% of uncertainty.
  */
void Sams_View::TopXPercent(int percentage) {
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(preprocessedUncertainty->GetData());

  // Get the pixel value corresponding to 10%.
  double lowerThreshold;
  double upperThreshold;
  AccessByItk_3(uncertaintyImage, ItkTopXPercent, percentage / 100.0, lowerThreshold, upperThreshold);
  
  // Filter the uncertainty to only show the top 10%.
  SetLowerThreshold(lowerThreshold);
  SetUpperThreshold(upperThreshold);
}

template <typename TPixel, unsigned int VImageDimension>
void Sams_View::ItkTopXPercent(itk::Image<TPixel, VImageDimension>* itkImage, double percentage, double & lowerThreshold, double & upperThreshold) {
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
  if (UI.checkBoxIgnoreZeros->isChecked()) {
    totalPixels -= histogram->GetFrequency(0);
    histogram->SetFrequency(0, 0);
  }

  std::cout << "Total: " << totalPixels << " pixels." << std::endl;

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

// ------------ //
// ---- 2b ---- //
// ------------ //

/**
  * Creates a sphere and maps a texture created from uncertainty to it.
  */
void Sams_View::GenerateUncertaintySphere() {
  // Create a sphere.
  vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
  sphere->SetThetaResolution(100);
  sphere->SetPhiResolution(100);
  sphere->SetRadius(20.0);
  sphere->SetCenter(0, 0, 0);

  // Create a VTK texture map.
  vtkSmartPointer<vtkTextureMapToSphere> mapToSphere = vtkSmartPointer<vtkTextureMapToSphere>::New();
  mapToSphere->SetInputConnection(sphere->GetOutputPort());
  mapToSphere->PreventSeamOff();
  mapToSphere->Update();

  // Generate texture.
  mitk::Image::Pointer textureImage = GenerateUncertaintyTexture();

  // Export texture.
  mitk::DataNode::Pointer textureNode = mitk::DataNode::New();
  textureNode->SetData(textureImage);
  textureNode->SetProperty("name", mitk::StringProperty::New("Uncertainty Texture"));
  textureNode->SetProperty("layer", mitk::IntProperty::New(3));
  textureNode->SetProperty("color", mitk::ColorProperty::New(0.0, 1.0, 0.0));
  textureNode->SetProperty("opacity", mitk::FloatProperty::New(1.0));
  this->GetDataStorage()->Add(textureNode);

  // Create an MITK surface from the texture map.
  mitk::Surface::Pointer surfaceToPutTextureOn = mitk::Surface::New();
  surfaceToPutTextureOn->SetVtkPolyData(static_cast<vtkPolyData*>(mapToSphere->GetOutput()));

  // Optional: Manually set texture co-ordinates.
  // vtkSmartPointer<vtkFloatArray> textureCoords = vtkSmartPointer<vtkFloatArray>::New();
  // textureCoords->SetNumberOfComponents(2); 
  // textureCoords->SetNumberOfTuples(4); 
  // textureCoords->SetTuple2(0, 0.0, 0.0);
  // textureCoords->SetTuple2(1, 0.0, 1.0);
  // textureCoords->SetTuple2(2, 1.0, 0.0);
  // textureCoords->SetTuple2(3, 1.0, 1.0);

  // vtkPolyData * polyData = surfaceToPutTextureOn->GetVtkPolyData();
  // vtkPointData * pointData = polyData->GetPointData();
  // pointData->SetTCoords(textureCoords);

  // Create a datanode to store it.
  mitk::DataNode::Pointer surfaceNode = mitk::DataNode::New();
  surfaceNode->SetData(surfaceToPutTextureOn);
  surfaceNode->SetProperty("name", mitk::StringProperty::New("Uncertainty Sphere"));

  mitk::SmartPointerProperty::Pointer textureProperty = mitk::SmartPointerProperty::New(textureImage);
  surfaceNode->SetProperty("Surface.Texture", textureProperty);
  surfaceNode->SetProperty("layer", mitk::IntProperty::New(3));
  surfaceNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  surfaceNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));

  this->GetDataStorage()->Add(surfaceNode);
}

/**
  * Generates a texture that represents the uncertainty of the uncertainty volume.
  * It works by projecting a point in the center of the volume outwards, onto a sphere.
  */
mitk::Image::Pointer Sams_View::GenerateUncertaintyTexture() {
  unsigned int width = 150;
  unsigned int height = 50;

  // Create a blank ITK image.
  TextureImageType::RegionType region;
  TextureImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
 
  TextureImageType::SizeType size;
  size[0] = width;
  size[1] = height;
 
  region.SetSize(size);
  region.SetIndex(start);

  uncertaintyTexture = TextureImageType::New();
  uncertaintyTexture->SetRegions(region);
  uncertaintyTexture->Allocate();

  // Compute center of uncertainty data.
  vtkVector<float, 3> center = vtkVector<float, 3>();
  center[0] = ((float) uncertaintyHeight - 1) / 2.0;
  center[1] = ((float) uncertaintyWidth - 1) / 2.0;
  center[2] = ((float) uncertaintyDepth - 1) / 2.0;

  // For each pixel in the texture, sample the uncertainty data.
  for (unsigned int r = 0; r < height; r++) {
    for (unsigned int c = 0; c < width; c++) {
      // Compute spherical coordinates: phi (longitude) & theta (latitude).
      float theta = ((float) r / (float) height) * M_PI;
      float phi = ((float) c / (float) width) * (2 * M_PI);
      
      // Compute point on sphere with radius 1. This is also the vector from the center of the sphere to the point.
      vtkVector<float, 3> direction = vtkVector<float, 3>();
      direction[0] = cos(phi) * sin(theta);
      direction[1] = cos(theta);
      direction[2] = sin(phi) * sin(theta);
      direction.Normalize();

      // Sample the uncertainty data.
      int pixelValue = SampleUncertainty(center, direction);

      if (c == 75 && r == 25) {
        cout << "75, 25 has direction (" << direction[0] << ", " << direction[1] << ", " << direction[2] << ") and value " << pixelValue << endl;
      }

      // Set texture value.
      TextureImageType::IndexType pixelIndex;
      pixelIndex[0] = c;
      pixelIndex[1] = r;

      uncertaintyTexture->SetPixel(pixelIndex, pixelValue);
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(uncertaintyTexture);
  return mitkImage;
}

/**
  * Returns the average uncertainty along a vector in the uncertainty.
  * startPosition - the vector to begin tracing from
  * direction - the vector of the direction to trace in
  */
int Sams_View::SampleUncertainty(vtkVector<float, 3> startPosition, vtkVector<float, 3> direction) {
  bool print = ((std::abs(direction[0] - -1.0f) < 0.000001f) && ((std::abs(direction[1]) < 0.000001f) && (std::abs(direction[2]) < 0.000001f)));
  if (print)
    cout << "Found Troublesome Direction (" << direction[0] << ", " << direction[1] << ", " << direction[2] << ")" << endl;
  // Use an image accessor to read values from the uncertainty.
  try  {
    // See if the uncertainty data is available to be read.
    mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(preprocessedUncertainty->GetData());
    mitk::ImagePixelReadAccessor<unsigned char, 3> readAccess(uncertaintyImage);

    // Starting at 'startPosition' move in 'direction' in unit steps, taking samples.
    vtkVector<float, 3> position = vtkVector<float, 3>(startPosition);

    double uncertaintyAccumulator = 0.0;
    unsigned int numSamples = 0;
    while (0 <= position[0] && position[0] <= uncertaintyHeight - 1 &&
           0 <= position[1] && position[1] <= uncertaintyWidth - 1 &&
           0 <= position[2] && position[2] <= uncertaintyDepth - 1) {
      double interpolationTotalAccumulator = 0.0;
      double interpolationDistanceAccumulator = 0.0;

      // We're going to interpolate this point by looking at the 8 nearest neighbours. 
      int xSampleRange = (round(position[0]) < position[0])? 1 : -1;
      int ySampleRange = (round(position[1]) < position[1])? 1 : -1;
      int zSampleRange = (round(position[2]) < position[2])? 1 : -1;

      if (print) {
        cout << "- Range for Sample: " << xSampleRange << ", " << ySampleRange << ", " << zSampleRange << endl;
      }

      // Loop through the 8 samples.
      for (int i = std::min(xSampleRange, 0); i <= std::max(xSampleRange, 0); i++) {
        for (int j = std::min(ySampleRange, 0); j <= std::max(ySampleRange, 0); j++) { 
          for (int k = std::min(zSampleRange, 0); k <= std::max(zSampleRange, 0); k++) {
            if (print)
              cout << "-- i: " << i << ", j: " << j << ", k: " << k << endl;
            // Get the position of the neighbour.
            vtkVector<float, 3> neighbour = vtkVector<float, 3>();
            neighbour[0] = round(position[0] + i);
            neighbour[1] = round(position[1] + j);
            neighbour[2] = round(position[2] + k);

            // If the neighbour doesn't exist (we're over the edge), skip it.
            if (neighbour[0] < 0.0f || uncertaintyHeight <= neighbour[0] ||
                neighbour[1] < 0.0f || uncertaintyWidth <= neighbour[1] ||
                neighbour[2] < 0.0f || uncertaintyDepth <= neighbour[2]) {
              if (print)
                cout << "-- SKIPPED NEIGHBOUR" << endl;
              continue;
            }

            // Read the uncertainty of the neighbour.
            itk::Index<3> index;
            index[0] = neighbour[0];
            index[1] = neighbour[1];
            index[2] = neighbour[2];
            float neighbourUncertainty = readAccess.GetPixelByIndex(index);

            // If the uncertainty of the neighbour is 0, skip it.
            if (std::abs(neighbourUncertainty) < 0.0001) {
              if (print)
                cout << "-- SKIPPED NEIGHBOUR" << endl;
              continue;
            }

            // Get the distance to this neighbour
            vtkVector<float, 3> difference = vectorSubtract(position, neighbour);
            double distanceToSample = difference.Norm();

            // If the distance turns out to be zero, we have a perfect match. Ignore all other samples.
            if (std::abs(distanceToSample) < 0.0001) {
              interpolationTotalAccumulator = neighbourUncertainty;
              interpolationDistanceAccumulator = 1;
              if (print)
                cout << "-- Distance is zero. - (" << i << ", " << j << ", " << k << ") - " << interpolationTotalAccumulator << endl;
              goto BREAK_ALL_LOOPS;
            }

            if (print)
              cout << "-- Interpolation Sample: Uncertainty: " << neighbourUncertainty << ", Distance: " << distanceToSample << endl;
            // Accumulate
            interpolationTotalAccumulator += neighbourUncertainty / distanceToSample;
            interpolationDistanceAccumulator += 1.0 / distanceToSample;
          }
        }
      }
      BREAK_ALL_LOOPS:

      // Interpolate the values. If there were no valid samples, set it to zero.
      double interpolatedSample = (interpolationTotalAccumulator == 0.0) ? 0 : interpolationTotalAccumulator / interpolationDistanceAccumulator;

      if (print) 
        cout << "- Combined Sample: " << interpolatedSample << " (from total=" << interpolationTotalAccumulator << " and distance=" << interpolationDistanceAccumulator << ")" <<endl;

      // Include sample if it's not background.
      if (interpolatedSample != 0.0) {
        uncertaintyAccumulator += interpolatedSample;
        numSamples++;
      }

      // Move along.
      position = vectorAdd(position, direction);
    }
    if (print) 
      cout << "Returning " << round(uncertaintyAccumulator / numSamples) << " - acc = " << uncertaintyAccumulator << " - num = " << numSamples << endl; 

    return round(uncertaintyAccumulator / numSamples);
  }

  catch (mitk::Exception & e) {
    cerr << "Hmmm... it appears we can't get read access to the uncertainty image. Maybe it's gone? Maybe it's type isn't unsigned char? (I've assumed it is)" << e << endl;
    return -1;
  }
}

// ------------ //
// ---- 2c ---- //
// ------------ //

/**
  * Takes a surface and maps the uncertainty onto it based on the normal vector.
  */
void Sams_View::SurfaceMapping() {
  // Get the surface.
  mitk::DataNode::Pointer brainModelNode = this->GetDataStorage()->GetNode(mitk::NodePredicateDataType::New("Surface"));
  if (brainModelNode == (void*)NULL) {
    cout << "No Surface Found." << endl;
    return;
  }

  // Stop the specular in the rendering.
  brainModelNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  brainModelNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  brainModelNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
  brainModelNode->SetProperty("scalar visibility", mitk::BoolProperty::New(true));

  // Cast it to an MITK surface.
  mitk::BaseData * brainModelData = brainModelNode->GetData();
  mitk::Surface::Pointer brainModelSurface = dynamic_cast<mitk::Surface*>(brainModelData);

  // Extract the vtkPolyData.
  vtkPolyData * brainModelVtk = brainModelSurface->GetVtkPolyData();

  // From this we can the array containing all the normals.
  vtkSmartPointer<vtkFloatArray> normals = vtkFloatArray::SafeDownCast(brainModelVtk->GetPointData()->GetNormals());
  if (!normals) {
    cerr << "Couldn't seem to find any normals." << endl;
    return;
  }

  // Compute the bounding box of the surface (for simple registration between surface and uncertainty volume)
  double bounds[6];
  brainModelVtk->GetBounds(bounds); // NOTE: Apparently this isn't thread safe.
  double xMin = bounds[0];
  double xMax = bounds[1];
  double xRange = xMax - xMin;
  double yMin = bounds[2];
  double yMax = bounds[3];
  double yRange = yMax - yMin;
  double zMin = bounds[4];
  double zMax = bounds[5];
  double zRange = zMax - zMin;
  cout << "Ranges:" << endl;
  cout << "x: (" << xMin << ", " << xMax << ") - " << xRange << endl;
  cout << "y: (" << yMin << ", " << yMax << ") - " << yRange << endl;
  cout << "z: (" << zMin << ", " << zMax << ") - " << zRange << endl;

  // Generate a list of colours, one for each point, based on the normal at that point.
  vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
  colors->SetNumberOfComponents(3);
  colors->SetName ("Colors");  
  for (vtkIdType i = 0; i < brainModelVtk->GetNumberOfPoints(); i++) {
    // Get the position of point i
    double positionOfPoint[3];
    brainModelVtk->GetPoint(i, positionOfPoint);

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
    int intensity = SampleUncertainty(position, normal);

    cout << "Intensity from (" << position[0] << ", " << position[1] << ", " << position[2] << ") -> (" << normal[0] << ", " << normal[1] << ", " << normal[2] << ") is " << intensity <<   "." << endl;

    // Set the colour to be a grayscale version of this sampling.
    unsigned char normalColour[3] = {static_cast<unsigned char>(intensity), static_cast<unsigned char>(intensity), static_cast<unsigned char>(intensity)};
    colors->InsertNextTupleValue(normalColour);
  }
  
  // Set the colours to be the scalar value of each point.
  brainModelVtk->GetPointData()->SetScalars(colors);
  
  cout << "Well, that works." << endl;
}

void Sams_View::GenerateSphereSurface() {
  // Create a simple (VTK) sphere.
  vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
  sphere->SetThetaResolution(50);
  sphere->SetPhiResolution(150);
  sphere->SetRadius(20.0);
  sphere->SetCenter(0, 0, 0);
  sphere->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer sphereSurface = mitk::Surface::New();
  sphereSurface->SetVtkPolyData(static_cast<vtkPolyData*>(sphere->GetOutput()));

  // Store it as a DataNode.
  mitk::DataNode::Pointer sphereNode = mitk::DataNode::New();
  sphereNode->SetData(sphereSurface);
  sphereNode->SetProperty("name", mitk::StringProperty::New("Sphere Surface"));
  sphereNode->SetProperty("layer", mitk::IntProperty::New(3));
  sphereNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  sphereNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  sphereNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));

  this->GetDataStorage()->Add(sphereNode);
}

// ----------- //
// ---- 3 ---- //
// ----------- //

/**
  * If state > 0 then crosshairs are enabled. Otherwise they are disabled.
  */
void Sams_View::ToggleCrosshairs(int state) {
  mitk::ILinkedRenderWindowPart* linkedRenderWindowPart = dynamic_cast<mitk::ILinkedRenderWindowPart*>(this->GetRenderWindowPart());
  if (linkedRenderWindowPart != NULL) {
    linkedRenderWindowPart->EnableSlicingPlanes(state > 0);
  }
}

/**
  * Resets all the cameras. Works, but doesn't call reinit on all the datanodes (which it appears 'Reset Views' does...)
  */
void Sams_View::ResetViews() {
  // Get all DataNode's that are visible.
  mitk::NodePredicateProperty::Pointer isVisible = mitk::NodePredicateProperty::New("visible", mitk::BoolProperty::New(true));
  mitk::DataStorage::SetOfObjects::ConstPointer visibleObjects = this->GetDataStorage()->GetSubset(isVisible);

  // Compute bounding box for them.
  const mitk::TimeGeometry::Pointer bounds = this->GetDataStorage()->ComputeBoundingGeometry3D(visibleObjects);

  // OLD: Compute optimal bounds.
  //   const mitk::TimeGeometry::Pointer bounds = this->GetDataStorage()->ComputeVisibleBoundingGeometry3D();

  // Set them.
  mitk::IRenderWindowPart* renderWindowPart = this->GetRenderWindowPart();
  mitk::IRenderingManager* renderManager = renderWindowPart->GetRenderingManager();
  renderManager->InitializeViews(bounds);

  this->RequestRenderWindowUpdate();
}

// ----------- //
// ---- 4 ---- //
// ----------- //

/**
  * Sets the uncertainty to be rendered in front of the scan (for overlaying in 2D).
  */
void Sams_View::SetLayers() {
  mitk::IntProperty::Pointer behindProperty = mitk::IntProperty::New(0);
  mitk::IntProperty::Pointer infrontProperty = mitk::IntProperty::New(1);

  scan->SetProperty("layer", behindProperty);
  uncertainty->SetProperty("layer", infrontProperty);

  this->RequestRenderWindowUpdate();
}

/**
  * Puts some useless text on the render windows.
  */
void Sams_View::ShowTextOverlay() {
  mitk::ILinkedRenderWindowPart* renderWindowPart = dynamic_cast<mitk::ILinkedRenderWindowPart*>(this->GetRenderWindowPart());
  QmitkRenderWindow * renderWindow = renderWindowPart->GetActiveQmitkRenderWindow();
  mitk::BaseRenderer * renderer = mitk::BaseRenderer::GetInstance(renderWindow->GetVtkRenderWindow());
  mitk::OverlayManager::Pointer overlayManager = renderer->GetOverlayManager();

  //Create a textOverlay2D
  mitk::TextOverlay2D::Pointer textOverlay = mitk::TextOverlay2D::New();
  textOverlay->SetText("Test!"); //set UTF-8 encoded text to render
  textOverlay->SetFontSize(40);
  textOverlay->SetColor(1,0,0); //Set text color to red
  textOverlay->SetOpacity(1);

  //The position of the Overlay can be set to a fixed coordinate on the display.
  mitk::Point2D pos;
  pos[0] = 10,pos[1] = 20;
  textOverlay->SetPosition2D(pos);
  
  //Add the overlay to the overlayManager. It is added to all registered renderers automatically
  overlayManager->AddOverlay(textOverlay.GetPointer());

  this->RequestRenderWindowUpdate();
}

// ----------- //
// ---- 5 ---- //
// ----------- //

void Sams_View::GenerateRandomUncertainty() {
  GenerateRandomUncertainty(50);
}

void Sams_View::GenerateCubeUncertainty() {
  GenerateCubeUncertainty(50, 10);
}

void Sams_View::GenerateSphereUncertainty() {
  GenerateSphereUncertainty(50, 30);
}

void Sams_View::GenerateQuadrantSphereUncertainty() {
  GenerateSphereUncertainty(50, 12.5, vtkVector<float, 3>(12.5));
}

/**
  * Generates uncertainty data (size * size * size).
  * Each voxel is a random uncertainty value between 0 and 255.
  */
void Sams_View::GenerateRandomUncertainty(unsigned int size) {
  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = size;
  uncertaintySize[1] = size;
  uncertaintySize[2] = size;
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  randomUncertainty = UncertaintyImageType::New();
  randomUncertainty->SetRegions(region);
  randomUncertainty->Allocate();

  // Go through each voxel and set a random value.
  for (unsigned int r = 0; r < size; r++) {
    for (unsigned int c = 0; c < size; c++) {
      for (unsigned int d = 0; d < size; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        randomUncertainty->SetPixel(pixelIndex, rand() % 256);
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(randomUncertainty);
  
  mitk::DataNode::Pointer randomUncertaintyNode = mitk::DataNode::New();
  randomUncertaintyNode->SetData(mitkImage);
  randomUncertaintyNode->SetProperty("name", mitk::StringProperty::New("Random Uncertainty"));
  this->GetDataStorage()->Add(randomUncertaintyNode);
}

/**
  * Generates uncertainty data (totalSize * totalSize * totalSize).
  * It's zero everywhere, apart from a cube placed at the center with size cubeSize that is 1.
  */
void Sams_View::GenerateCubeUncertainty(unsigned int totalSize, unsigned int cubeSize) {
  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = totalSize;
  uncertaintySize[1] = totalSize;
  uncertaintySize[2] = totalSize;
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  cubeUncertainty = UncertaintyImageType::New();
  cubeUncertainty->SetRegions(region);
  cubeUncertainty->Allocate();

  // Make sure the cube is within the total size.
  if (cubeSize > totalSize) {
    cubeSize = totalSize;
  }

  // Compute the cube location. At first to float precision, then round to pixels.
  float halfCubeWidth = (float) cubeSize / 2.0;
  float uncertaintyCenter = (((float) (totalSize - 1)) / 2.0);
  unsigned int cubeStartPixel = ceil(uncertaintyCenter - halfCubeWidth);
  unsigned int cubeEndPixel = floor(uncertaintyCenter + halfCubeWidth);

  // Go through each voxel and set uncertainty according to whether it's in the cube.
  for (unsigned int r = 0; r < totalSize; r++) {
    for (unsigned int c = 0; c < totalSize; c++) {
      for (unsigned int d = 0; d < totalSize; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        // If this pixel is in the cube.
        if ((cubeStartPixel <= r) && (r <= cubeEndPixel) &&
            (cubeStartPixel <= c) && (c <= cubeEndPixel) &&
            (cubeStartPixel <= d) && (d <= cubeEndPixel)) {
          cubeUncertainty->SetPixel(pixelIndex, 255);
        }
        // If it's not.
        else {
          cubeUncertainty->SetPixel(pixelIndex, 0);
        }
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(cubeUncertainty);
  
  mitk::DataNode::Pointer cubeUncertaintyNode = mitk::DataNode::New();
  cubeUncertaintyNode->SetData(mitkImage);
  cubeUncertaintyNode->SetProperty("name", mitk::StringProperty::New("Cube of Uncertainty"));
  this->GetDataStorage()->Add(cubeUncertaintyNode);
}

/**
  * Generates uncertainty data (totalSize * totalSize * totalSize).
  * It's zero everywhere, apart from a sphere of radius sphereRadius that has uncertainty 255 at the center and fades linearly to the edges.
  */
void Sams_View::GenerateSphereUncertainty(unsigned int totalSize, unsigned int sphereRadius, vtkVector<float, 3> sphereCenter) {
// Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = totalSize;
  uncertaintySize[1] = totalSize;
  uncertaintySize[2] = totalSize;
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  sphereUncertainty = UncertaintyImageType::New();
  sphereUncertainty->SetRegions(region);
  sphereUncertainty->Allocate();

  // If the center is not specified (-1) in any dimension, make it the center.
  for (unsigned int i = 0; i < 3; i++) {
    if (sphereCenter[i] == -1.0f) {
      sphereCenter[i] = (float) (totalSize - 1) / 2.0;
    }
  }

  // Go through each voxel and weight uncertainty by distance from center of sphere.
  for (unsigned int r = 0; r < totalSize; r++) {
    for (unsigned int c = 0; c < totalSize; c++) {
      for (unsigned int d = 0; d < totalSize; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        vtkVector<float, 3> position = vtkVector<float, 3>();
        position[0] = r;
        position[1] = c;
        position[2] = d;

        // Compute distance from center.
        vtkVector<float, 3> difference = vectorSubtract(position, sphereCenter);
        float distanceFromCenter = difference.Norm();

        // Get normalized 0-1 weighting.
        float uncertaintyValue = std::max(0.0f, ((float) sphereRadius - distanceFromCenter) / (float) sphereRadius);

        // Scale by 255.
        sphereUncertainty->SetPixel(pixelIndex, uncertaintyValue * 255);
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(sphereUncertainty);
  
  mitk::DataNode::Pointer sphereUncertaintyNode = mitk::DataNode::New();
  sphereUncertaintyNode->SetData(mitkImage);
  sphereUncertaintyNode->SetProperty("name", mitk::StringProperty::New("Sphere of Uncertainty"));
  this->GetDataStorage()->Add(sphereUncertaintyNode);
}

/* -------------------- */
/* ---- DEPRECATED ---- */
/* -------------------- */

/**
  * Uses ITK to get the minimum and maximum values in the volume. Parameters min and max are set.
  */
template <typename TPixel, unsigned int VImageDimension>
void Sams_View::ItkGetRange(itk::Image<TPixel, VImageDimension>* itkImage, float &min, float &max) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::MinimumMaximumImageCalculator<ImageType> ImageCalculatorFilterType;
  
  typename ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New();
  imageCalculatorFilter->SetImage(itkImage);
  imageCalculatorFilter->Compute();

  min = imageCalculatorFilter->GetMinimum();
  max = imageCalculatorFilter->GetMaximum();
  cout << "(" << min << ", " << max << ")" << endl;

  this->RequestRenderWindowUpdate();
}