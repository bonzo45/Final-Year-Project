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

// Util
#include "Util.h"
#include "UncertaintyProcessor.h"

// General
#include <mitkBaseProperty.h>
#include <mitkIRenderWindowPart.h>
#include <mitkILinkedRenderWindowPart.h>
#include <mitkIRenderingManager.h>
#include <mitkNodePredicateProperty.h>
#include <mitkProgressBar.h>  // For loading bar.

#include <cmath> // for abs
#include <algorithm> // for min/max
#include <cstdlib>

// 1
#include <itkRescaleIntensityImageFilter.h>
#include <itkIntensityWindowingImageFilter.h>
#include <itkInvertIntensityImageFilter.h>
#include <itkChangeInformationImageFilter.h>
#include <itkMaskImageFilter.h>
#include <itkGrayscaleDilateImageFilter.h>
#include <mitkSlicedGeometry3D.h>
#include <mitkPoint.h>
#include <mitkNodePredicateBase.h>

// 2
//  a. Thresholding
#include <mitkImageAccessByItk.h>
#include <itkBinaryThresholdImageFilter.h>
#include <mitkImageCast.h>
#include <itkMinimumMaximumImageCalculator.h>
#include <itkImageToHistogramFilter.h>
#include <itkGrayscaleErodeImageFilter.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkGrayscaleErodeImageFilter.h>
#include <itkSubtractImageFilter.h>

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
#include <itkImportImageFilter.h>
#include <itkAdaptiveHistogramEqualizationImageFilter.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>

// 4
// - Text Overlay
#include <mitkBaseRenderer.h>
#include <QmitkRenderWindow.h>
#include <mitkOverlayManager.h>
#include <mitkTextOverlay2D.h>

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

const bool DEBUG_SAMPLING = false;

const QString RANDOM_NAME = QString::fromStdString("Random (Demo)");
const QString SPHERE_NAME = QString::fromStdString("Sphere (Demo)");
const QString CUBE_NAME = QString::fromStdString("Cube (Demo)");
const QString QUAD_SPHERE_NAME = QString::fromStdString("Sphere in Quadrant (Demo)");

// ------------------ //
// ---- TYPEDEFS ---- //
// ------------------ //
typedef itk::Image<unsigned char, 2>  TextureImageType;
typedef itk::Image<unsigned char, 3>  UncertaintyImageType;

// ------------------------- //
// ---- GLOBAL VARIABLES --- //
// ------------------------- //

bool selectionConfirmed = false;

// 1. Select Data & Uncertainty
//  - Scan
mitk::DataNode::Pointer scan;

unsigned int scanHeight;
unsigned int scanWidth;
unsigned int scanDepth;

//  - Uncertainty
mitk::DataNode::Pointer uncertainty;
mitk::DataNode::Pointer preprocessedUncertainty;

unsigned int uncertaintyHeight;
unsigned int uncertaintyWidth;
unsigned int uncertaintyDepth;

const double NORMALIZED_MAX = 1.0;
const double NORMALIZED_MIN = 0.0;

// 3. Visualisation
//  a. Uncertainty Thresholding
mitk::DataNode::Pointer thresholdedUncertainty = 0;
bool thresholdingEnabled = false;
double lowerThreshold = 0;
double upperThreshold = 0;

//  b. Uncertainty Sphere
TextureImageType::Pointer uncertaintyTexture = TextureImageType::New();

// Demo Uncertainties
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
  connect(UI.comboBoxScan, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(ScanDropdownChanged(const QString &)));
  connect(UI.comboBoxUncertainty, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(UncertaintyDropdownChanged(const QString &)));
  connect(UI.buttonConfirmSelection, SIGNAL(clicked()), this, SLOT(ConfirmSelection()));
  connect(UI.checkBoxScanVisible, SIGNAL(toggled(bool)), this, SLOT(ToggleScanVisible(bool)));
  connect(UI.checkBoxUncertaintyVisible, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyVisible(bool)));

  // 2. Preprocessing
  connect(UI.checkBoxErosionEnabled, SIGNAL(toggled(bool)), this, SLOT(ToggleErosionEnabled(bool)));

  // 3.
  //  a. Thresholding
  connect(UI.checkBoxEnableThreshold, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyThresholding(bool)));
  connect(UI.sliderMinThreshold, SIGNAL(sliderMoved (int)), this, SLOT(LowerThresholdChanged(int)));
  connect(UI.sliderMaxThreshold, SIGNAL(sliderMoved (int)), this, SLOT(UpperThresholdChanged(int)));
  connect(UI.spinBoxTopXPercent, SIGNAL(valueChanged(int)), this, SLOT(TopXPercent(int)));
  connect(UI.buttonTop1Percent, SIGNAL(clicked()), this, SLOT(TopOnePercent()));
  connect(UI.buttonTop5Percent, SIGNAL(clicked()), this, SLOT(TopFivePercent()));
  connect(UI.buttonTop10Percent, SIGNAL(clicked()), this, SLOT(TopTenPercent()));
  connect(UI.checkBoxIgnoreZeros, SIGNAL(stateChanged(int)), this, SLOT(ThresholdUncertainty()));
  connect(UI.buttonOverlayThreshold, SIGNAL(clicked()), this, SLOT(OverlayThreshold()));

  //  b. Texture Mapping
  connect(UI.spinBoxTextureWidth, SIGNAL(valueChanged(int)), this, SLOT(TextureWidthChanged(int)));
  connect(UI.spinBoxTextureHeight, SIGNAL(valueChanged(int)), this, SLOT(TextureHeightChanged(int)));
  connect(UI.buttonSphere, SIGNAL(clicked()), this, SLOT(GenerateUncertaintySphere()));

  //  c. Surface Mapping
  connect(UI.buttonSurfaceMapping, SIGNAL(clicked()), this, SLOT(SurfaceMapping()));

  // 4. Options
  connect(UI.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(UI.buttonResetViews, SIGNAL(clicked()), this, SLOT(ResetViews()));

  // UI
  connect(UI.buttonMinimize1, SIGNAL(clicked()), this, SLOT(ToggleMinimize1()));
  connect(UI.buttonMinimize2, SIGNAL(clicked()), this, SLOT(ToggleMinimize2()));
  connect(UI.buttonMinimize3, SIGNAL(clicked()), this, SLOT(ToggleMinimize3()));
  connect(UI.buttonMinimize4, SIGNAL(clicked()), this, SLOT(ToggleMinimize4()));

  // Debugging
  connect(UI.buttonToggleDebug, SIGNAL(clicked()), this, SLOT(ToggleDebug()));
  connect(UI.buttonDebugRandom, SIGNAL(clicked()), this, SLOT(GenerateRandomUncertainty()));
  connect(UI.buttonDebugCube, SIGNAL(clicked()), this, SLOT(GenerateCubeUncertainty()));
  connect(UI.buttonDebugSphere, SIGNAL(clicked()), this, SLOT(GenerateSphereUncertainty()));
  connect(UI.buttonDebugQuadSphere, SIGNAL(clicked()), this, SLOT(GenerateQuadrantSphereUncertainty()));

  InitializeUI();
}

// ------------ //
// ---- UI ---- //
// ------------ //

void Sams_View::InitializeUI() {
  // Hide Debug Widget.
  UI.widgetDebug->setVisible(false);

  // Initialize Drop-Down boxes.
  UpdateSelectionDropDowns();

  // Hide erode options boxes.
  UI.widgetErodeOptions->setVisible(false);

  // Disable visualisation
  UI.tab3a->setEnabled(false);
  UI.tab3b->setEnabled(false);
  UI.tab3c->setEnabled(false);
  selectionConfirmed = false;
}

void Sams_View::ToggleMinimize1() {
  UI.widget1Minimizable->setVisible(!UI.widget1Minimizable->isVisible());
}

void Sams_View::ToggleMinimize2() {
  UI.widget2Minimizable->setVisible(!UI.widget2Minimizable->isVisible());
}

void Sams_View::ToggleMinimize3() {
  UI.widget3Minimizable->setVisible(!UI.widget3Minimizable->isVisible());
}

void Sams_View::ToggleMinimize4() {
  UI.widget4Minimizable->setVisible(!UI.widget4Minimizable->isVisible());
}

void Sams_View::ToggleErosionEnabled(bool checked) {
  UI.widgetErodeOptions->setVisible(checked);
}

/**
  * What to do when the plugin window is selected.
  */
void Sams_View::SetFocus() {
  // Focus on something useful?
  //    e.g. UI.buttonOverlayText->setFocus();
}

// --------------- //
// ---- Utils ---- //
// --------------- //

mitk::Image::Pointer GetMitkScan() {
  return Util::MitkImageFromNode(scan);
}

mitk::Image::Pointer GetMitkUncertainty() {
  return Util::MitkImageFromNode(uncertainty);
}

mitk::Image::Pointer GetMitkPreprocessedUncertainty() {
  return Util::MitkImageFromNode(preprocessedUncertainty);
}

mitk::DataNode::Pointer Sams_View::SaveDataNode(const char * name, mitk::BaseData * data, bool overwrite, mitk::DataNode::Pointer parent) {
  // If overwrite is set to true, check for previous version and delete.
  if (overwrite) {
    mitk::DataNode::Pointer previousVersion;
    // If parent is set, look under the parent.
    if (parent.IsNull()) {
      previousVersion = this->GetDataStorage()->GetNamedNode(name);
    }
    else {
      previousVersion = this->GetDataStorage()->GetNamedDerivedNode(name, parent);
    }

    if (previousVersion) {
      this->GetDataStorage()->Remove(previousVersion);
    }
  }

  // Create a new version.
  mitk::DataNode::Pointer newVersion = mitk::DataNode::New();
  newVersion->SetProperty("name", mitk::StringProperty::New(name));
  newVersion->SetData(data);

  // Save it.
  if (parent.IsNull()) {
    this->GetDataStorage()->Add(newVersion);
  }
  else {
    this->GetDataStorage()->Add(newVersion, parent);
  }

  return newVersion;
}

// ----------- //
// ---- 1 ---- //
// ----------- //

/**
  * What to do when a data node or selection changes.
  */
void Sams_View::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList<mitk::DataNode::Pointer>& nodes) { 
  UpdateSelectionDropDowns();
}

void Sams_View::UpdateSelectionDropDowns() {
  // Remember our previous selection.
  QString scanName = UI.comboBoxScan->currentText();
  QString uncertaintyName = UI.comboBoxUncertainty->currentText();

  // Clear the dropdowns.
  UI.comboBoxScan->clear();
  UI.comboBoxUncertainty->clear();

  // Get all the potential images.
  mitk::TNodePredicateDataType<mitk::Image>::Pointer predicate(mitk::TNodePredicateDataType<mitk::Image>::New());
  mitk::DataStorage::SetOfObjects::ConstPointer allImages = this->GetDataStorage()->GetSubset(predicate);

  // Add them all to the dropdowns.
  mitk::DataStorage::SetOfObjects::ConstIterator image = allImages->Begin();
  while(image != allImages->End()) {
    QString name = QString::fromStdString(Util::StringFromStringProperty(image->Value()->GetProperty("name")));
    UI.comboBoxScan->addItem(name);
    UI.comboBoxUncertainty->addItem(name);
    ++image;
  }
  
  // Add the demo uncertainties to the uncertainty dropdown.
  UI.comboBoxUncertainty->addItem(RANDOM_NAME);
  UI.comboBoxUncertainty->addItem(SPHERE_NAME);
  UI.comboBoxUncertainty->addItem(CUBE_NAME);
  UI.comboBoxUncertainty->addItem(QUAD_SPHERE_NAME);

  // If our previous selections are still valid, select those again.
  int scanStillThere = UI.comboBoxScan->findText(scanName);
  if (scanStillThere != -1) {
    UI.comboBoxScan->setCurrentIndex(scanStillThere);
  }

  int uncertaintyStillThere = UI.comboBoxUncertainty->findText(uncertaintyName);
  if (uncertaintyStillThere != -1) {
    UI.comboBoxUncertainty->setCurrentIndex(uncertaintyStillThere);
  }
}

void Sams_View::ScanDropdownChanged(const QString & scanName) {
  scan = this->GetDataStorage()->GetNamedNode(scanName.toStdString());
  if (scan.IsNotNull()) {
    // Update the visibility checkbox to match the visibility of the scan we've picked.
    UI.checkBoxScanVisible->setChecked(Util::BoolFromBoolProperty(scan->GetProperty("visible")));
  }
}

void Sams_View::UncertaintyDropdownChanged(const QString & uncertaintyName) {
  uncertainty = this->GetDataStorage()->GetNamedNode(uncertaintyName.toStdString());
  if (uncertainty.IsNotNull()) {
    // Update the visibility checkbox to match the visibility of the uncertainty we've picked.
    UI.checkBoxUncertaintyVisible->setChecked(Util::BoolFromBoolProperty(uncertainty->GetProperty("visible")));
  }
}

/**
  * Once the scan and uncertainty have been picked and the user confirms they are correct, we do some pre-processing on the data.
  */
void Sams_View::ConfirmSelection() {
  // Get the DataNodes corresponding to the drop-down boxes.
  mitk::DataNode::Pointer scanNode = this->GetDataStorage()->GetNamedNode(UI.comboBoxScan->currentText().toStdString());
  mitk::DataNode::Pointer uncertaintyNode = this->GetDataStorage()->GetNamedNode(UI.comboBoxUncertainty->currentText().toStdString());
  
  // If the scan can't be found, stop.
  if (scanNode.IsNull()) {
    return;
  }

  // If the uncertainty can't be found, maybe it's a demo uncertainty. (we need to generate it)
  if (uncertaintyNode.IsNull()) {
    // Get it's name.
    QString uncertaintyName = UI.comboBoxUncertainty->currentText();
    vtkVector<float, 3> scanSize = vtkVector<float, 3>();
    mitk::Image::Pointer scanImage = Util::MitkImageFromNode(scanNode);
    scanSize[0] = scanImage->GetDimension(0);
    scanSize[1] = scanImage->GetDimension(1);
    scanSize[2] = scanImage->GetDimension(2);

    // If it's supposed to be random.
    if (QString::compare(uncertaintyName, RANDOM_NAME) == 0) {
      uncertaintyNode = GenerateRandomUncertainty(scanSize);
    }
    // If it's supposed to be a sphere.
    else if (QString::compare(uncertaintyName, SPHERE_NAME) == 0) {
      float half = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 2;
      uncertaintyNode = GenerateSphereUncertainty(scanSize, half);
    }
    // If it's supposed to be a cube.
    else if (QString::compare(uncertaintyName, CUBE_NAME) == 0) {
      uncertaintyNode = GenerateCubeUncertainty(scanSize, 10);
    }
    // If it's supposed to be a sphere in a quadrant.
    else if (QString::compare(uncertaintyName, QUAD_SPHERE_NAME) == 0) {
      float quarter = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 4;
      uncertaintyNode = GenerateSphereUncertainty(scanSize, quarter, vtkVector<float, 3>(quarter));
    }
    // If it's not a demo uncertainty, stop.
    else {
      return;
    }
  }

  // Save references to the nodes.
  scan = scanNode;
  uncertainty = uncertaintyNode;

  // Preprocess the uncertainty.
  PreprocessNode(uncertainty);

  // Save dimensions.
  mitk::Image::Pointer scanImage = GetMitkScan();
  scanHeight = scanImage->GetDimension(0);
  scanWidth = scanImage->GetDimension(1);
  scanDepth = scanImage->GetDimension(2);

  mitk::Image::Pointer uncertaintyImage = GetMitkUncertainty();
  uncertaintyHeight = uncertaintyImage->GetDimension(0);
  uncertaintyWidth = uncertaintyImage->GetDimension(1);
  uncertaintyDepth = uncertaintyImage->GetDimension(2);

  // 3a. Thresholding
  // Update min/max values.
  UI.labelSliderLeftLimit->setText(QString::fromStdString(Util::StringFromDouble(NORMALIZED_MIN)));
  UI.labelSliderRightLimit->setText(QString::fromStdString(Util::StringFromDouble(NORMALIZED_MAX)));

  // Move sliders to start/end.
  SetLowerThreshold(NORMALIZED_MIN);
  SetUpperThreshold(NORMALIZED_MAX);

  // Enable visualisations.
  UI.tab3a->setEnabled(true);
  UI.tab3b->setEnabled(true);
  UI.tab3c->setEnabled(true);

  // Flag that we're confirmed.
  selectionConfirmed = true;
}

/**
  * Normalizes and Inverts (if enabled) a node.
  * Saves intermediate steps and result as a child node of the one given.
  */
void Sams_View::PreprocessNode(mitk::DataNode::Pointer node) {
  UncertaintyProcessor * processor = new UncertaintyProcessor();
  processor->setScan(Util::MitkImageFromNode(scan));
  processor->setUncertainty(Util::MitkImageFromNode(node));
  processor->setNormalizationParams(
    NORMALIZED_MIN,
    NORMALIZED_MAX
  );
  processor->setErodeParams(
    UI.spinBoxErodeThickness->value(),
    UI.spinBoxErodeThreshold->value(),
    UI.spinBoxDilateThickness->value()
  );
  mitk::Image::Pointer fullyProcessedMitkImage = processor->PreprocessUncertainty(
    UI.checkBoxInversionEnabled->isChecked(),
    UI.checkBoxErosionEnabled->isChecked(),
    UI.checkBoxAligningEnabled->isChecked()
  );
  preprocessedUncertainty = SaveDataNode("Preprocessed", fullyProcessedMitkImage, true, node);
  preprocessedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));
  delete processor;
}

void Sams_View::ToggleScanVisible(bool checked) {
  if (checked) {
    scan->SetProperty("visible", mitk::BoolProperty::New(true));
  }
  else {
    scan->SetProperty("visible", mitk::BoolProperty::New(false));
  }
  
  this->RequestRenderWindowUpdate();  
}

void Sams_View::ToggleUncertaintyVisible(bool checked) {
  if (checked) {
    uncertainty->SetProperty("visible", mitk::BoolProperty::New(true));
  }
  else {
    uncertainty->SetProperty("visible", mitk::BoolProperty::New(false));
  }

  this->RequestRenderWindowUpdate();
}

// ------------ //
// ---- 3a ---- //
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
  thresholdedUncertainty = SaveDataNode("Thresholded", thresholdedImage, true, preprocessedUncertainty);
  thresholdedUncertainty->SetProperty("binary", mitk::BoolProperty::New(true));
  thresholdedUncertainty->SetProperty("color", mitk::ColorProperty::New(1.0, 0.0, 0.0));
  thresholdedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));
  thresholdedUncertainty->SetProperty("layer", mitk::IntProperty::New(1));
  thresholdedUncertainty->SetProperty("opacity", mitk::FloatProperty::New(0.5));

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
  float translatedLowerValue = ((NORMALIZED_MAX - NORMALIZED_MIN) / 1000) * lower + NORMALIZED_MIN;
  if (translatedLowerValue > upperThreshold) {
    UI.sliderMinThreshold->setValue(UI.sliderMaxThreshold->value());
    return;
  }
  lowerThreshold = translatedLowerValue;
  UI.labelSliderLeft->setText(QString::fromStdString(Util::StringFromDouble(lowerThreshold)));

  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
}

/**
  * Set Upper Threshold (called by sliders)
    * - upper is between 0 and 1000
  */
void Sams_View::UpperThresholdChanged(int upper) {
  float translatedUpperValue = ((NORMALIZED_MAX - NORMALIZED_MIN) / 1000) * upper + NORMALIZED_MIN;
  if (translatedUpperValue < lowerThreshold) {
    UI.sliderMaxThreshold->setValue(UI.sliderMinThreshold->value());
    return;
  }
  upperThreshold = translatedUpperValue;
  UI.labelSliderRight->setText(QString::fromStdString(Util::StringFromDouble(upperThreshold)));

  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
}

/**
  * Sets the lower threshold to be a float value (between 0.0 and 1.0)
  */
void Sams_View::SetLowerThreshold(double lower) {
  int sliderValue = round(((lower - NORMALIZED_MIN) / (NORMALIZED_MAX - NORMALIZED_MIN)) * 1000);
  sliderValue = std::min(sliderValue, 1000);
  sliderValue = std::max(sliderValue, 0);

  UI.sliderMinThreshold->setValue(sliderValue);
  LowerThresholdChanged(sliderValue);
}

/**
  * Sets the upper threshold to be a float value (between 0.0 and 1.0)
  */
void Sams_View::SetUpperThreshold(double upper) {
  int sliderValue = round(((upper - NORMALIZED_MIN) / (NORMALIZED_MAX - NORMALIZED_MIN)) * 1000);
  sliderValue = std::min(sliderValue, 1000);
  sliderValue = std::max(sliderValue, 0);

  UI.sliderMaxThreshold->setValue(sliderValue);
  UpperThresholdChanged(sliderValue);
}

void Sams_View::TopOnePercent() {
  TopXPercent(1);
}

void Sams_View::TopFivePercent() {
  TopXPercent(5);
}

void Sams_View::TopTenPercent() {
  TopXPercent(10);
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

  // Update the spinBox.
  UI.spinBoxTopXPercent->setValue(percentage);
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

/**
  * Sets the layers and visibility required for the thresholded uncertainty to be displayed on top of the scan.
  */
void Sams_View::OverlayThreshold() {
  // Make Scan and Thresholded Visible
  mitk::BoolProperty::Pointer propertyTrue = mitk::BoolProperty::New(true);
  
  // Scan -> Visible.
  if (scan) {
    scan->SetProperty("visible", propertyTrue);
  }

  // Thresholded Uncertainty -> Visible.
  if (thresholdedUncertainty) {
    thresholdedUncertainty->SetProperty("visible", propertyTrue);
  }

  // Put Scan behind Thresholded Uncertainty
  mitk::IntProperty::Pointer behindProperty = mitk::IntProperty::New(0);
  mitk::IntProperty::Pointer infrontProperty = mitk::IntProperty::New(1);
  if (scan) {
    scan->SetProperty("layer", behindProperty);
  }

  if (thresholdedUncertainty) {
    thresholdedUncertainty->SetProperty("layer", infrontProperty);  
  }
  
  // Everything else is invisible.
  mitk::BoolProperty::Pointer propertyFalse = mitk::BoolProperty::New(false);

  // Uncertainty -> Invisible.
  if (uncertainty) {
    uncertainty->SetProperty("visible", propertyFalse);
  }

  // Normalized -> Invisible.
  mitk::DataNode::Pointer normalizedNode = this->GetDataStorage()->GetNamedDerivedNode("Normalized", uncertainty);
  if (normalizedNode) {
    normalizedNode->SetProperty("visible", propertyFalse);
  }

  // Preprocessed -> Invisible.
  preprocessedUncertainty = this->GetDataStorage()->GetNamedDerivedNode("Preprocessed", uncertainty);
  if (preprocessedUncertainty) {
    preprocessedUncertainty->SetProperty("visible", propertyFalse);
  }

  this->RequestRenderWindowUpdate();
}

// ------------ //
// ---- 3b ---- //
// ------------ //

double latLongRatio = 2.0;

void Sams_View::TextureWidthChanged(int value) {
  if (UI.checkBoxLocked->isChecked()) {
    UI.spinBoxTextureHeight->setValue(value / latLongRatio);
  }

  latLongRatio = UI.spinBoxTextureWidth->value() / UI.spinBoxTextureHeight->value();
}

void Sams_View::TextureHeightChanged(int value) {
  if (UI.checkBoxLocked->isChecked()) {
    UI.spinBoxTextureWidth->setValue(value * latLongRatio);
  }

  latLongRatio = UI.spinBoxTextureWidth->value() / UI.spinBoxTextureHeight->value();
}

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
  mitk::DataNode::Pointer textureNode = SaveDataNode("Uncertainty Texture", textureImage, true);
  textureNode->SetProperty("layer", mitk::IntProperty::New(3));
  textureNode->SetProperty("color", mitk::ColorProperty::New(0.0, 1.0, 0.0));
  textureNode->SetProperty("opacity", mitk::FloatProperty::New(1.0));

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
  mitk::DataNode::Pointer surfaceNode = SaveDataNode("Uncertainty Sphere", surfaceToPutTextureOn, true);
  mitk::SmartPointerProperty::Pointer textureProperty = mitk::SmartPointerProperty::New(textureImage);
  surfaceNode->SetProperty("Surface.Texture", textureProperty);
  surfaceNode->SetProperty("layer", mitk::IntProperty::New(3));
  surfaceNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  surfaceNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
}

/**
  * Generates a texture that represents the uncertainty of the uncertainty volume.
  * It works by projecting a point in the center of the volume outwards, onto a sphere.
  */
mitk::Image::Pointer Sams_View::GenerateUncertaintyTexture() {
  unsigned int width = UI.spinBoxTextureWidth->value();
  unsigned int height = UI.spinBoxTextureHeight->value();

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
      int pixelValue = SampleUncertainty(center, direction) * 255;
      
      // Set texture value.
      TextureImageType::IndexType pixelIndex;
      pixelIndex[0] = c;
      pixelIndex[1] = r;

      uncertaintyTexture->SetPixel(pixelIndex, pixelValue);
    }
  }

  // Scale the texture values to increase contrast.
  if (UI.radioButtonTextureScalingLinear->isChecked()) {
    typedef itk::RescaleIntensityImageFilter<TextureImageType, TextureImageType> RescaleFilterType;
    typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(uncertaintyTexture);
    rescaleFilter->SetOutputMinimum(0);
    rescaleFilter->SetOutputMaximum(255);
    rescaleFilter->Update();
    uncertaintyTexture = rescaleFilter->GetOutput();
  }

  // Convert from ITK to MITK.
  return mitk::ImportItkImage(uncertaintyTexture);;
}

bool WithinBoundsOfUncertainty(vtkVector<float, 3> position) {
  return (0 <= position[0] && position[0] <= uncertaintyHeight - 1 &&
         0 <= position[1] && position[1] <= uncertaintyWidth - 1 &&
         0 <= position[2] && position[2] <= uncertaintyDepth - 1);
}

/**
  * Returns the average uncertainty along a vector in the uncertainty.
  * startPosition - the vector to begin tracing from
  * direction - the vector of the direction to trace in
  * percentage - how far to send the ray through the volume (0-100) (default 100)
  */
double Sams_View::SampleUncertainty(vtkVector<float, 3> startPosition, vtkVector<float, 3> direction, int percentage) {
  // Starting at 'startPosition' move in 'direction' in unit steps, taking samples.  
  // Similar to tortoise & hare algorithm. The tortoise moves slowly, collecting the samples we use
  // and the hare travels faster to see where the end of the uncertainty is.
  // (this allows us to stop a certain percentage of the way)
  vtkVector<float, 3> tortoise = vtkVector<float, 3>(startPosition);
  vtkVector<float, 3> hare = vtkVector<float, 3>(startPosition);
  // Hare travels faster.
  vtkVector<float, 3> hareDirection = Util::vectorScale(direction, 100.0f / percentage);

  if (DEBUG_SAMPLING) {
    cout << "Tortoise: (" << tortoise[0] << ", " << tortoise[1] << ", " << tortoise[2] << ")" << endl <<
            "Direction: (" << direction[0] << ", " << direction[1] << ", " << direction[2] << ")" << endl <<
            "Hare: (" << hare[0] << ", " << hare[1] << ", " << hare[2] << ")" << endl <<
            "Hare Direction: (" << hareDirection[0] << ", " << hareDirection[1] << ", " << hareDirection[2] << ")" << endl;  
  }

  // Move the tortoise and hare to the start of the uncertainty (i.e. not background)
  while (WithinBoundsOfUncertainty(tortoise)) {
    double sample = InterpolateUncertaintyAtPosition(tortoise);

    if (DEBUG_SAMPLING) {
      cout << " - Finding Uncertainty: (" << tortoise[0] << ", " << tortoise[1] << ", " << tortoise[2] << ")" <<
              " is " << sample << endl;
    }

    if (sample == 0.0) {
      tortoise = Util::vectorAdd(tortoise, direction);
    }
    else {
      break;
    }
  }
  hare = vtkVector<float, 3>(tortoise);

  // Move the tortoise and hare at different speeds. The tortoise gathers samples, but
  // stops when the hare reaches the edge of the uncertainty.
  double accumulator = 0.0;
  unsigned int sampleCount = 0;
  while (0 <= tortoise[0] && tortoise[0] <= uncertaintyHeight - 1 &&
         0 <= tortoise[1] && tortoise[1] <= uncertaintyWidth - 1 &&
         0 <= tortoise[2] && tortoise[2] <= uncertaintyDepth - 1) {
    double sample = InterpolateUncertaintyAtPosition(tortoise);

    // Include sample if it's not background.
    if (sample != 0.0) {
      accumulator += sample;
      sampleCount++;
    }

    if (DEBUG_SAMPLING) {
      cout << " - Sample at: (" << tortoise[0] << ", " << tortoise[1] << ", " << tortoise[2] << ")" <<
              " is " << sample << endl;
    }

    // Move along.
    tortoise = Util::vectorAdd(tortoise, direction);
    hare = Util::vectorAdd(hare, hareDirection);

    if (DEBUG_SAMPLING) {
      cout << " - Hare moves to: (" << hare[0] << ", " << hare[1] << ", " << hare[2] << ")" << endl;
    }

    // If the hare goes over the edge, stop.
    if (percentage != 100 && (!WithinBoundsOfUncertainty(hare) || InterpolateUncertaintyAtPosition(hare) == 0.0)) {
      if (DEBUG_SAMPLING) {
        cout << "- Hare over the edge." << endl;
      }
      break;
    }
  }

  double result = (accumulator / sampleCount);
  if (DEBUG_SAMPLING) {
    cout << "Result is " << result << " (" << accumulator << "/" << sampleCount << ")" << endl;
  }
  return result;
}

double Sams_View::InterpolateUncertaintyAtPosition(vtkVector<float, 3> position) {
  // Use an image accessor to read values from the uncertainty.
  try  {
    // See if the uncertainty data is available to be read.
    mitk::Image::Pointer uncertaintyImage = GetMitkPreprocessedUncertainty();
    mitk::ImagePixelReadAccessor<double, 3> readAccess(uncertaintyImage);

    double interpolationTotalAccumulator = 0.0;
    double interpolationDistanceAccumulator = 0.0;

    // We're going to interpolate this point by looking at the 8 nearest neighbours. 
    int xSampleRange = (round(position[0]) < position[0])? 1 : -1;
    int ySampleRange = (round(position[1]) < position[1])? 1 : -1;
    int zSampleRange = (round(position[2]) < position[2])? 1 : -1;

    // Loop through the 8 samples.
    for (int i = std::min(xSampleRange, 0); i <= std::max(xSampleRange, 0); i++) {
      for (int j = std::min(ySampleRange, 0); j <= std::max(ySampleRange, 0); j++) { 
        for (int k = std::min(zSampleRange, 0); k <= std::max(zSampleRange, 0); k++) {
          // Get the position of the neighbour.
          vtkVector<float, 3> neighbour = vtkVector<float, 3>();
          neighbour[0] = round(position[0] + i);
          neighbour[1] = round(position[1] + j);
          neighbour[2] = round(position[2] + k);

          // If the neighbour doesn't exist (we're over the edge), skip it.
          if (neighbour[0] < 0.0f || uncertaintyHeight <= neighbour[0] ||
              neighbour[1] < 0.0f || uncertaintyWidth <= neighbour[1] ||
              neighbour[2] < 0.0f || uncertaintyDepth <= neighbour[2]) {
            continue;
          }

          // Read the uncertainty of the neighbour.
          itk::Index<3> index;
          index[0] = neighbour[0];
          index[1] = neighbour[1];
          index[2] = neighbour[2];
          double neighbourUncertainty = readAccess.GetPixelByIndex(index);

          // If the uncertainty of the neighbour is 0, skip it.
          if (std::abs(neighbourUncertainty) < 0.0001) {
            continue;
          }

          // Get the distance to this neighbour
          vtkVector<float, 3> difference = Util::vectorSubtract(position, neighbour);
          double distanceToSample = difference.Norm();

          // If the distance turns out to be zero, we have a perfect match. Ignore all other samples.
          if (std::abs(distanceToSample) < 0.0001) {
            interpolationTotalAccumulator = neighbourUncertainty;
            interpolationDistanceAccumulator = 1;
            goto BREAK_ALL_LOOPS;
          }

          // Accumulate
          interpolationTotalAccumulator += neighbourUncertainty / distanceToSample;
          interpolationDistanceAccumulator += 1.0 / distanceToSample;
        }
      }
    }
    BREAK_ALL_LOOPS:

    // Interpolate the values. If there were no valid samples, set it to zero.
    return (interpolationTotalAccumulator == 0.0) ? 0 : interpolationTotalAccumulator / interpolationDistanceAccumulator;
  }

  catch (mitk::Exception & e) {
    cerr << "Hmmm... it appears we can't get read access to the uncertainty image. Maybe it's gone? Maybe it's type isn't double? (I've assumed it is)" << e << endl;
    return -1;
  }
}

// ------------ //
// ---- 3c ---- //
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
  if (DEBUG_SAMPLING) {
    cout << "Surface Bounds:" << endl;
    cout << xMin << "<= x <=" << xMax << " (" << xRange << ")" << endl;
    cout << yMin << "<= y <=" << yMax << " (" << yRange << ")" << endl;
    cout << zMin << "<= z <=" << zMax << " (" << zRange << ")" << endl;
    cout << "Uncertainty Size:" << endl;
    cout << "(" << uncertaintyHeight << ", " << uncertaintyWidth << ", " << uncertaintyDepth << ")" << endl;
  }

  // ----------------------------------------- //
  // ---- Compute Uncertainty Intensities ---- //
  // ----------------------------------------- //
  // Generate a list of intensities, one for each point.
  unsigned int numberOfPoints = brainModelVtk->GetNumberOfPoints();
  double * intensityArray;
  intensityArray  = new double[numberOfPoints];
  // Should probably call this at some point: delete [] myArrray;

  for (unsigned int i = 0; i < numberOfPoints; i++) {
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
    if (UI.radioButtonSamplingFull->isChecked()) {
      intensityArray[i] = SampleUncertainty(position, normal);
    }
    else if (UI.radioButtonSamplingHalf->isChecked()) {
      intensityArray[i] = SampleUncertainty(position, normal, 50);
    }
  }
  
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

  // -------------------------------- //
  // ---- Scale the Uncertanties ---- //
  // -------------------------------- //
  UncertaintyListType::Pointer scaled;
  // No scaling.
  if (UI.radioButtonScalingNone->isChecked()) {
    scaled = importFilter->GetOutput();
  }

  // Linear scaling. Map {min-max} to {0-1.}.
  else if (UI.radioButtonScalingLinear->isChecked()) {
    typedef itk::RescaleIntensityImageFilter<UncertaintyListType, UncertaintyListType> RescaleFilterType;
    typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
    rescaleFilter->SetInput(importFilter->GetOutput());
    rescaleFilter->SetOutputMinimum(0.0);
    rescaleFilter->SetOutputMaximum(1.0);
    rescaleFilter->Update();
    scaled = rescaleFilter->GetOutput();
  }

  // Histogram equalization.
  else if (UI.radioButtonScalingHistogram->isChecked()) {
    typedef itk::AdaptiveHistogramEqualizationImageFilter<UncertaintyListType> AdaptiveHistogramEqualizationImageFilterType;
    AdaptiveHistogramEqualizationImageFilterType::Pointer histogramEqualizationFilter = AdaptiveHistogramEqualizationImageFilterType::New();
    histogramEqualizationFilter->SetInput(importFilter->GetOutput());
    histogramEqualizationFilter->SetAlpha(UI.double1->value());
    histogramEqualizationFilter->SetBeta(UI.double2->value());
    histogramEqualizationFilter->SetRadius(UI.int1->value());
    histogramEqualizationFilter->Update();
    scaled = histogramEqualizationFilter->GetOutput();
  }

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
    if (UI.radioButtonColourBlackAndWhite->isChecked()) {
      normalColour[0] = intensity;
      normalColour[1] = intensity;
      normalColour[2] = intensity;
    }
    // Colour
    else if (UI.radioButtonColourColour->isChecked()) {
      normalColour[0] = 255 - intensity;
      normalColour[1] = intensity;
      normalColour[2] = 0;
    }
    colors->InsertNextTupleValue(normalColour);
  }
  
  // Set the colours to be the scalar value of each point.
  brainModelVtk->GetPointData()->SetScalars(colors);
  
  this->RequestRenderWindowUpdate();
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
  mitk::DataNode::Pointer sphereNode = SaveDataNode("Sphere Surface", sphereSurface, true);
  sphereNode->SetProperty("layer", mitk::IntProperty::New(3));
  sphereNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  sphereNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  sphereNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
}

void Sams_View::GenerateCubeSurface() {
  // Create a simple (VTK) cube.
  vtkSmartPointer<vtkCubeSource> cube = vtkSmartPointer<vtkCubeSource>::New();
  cube->SetXLength(20.0);
  cube->SetYLength(20.0);
  cube->SetZLength(20.0);
  cube->SetCenter(0.0, 0.0, 0.0);
  cube->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer cubeSurface = mitk::Surface::New();
  cubeSurface->SetVtkPolyData(static_cast<vtkPolyData*>(cube->GetOutput()));

  // Store it as a DataNode.
  mitk::DataNode::Pointer cubeNode = SaveDataNode("Cube Surface", cubeSurface, true);
  cubeNode->SetProperty("layer", mitk::IntProperty::New(3));
  cubeNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  cubeNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  cubeNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
}

void Sams_View::GenerateCylinderSurface() {
  // Create a simple (VTK) cylinder.
  vtkSmartPointer<vtkCylinderSource> cylinder = vtkSmartPointer<vtkCylinderSource>::New();
  cylinder->SetRadius(20.0);
  cylinder->SetHeight(20.0);
  cylinder->SetResolution(10);
  cylinder->SetCenter(0.0, 0.0, 0.0);
  cylinder->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer cylinderSurface = mitk::Surface::New();
  cylinderSurface->SetVtkPolyData(static_cast<vtkPolyData*>(cylinder->GetOutput()));

  // Store it as a DataNode.
  mitk::DataNode::Pointer cylinderNode = SaveDataNode("Cylinder Surface", cylinderSurface, true);
  cylinderNode->SetProperty("layer", mitk::IntProperty::New(3));
  cylinderNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  cylinderNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  cylinderNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
}

// ----------- //
// ---- 4 ---- //
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

  // Set them.
  mitk::IRenderWindowPart* renderWindowPart = this->GetRenderWindowPart();
  mitk::IRenderingManager* renderManager = renderWindowPart->GetRenderingManager();
  renderManager->InitializeViews(bounds);

  this->RequestRenderWindowUpdate();
}

// ----------- //
// ---- 5 ---- //
// ----------- //

/**
  * Generates uncertainty data (height * width * depth).
  * Each voxel is a random uncertainty value between 0 and 255.
  */
mitk::DataNode::Pointer Sams_View::GenerateRandomUncertainty(vtkVector<float, 3> imageSize) {
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

  randomUncertainty = UncertaintyImageType::New();
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
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(randomUncertainty);
  
  std::ostringstream ss;
  ss << "Random Uncertainty (" << imageSize[0] << "x" << imageSize[1] << "x" << imageSize[2] << ")";
  mitk::DataNode::Pointer randomUncertaintyNode = SaveDataNode(ss.str().c_str(), mitkImage);
  return randomUncertaintyNode;
}

/**
  * Generates uncertainty data (height * width * depth).
  * The cube, placed at the center with side length cubeSize, is totally uncertain (1) and everywhere else is completely certain (255).
  */
mitk::DataNode::Pointer Sams_View::GenerateCubeUncertainty(vtkVector<float, 3> imageSize, unsigned int cubeSize) {
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

  cubeUncertainty = UncertaintyImageType::New();
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
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(cubeUncertainty);
  
  std::ostringstream ss;
  ss << "Cube of Uncertainty (" << imageSize[0] << "x" << imageSize[1] << "x" << imageSize[2] << ")";
  mitk::DataNode::Pointer cubeUncertaintyNode = SaveDataNode(ss.str().c_str(), mitkImage);
  return cubeUncertaintyNode;
}

/**
  * Generates uncertainty data (imageSize[0] * imageSize[1] * imageSize[2]).
  * It's zero everywhere, apart from a sphere of radius sphereRadius that has uncertainty 255 at the center and fades linearly to the edges.
  */
mitk::DataNode::Pointer Sams_View::GenerateSphereUncertainty(vtkVector<float, 3> imageSize, unsigned int sphereRadius, vtkVector<float, 3> sphereCenter) {
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

  sphereUncertainty = UncertaintyImageType::New();
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
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(sphereUncertainty);
  
  std::ostringstream ss;
  ss << "Sphere of Uncertainty (" << imageSize[0] << "x" << imageSize[1] << "x" << imageSize[2] << ")";
  mitk::DataNode::Pointer sphereUncertaintyNode = SaveDataNode(ss.str().c_str(), mitkImage);
  return sphereUncertaintyNode;
}

// --------------- //
// ---- Debug ---- //
// --------------- //

void Sams_View::ToggleDebug() {
  UI.widgetDebug->setVisible(!UI.widgetDebug->isVisible());
}

void Sams_View::GenerateRandomUncertainty() {
  vtkVector<float, 3> uncertaintySize = vtkVector<float, 3>(50);
  GenerateRandomUncertainty(uncertaintySize);
}

void Sams_View::GenerateCubeUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  GenerateCubeUncertainty(imageSize, 10);
}

void Sams_View::GenerateSphereUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  GenerateSphereUncertainty(imageSize, std::min(std::min(imageSize[0], imageSize[1]), imageSize[2]) / 2);
}

void Sams_View::GenerateQuadrantSphereUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  float quarter = std::min(std::min(imageSize[0], imageSize[1]), imageSize[2]) / 4;
  GenerateSphereUncertainty(imageSize, quarter, vtkVector<float, 3>(quarter));
}