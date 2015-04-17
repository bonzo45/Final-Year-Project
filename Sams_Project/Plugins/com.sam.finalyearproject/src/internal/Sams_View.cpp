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
#include "UncertaintyPreprocessor.h"
#include "UncertaintyThresholder.h"
#include "UncertaintySampler.h"
#include "UncertaintyTexture.h"
#include "SurfaceGenerator.h"
#include "UncertaintySurfaceMapper.h"
#include "DemoUncertainty.h"

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

// 2
//  a. Thresholding

//  a. Volume Rendering

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

    std::ostringstream name;
    mitk::Image::Pointer generatedUncertainty;

    // If it's supposed to be random.
    if (QString::compare(uncertaintyName, RANDOM_NAME) == 0) {
      name << "Random Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      generatedUncertainty = DemoUncertainty::generateRandomUncertainty(scanSize);
    }
    // If it's supposed to be a sphere.
    else if (QString::compare(uncertaintyName, SPHERE_NAME) == 0) {
      name << "Sphere of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      float half = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 2;
      generatedUncertainty = DemoUncertainty::generateSphereUncertainty(scanSize, half);
    }
    // If it's supposed to be a cube.
    else if (QString::compare(uncertaintyName, CUBE_NAME) == 0) {
      name << "Cube of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      generatedUncertainty = DemoUncertainty::generateCubeUncertainty(scanSize, 10);
    }
    // If it's supposed to be a sphere in a quadrant.
    else if (QString::compare(uncertaintyName, QUAD_SPHERE_NAME) == 0) {
      name << "Quadsphere of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      float quarter = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 4;
      generatedUncertainty = DemoUncertainty::generateSphereUncertainty(scanSize, quarter, vtkVector<float, 3>(quarter));
    }
    // If it's not a demo uncertainty, stop.
    else {
      return;
    }

    uncertaintyNode = SaveDataNode(name.str().c_str(), generatedUncertainty);
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
  UncertaintyPreprocessor * preprocessor = new UncertaintyPreprocessor();
  preprocessor->setScan(Util::MitkImageFromNode(scan));
  preprocessor->setUncertainty(Util::MitkImageFromNode(node));
  preprocessor->setNormalizationParams(
    NORMALIZED_MIN,
    NORMALIZED_MAX
  );
  preprocessor->setErodeParams(
    UI.spinBoxErodeThickness->value(),
    UI.spinBoxErodeThreshold->value(),
    UI.spinBoxDilateThickness->value()
  );
  mitk::Image::Pointer fullyProcessedMitkImage = preprocessor->preprocessUncertainty(
    UI.checkBoxInversionEnabled->isChecked(),
    UI.checkBoxErosionEnabled->isChecked(),
    UI.checkBoxAligningEnabled->isChecked()
  );
  delete preprocessor;
  preprocessedUncertainty = SaveDataNode("Preprocessed", fullyProcessedMitkImage, true, node);
  preprocessedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));
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
  UncertaintyThresholder * thresholder = new UncertaintyThresholder();
  thresholder->setUncertainty(GetMitkPreprocessedUncertainty());
  thresholder->setIgnoreZeros(UI.checkBoxIgnoreZeros->isChecked());
  mitk::Image::Pointer thresholdedImage = thresholder->thresholdUncertainty(lowerThreshold, upperThreshold);
  delete thresholder;

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
  mitk::Image::Pointer uncertaintyImage = GetMitkPreprocessedUncertainty();

  UncertaintyThresholder * thresholder = new UncertaintyThresholder();
  thresholder->setUncertainty(GetMitkPreprocessedUncertainty());
  thresholder->setIgnoreZeros(UI.checkBoxIgnoreZeros->isChecked());
  double min, max;
  thresholder->getTopXPercentThreshold(percentage, min, max);
  delete thresholder;

  // Filter the uncertainty to only show the top 10%.
  SetLowerThreshold(min);
  SetUpperThreshold(max);

  // Update the spinBox.
  UI.spinBoxTopXPercent->setValue(percentage);
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
  // Create Sphere
  mitk::Surface::Pointer sphereToPutTextureOn = SurfaceGenerator::generateSphere(100, 20);
  
  // Create Texture
  UncertaintyTexture * texturerer = new UncertaintyTexture();
  texturerer->setUncertainty(GetMitkPreprocessedUncertainty());
  texturerer->setDimensions(UI.spinBoxTextureWidth->value(), UI.spinBoxTextureHeight->value());
  mitk::Image::Pointer texture = texturerer->generateUncertaintyTexture();
  delete texturerer;

  // Save texture.
  mitk::DataNode::Pointer textureNode = SaveDataNode("Uncertainty Texture", texture, true);
  textureNode->SetProperty("layer", mitk::IntProperty::New(3));
  textureNode->SetProperty("color", mitk::ColorProperty::New(0.0, 1.0, 0.0));
  textureNode->SetProperty("opacity", mitk::FloatProperty::New(1.0));

  // Save sphere.
  mitk::DataNode::Pointer surfaceNode = SaveDataNode("Uncertainty Sphere", sphereToPutTextureOn, true);
  mitk::SmartPointerProperty::Pointer textureProperty = mitk::SmartPointerProperty::New(texture);
  surfaceNode->SetProperty("Surface.Texture", textureProperty);
  surfaceNode->SetProperty("layer", mitk::IntProperty::New(3));
  surfaceNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  surfaceNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
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

  // Stop it being specular in the rendering.
  brainModelNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  brainModelNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  brainModelNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
  brainModelNode->SetProperty("scalar visibility", mitk::BoolProperty::New(true));

  // Cast it to an MITK surface.
  mitk::Surface::Pointer brainModelSurface = dynamic_cast<mitk::Surface*>(brainModelNode->GetData());

  // Map the uncertainty to it.
  UncertaintySurfaceMapper * mapper = new UncertaintySurfaceMapper();
  mapper->setUncertainty(GetMitkPreprocessedUncertainty());
  mapper->setSurface(brainModelSurface);
  // ---- Sampling Options ---- ///
  if (UI.radioButtonSamplingFull->isChecked()) {
    mapper->setSamplingFull();
  }
  else if (UI.radioButtonSamplingHalf->isChecked()) {
    mapper->setSamplingHalf();
  }
  // ---- Scaling Options ---- //
  if (UI.radioButtonScalingNone->isChecked()) {
    mapper->setScalingNone();
  }
  else if (UI.radioButtonScalingLinear->isChecked()) {
    mapper->setScalingLinear();
  }
  else if (UI.radioButtonScalingHistogram->isChecked()) {
    mapper->setScalingHistogram();
  }
  // ---- Colour Options ---- //
  if (UI.radioButtonColourBlackAndWhite->isChecked()) {
    mapper->setBlackAndWhite();
  }
  else if (UI.radioButtonColourColour->isChecked()) {
    mapper->setColour();
  }
  mapper->map();
  delete mapper;
  
  this->RequestRenderWindowUpdate();
}

void Sams_View::GenerateSphereSurface() {
  mitk::Surface::Pointer sphereSurface = SurfaceGenerator::generateSphere(100, 20);

  // Store it as a DataNode.
  mitk::DataNode::Pointer sphereNode = SaveDataNode("Sphere Surface", sphereSurface, true);
  sphereNode->SetProperty("layer", mitk::IntProperty::New(3));
  sphereNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  sphereNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  sphereNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
}

void Sams_View::GenerateCubeSurface() {
  mitk::Surface::Pointer cubeSurface = SurfaceGenerator::generateCube(20.0);

  // Store it as a DataNode.
  mitk::DataNode::Pointer cubeNode = SaveDataNode("Cube Surface", cubeSurface, true);
  cubeNode->SetProperty("layer", mitk::IntProperty::New(3));
  cubeNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  cubeNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  cubeNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
}

void Sams_View::GenerateCylinderSurface() {
  mitk::Surface::Pointer cylinderSurface = SurfaceGenerator::generateCylinder(20.0, 20.0, 10);

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

// --------------- //
// ---- Debug ---- //
// --------------- //

void Sams_View::ToggleDebug() {
  UI.widgetDebug->setVisible(!UI.widgetDebug->isVisible());
}

void Sams_View::GenerateRandomUncertainty() {
  vtkVector<float, 3> uncertaintySize = vtkVector<float, 3>(50);
  mitk::Image::Pointer random = DemoUncertainty::generateRandomUncertainty(uncertaintySize);
  SaveDataNode("Random Uncertainty", random);
}

void Sams_View::GenerateCubeUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  mitk::Image::Pointer cube = DemoUncertainty::generateCubeUncertainty(imageSize, 10);
  SaveDataNode("Cube Uncertainty", cube);
}

void Sams_View::GenerateSphereUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  mitk::Image::Pointer sphere = DemoUncertainty::generateSphereUncertainty(imageSize, std::min(std::min(imageSize[0], imageSize[1]), imageSize[2]) / 2);
  SaveDataNode("Sphere Uncertainty", sphere);
}

void Sams_View::GenerateQuadrantSphereUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  float quarter = std::min(std::min(imageSize[0], imageSize[1]), imageSize[2]) / 4;
  mitk::Image::Pointer quadsphere = DemoUncertainty::generateSphereUncertainty(imageSize, quarter, vtkVector<float, 3>(quarter));
  SaveDataNode("Quadsphere Uncertainty", quadsphere);
}