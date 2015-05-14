#include "Sams_View.h"

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qt
#include <QMessageBox>

// Sam's Helper Functions
#include "Util.h"
#include "UncertaintyPreprocessor.h"
#include "UncertaintyThresholder.h"
#include "UncertaintySampler.h"
#include "UncertaintyTexture.h"
#include "SurfaceGenerator.h"
#include "UncertaintySurfaceMapper.h"
#include "DemoUncertainty.h"
#include "RANSACScanPlaneGenerator.h"
#include "SVDScanPlaneGenerator.h"
#include "ColourLegendOverlay.h"
#include "ScanSimulator.h"

// MITK Interaction
#include <mitkIRenderWindowPart.h>
#include <mitkILinkedRenderWindowPart.h>
#include <mitkIRenderingManager.h>
#include <mitkNodePredicateProperty.h>
#include <mitkNodePredicateDataType.h>

// C Libraries
#include <algorithm> // for min/max
#include <cstdlib>

// 3
//  a. Thresholding
#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>
#include <mitkRenderingModeProperty.h>

//  b. Sphere Texture
#include <mitkSurface.h>
#include <mitkSmartPointerProperty.h>

// c. Surface Mapping
#include <vtkLookupTable.h>
#include <mitkLookupTableProperty.h>

// Debug
//  Overlays
#include <QmitkRenderWindow.h>
#include <mitkScaleLegendOverlay.h>

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

const QString RANDOM_NAME = QString::fromStdString("Random (Demo)");
const QString SPHERE_NAME = QString::fromStdString("Sphere (Demo)");
const QString CUBE_NAME = QString::fromStdString("Cube (Demo)");
const QString QUAD_SPHERE_NAME = QString::fromStdString("Sphere in Quadrant (Demo)");

const QString SPHERE_SURFACE_NAME = QString::fromStdString("Sphere");
const QString CUBE_SURFACE_NAME = QString::fromStdString("Cube");
const QString CYLINDER_SURFACE_NAME = QString::fromStdString("Cylinder");

const double NORMALIZED_MAX = 1.0;
const double NORMALIZED_MIN = 0.0;
// ------------------------- //
// ---- GLOBAL VARIABLES --- //
// ------------------------- //

// Legend
ColourLegendOverlay * legendOverlay = NULL;

// 3. Visualisation
//  a. Uncertainty Thresholding
UncertaintyThresholder * thresholder;
mitk::DataNode::Pointer thresholdedUncertainty = 0;
bool thresholdingEnabled = false;
bool thresholdingAutoUpdate = true;
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
  connect(UI.pushButtonEnableThreshold, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyThresholding(bool)));
  connect(UI.pushButtonEnableThresholdAutoUpdate, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyThresholdingAutoUpdate(bool)));
  connect(UI.sliderMinThreshold, SIGNAL(sliderMoved(int)), this, SLOT(LowerThresholdSliderMoved(int)));
  connect(UI.sliderMinThreshold, SIGNAL(valueChanged(int)), this, SLOT(LowerThresholdChanged(int)));
  connect(UI.sliderMaxThreshold, SIGNAL(sliderMoved(int)), this, SLOT(UpperThresholdSliderMoved(int)));
  connect(UI.sliderMaxThreshold, SIGNAL(valueChanged(int)), this, SLOT(UpperThresholdChanged(int)));
  connect(UI.spinBoxTopXPercent, SIGNAL(valueChanged(double)), this, SLOT(TopXPercent(double)));
  connect(UI.sliderTopXPercent, SIGNAL(sliderReleased()), this, SLOT(TopXPercent()));
  connect(UI.sliderTopXPercent, SIGNAL(sliderMoved(double)), this, SLOT(TopXPercentSliderMoved(double)));
  connect(UI.buttonTop1Percent, SIGNAL(clicked()), this, SLOT(TopOnePercent()));
  connect(UI.buttonTop5Percent, SIGNAL(clicked()), this, SLOT(TopFivePercent()));
  connect(UI.buttonTop10Percent, SIGNAL(clicked()), this, SLOT(TopTenPercent()));
  connect(UI.checkBoxIgnoreZeros, SIGNAL(stateChanged(int)), this, SLOT(ThresholdUncertaintyIfAutoUpdateEnabled()));
  // connect(UI.buttonOverlayThreshold, SIGNAL(clicked()), this, SLOT(OverlayThreshold()));
  connect(UI.buttonThresholdingReset, SIGNAL(clicked()), this, SLOT(ResetThresholds()));
  connect(UI.buttonVolumeRenderThreshold, SIGNAL(toggled(bool)), SLOT(VolumeRenderThreshold(bool)));

  //  b. Texture Mapping
  connect(UI.spinBoxSphereThetaResolution, SIGNAL(valueChanged(int)), this, SLOT(ThetaResolutionChanged(int)));
  connect(UI.spinBoxSpherePhiResolution, SIGNAL(valueChanged(int)), this, SLOT(PhiResolutionChanged(int)));
  connect(UI.buttonSphere, SIGNAL(clicked()), this, SLOT(GenerateUncertaintySphere()));

  //  c. Surface Mapping
  connect(UI.buttonSurfaceMapping, SIGNAL(clicked()), this, SLOT(SurfaceMapping()));

  //  d. Next Scan Plane
  connect(UI.buttonNextScanPlane, SIGNAL(clicked()), this, SLOT(ComputeNextScanPlane()));

  // 4. Options
  connect(UI.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(UI.checkBoxLegend, SIGNAL(stateChanged(int)), this, SLOT(ToggleLegend(int)));
  connect(UI.buttonResetViews, SIGNAL(clicked()), this, SLOT(ResetViews()));

  // UI
  connect(UI.buttonMinimize4, SIGNAL(clicked()), this, SLOT(ToggleMinimize4()));
  connect(UI.buttonReset2, SIGNAL(clicked()), this, SLOT(ResetPreprocessingSettings()));
  connect(UI.buttonVisualizeSelect, SIGNAL(clicked()), this, SLOT(ShowVisualizeSelect()));
  connect(UI.buttonVisualizeThreshold, SIGNAL(clicked()), this, SLOT(ShowVisualizeThreshold()));
  connect(UI.buttonVisualizeSphere, SIGNAL(clicked()), this, SLOT(ShowVisualizeSphere()));
  connect(UI.buttonVisualizeSurface, SIGNAL(clicked()), this, SLOT(ShowVisualizeSurface()));
  connect(UI.buttonVisualizeNextScanPlane, SIGNAL(clicked()), this, SLOT(ShowVisualizeNextScanPlane()));

  // Debugging
  connect(UI.buttonToggleDebug, SIGNAL(clicked()), this, SLOT(ToggleDebug()));
  connect(UI.buttonDebugRandom, SIGNAL(clicked()), this, SLOT(GenerateRandomUncertainty()));
  connect(UI.buttonDebugCube, SIGNAL(clicked()), this, SLOT(GenerateCubeUncertainty()));
  connect(UI.buttonDebugSphere, SIGNAL(clicked()), this, SLOT(GenerateSphereUncertainty()));
  connect(UI.buttonDebugQuadSphere, SIGNAL(clicked()), this, SLOT(GenerateQuadrantSphereUncertainty()));
  connect(UI.buttonDebug1, SIGNAL(clicked()), SLOT(HideAllDataNodes()));
  connect(UI.buttonDebug2, SIGNAL(clicked()), SLOT(BodgeSurface()));

  // RECONSTRUCTION
  connect(UI.buttonReconstructGUI, SIGNAL(clicked()), this, SLOT(ReconstructGUI()));
  connect(UI.buttonReconstructGo, SIGNAL(clicked()), this, SLOT(ReconstructGo()));

  // SCAN SIMULATION
  connect(UI.comboBoxSimulateScanVolume, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(ScanSimulationDropdownChanged(const QString &)));
  connect(UI.buttonScanPointCenter, SIGNAL(clicked()), this, SLOT(ScanSimulationSetPointCenter()));
  connect(UI.buttonDirectionAxial, SIGNAL(clicked()), this, SLOT(ScanSimulationSetDirectionAxial()));
  connect(UI.buttonDirectionCoronal, SIGNAL(clicked()), this, SLOT(ScanSimulationSetDirectionCoronal()));
  connect(UI.buttonDirectionSagittal, SIGNAL(clicked()), this, SLOT(ScanSimulationSetDirectionSagittal()));
  connect(UI.buttonSimulateScan, SIGNAL(clicked()), this, SLOT(ScanSimulationSimulateScan()));
  connect(UI.buttonScanPreview, SIGNAL(clicked()), this, SLOT(ScanSimulationPreview()));

  connect(UI.buttonScanPreview, SIGNAL(toggled(bool)), this, SLOT(ToggleScanSimulationPreview(bool)));
  connect(UI.spinBoxScanDimensionX, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanDimensionY, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanDimensionZ, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanPointX, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanPointY, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanPointZ, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanXAxisX, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanXAxisY, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanXAxisZ, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanYAxisX, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanYAxisY, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanYAxisZ, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanZAxisX, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanZAxisY, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));
  connect(UI.doubleSpinBoxScanZAxisZ, SIGNAL(valueChanged(double)), this, SLOT(ScanSimulationPreview()));

  InitializeUI();
  UI.buttonScanPreview->setChecked(false);
  scanPreviewEnabled = false;
}

// ------------ //
// ---- UI ---- //
// ------------ //

void Sams_View::InitializeUI() {
  // Hide Debug Widget.
  UI.widgetDebug->setVisible(false);

  // Hide Options Widget
  UI.widget4Minimizable->setVisible(false);

  // Initialize Drop-Down boxes.
  UpdateSelectionDropDowns();

  // Hide erode options boxes.
  UI.widgetVisualizeSelectErodeOptions->setVisible(false);

  // Disable visualisation
  UI.widgetVisualizeThreshold->setEnabled(false);
  UI.widgetVisualizeSphere->setEnabled(false);
  UI.widgetVisualizeSurface->setEnabled(false);
  UI.widgetVisualizeNextScanPlane->setEnabled(false);

  // ---- Reconstruction ---- //
  // Hide Reconstruction Error
  UI.labelReconstructExecutableError->setVisible(false);
  UI.labelReconstructExecutableSuccess->setVisible(false);

  ShowVisualizeSelect();
}

void Sams_View::ShowVisualizeSelect() {
  HideVisualizeAll();
  UI.widgetVisualizeSelect->setVisible(true);
  UI.buttonVisualizeSelect->setChecked(true);
}

void Sams_View::ShowVisualizeThreshold() {
  HideVisualizeAll();
  UI.widgetVisualizeThreshold->setVisible(true);
  UI.buttonVisualizeThreshold->setChecked(true);
}

void Sams_View::ShowVisualizeSphere() {
  HideVisualizeAll();
  UI.widgetVisualizeSphere->setVisible(true); 
  UI.buttonVisualizeSphere->setChecked(true);  
}

void Sams_View::ShowVisualizeSurface() {
  HideVisualizeAll();
  UI.widgetVisualizeSurface->setVisible(true); 
  UI.buttonVisualizeSurface->setChecked(true);  
}

void Sams_View::ShowVisualizeNextScanPlane() {
  HideVisualizeAll();
  UI.widgetVisualizeNextScanPlane->setVisible(true); 
  UI.buttonVisualizeNextScanPlane->setChecked(true);  
}

void Sams_View::HideVisualizeAll() {
  UI.widgetVisualizeSelect->setVisible(false);
  UI.widgetVisualizeThreshold->setVisible(false);
  UI.widgetVisualizeSphere->setVisible(false);
  UI.widgetVisualizeSurface->setVisible(false);
  UI.widgetVisualizeNextScanPlane->setVisible(false);
  UI.buttonVisualizeSelect->setChecked(false);
  UI.buttonVisualizeThreshold->setChecked(false);
  UI.buttonVisualizeSphere->setChecked(false);
  UI.buttonVisualizeSurface->setChecked(false);
  UI.buttonVisualizeNextScanPlane->setChecked(false);
}

void Sams_View::ToggleMinimize4() {
  UI.widget4Minimizable->setVisible(!UI.widget4Minimizable->isVisible());
}

void Sams_View::ResetPreprocessingSettings() {
  UI.checkBoxAligningEnabled->setChecked(false);
  UI.checkBoxInversionEnabled->setChecked(false);
  UI.checkBoxErosionEnabled->setChecked(false);
  UI.spinBoxErodeThickness->setValue(2);
  UI.spinBoxErodeThreshold->setValue(0.2);
  UI.spinBoxDilateThickness->setValue(2);
}

void Sams_View::ToggleErosionEnabled(bool checked) {
  UI.widgetVisualizeSelectErodeOptions->setVisible(checked);
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

mitk::Image::Pointer Sams_View::GetMitkScan() {
  return Util::MitkImageFromNode(this->scan);
}

mitk::Image::Pointer Sams_View::GetMitkUncertainty() {
  return Util::MitkImageFromNode(uncertainty);
}

mitk::Image::Pointer Sams_View::GetMitkPreprocessedUncertainty() {
  return Util::MitkImageFromNode(preprocessedUncertainty);
}

mitk::Image::Pointer Sams_View::GetMitkScanVolume() {
  return Util::MitkImageFromNode(scanSimulationVolume);
}

mitk::DataNode::Pointer Sams_View::SaveDataNode(const char * name, mitk::BaseData * data, bool overwrite, mitk::DataNode::Pointer parent) {
  // If overwrite is set to true, check for previous version and delete.
  if (overwrite) {
    RemoveDataNode(name, parent);
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

void Sams_View::RemoveDataNode(const char * name, mitk::DataNode::Pointer parent) {
  mitk::DataNode::Pointer nodeToDelete;
  // If parent is set, look under the parent.
  if (parent.IsNull()) {
    nodeToDelete = this->GetDataStorage()->GetNamedNode(name);
  }
  else {
    nodeToDelete = this->GetDataStorage()->GetNamedDerivedNode(name, parent);
  }

  if (nodeToDelete) {
    this->GetDataStorage()->Remove(nodeToDelete);
  }
}

void Sams_View::HideAllDataNodes() {
  // Get all the images.
  mitk::DataStorage::SetOfObjects::ConstPointer allImages = this->GetDataStorage()->GetAll();

  // Add property "visible = false"
  mitk::DataStorage::SetOfObjects::ConstIterator image = allImages->Begin();
  while(image != allImages->End()) {
    HideDataNode(image->Value());
    ++image;
  }

  this->RequestRenderWindowUpdate();
}

void Sams_View::SetDataNodeLayer(mitk::DataNode::Pointer node, int layer) {
  mitk::IntProperty::Pointer layerProperty = mitk::IntProperty::New(layer);
  if (node) {
    node->SetProperty("layer", layerProperty);
  }
}

void Sams_View::ShowDataNode(mitk::DataNode::Pointer node) {
  if (node) {
    node->SetProperty("visible", mitk::BoolProperty::New(true));
  }
}

void Sams_View::HideDataNode(mitk::DataNode::Pointer node) {
  if (node) {
    node->SetProperty("visible", mitk::BoolProperty::New(false));
  }
}

mitk::OverlayManager::Pointer Sams_View::GetOverlayManager() {
  mitk::ILinkedRenderWindowPart* renderWindowPart = dynamic_cast<mitk::ILinkedRenderWindowPart*>(this->GetRenderWindowPart());
  QmitkRenderWindow * renderWindow = renderWindowPart->GetActiveQmitkRenderWindow();
  mitk::BaseRenderer * renderer = mitk::BaseRenderer::GetInstance(renderWindow->GetVtkRenderWindow());
  return renderer->GetOverlayManager();
}

// ---------------- //
// ---- Legend ---- //
// ---------------- //

void Sams_View::SetLegend(double value1, char * colour1, double value2, char * colour2) {
  cout << "Updating Legend: " << endl;
  cout << "(" << value1 << ", [" << (int) colour1[0] << ", " << (int) colour1[1] << ", " << (int) colour1[2] << "])" << endl;
  cout << "(" << value2 << ", [" << (int) colour2[0] << ", " << (int) colour2[1] << ", " << (int) colour2[2] << "])" << endl;
  
  HideLegend();

  if (legendOverlay != NULL) {
    delete legendOverlay;
  }

  // Create an overlay for the legend.
  legendOverlay = new ColourLegendOverlay();
  legendOverlay->setValue1(value1);
  legendOverlay->setColour1(colour1[0], colour1[1], colour1[2]);
  legendOverlay->setValue2(value2);
  legendOverlay->setColour2(colour2[0], colour2[1], colour2[2]);

  ShowLegend();
}

void Sams_View::ShowLegend() {
  mitk::OverlayManager::Pointer overlayManager = GetOverlayManager();
  overlayManager->AddOverlay(legendOverlay);
  UI.checkBoxLegend->setChecked(true);
  this->RequestRenderWindowUpdate();
}

void Sams_View::HideLegend() {
  mitk::OverlayManager::Pointer overlayManager = GetOverlayManager();
  overlayManager->RemoveOverlay(legendOverlay);
  UI.checkBoxLegend->setChecked(false);
  this->RequestRenderWindowUpdate();
}

// ----------- //
// ---- 1 ---- //
// ----------- //

/**
  * What to do when a data node or selection changes.
  */
void Sams_View::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList<mitk::DataNode::Pointer>& /*nodes*/) { 
  UpdateSelectionDropDowns();
}

void Sams_View::UpdateSelectionDropDowns() {
  // Remember our previous selection.
  QString scanName = UI.comboBoxScan->currentText();
  QString uncertaintyName = UI.comboBoxUncertainty->currentText();
  QString simulatedScanName = UI.comboBoxSimulateScanVolume->currentText();
  QString surfaceName = UI.comboBoxSurface->currentText();

  // Clear the dropdowns.
  UI.comboBoxScan->clear();
  UI.comboBoxUncertainty->clear();
  UI.comboBoxSimulateScanVolume->clear();
  UI.comboBoxSurface->clear();

  // Get all the potential images.
  mitk::TNodePredicateDataType<mitk::Image>::Pointer imagePredicate(mitk::TNodePredicateDataType<mitk::Image>::New());
  mitk::DataStorage::SetOfObjects::ConstPointer allImages = this->GetDataStorage()->GetSubset(imagePredicate);

  // Add them all to the dropdowns.
  mitk::DataStorage::SetOfObjects::ConstIterator image = allImages->Begin();
  while(image != allImages->End()) {
    QString name = QString::fromStdString(Util::StringFromStringProperty(image->Value()->GetProperty("name")));
    UI.comboBoxScan->addItem(name);
    UI.comboBoxUncertainty->addItem(name);
    UI.comboBoxSimulateScanVolume->addItem(name);
    ++image;
  }

  // Add the demo uncertainties to the uncertainty dropdown.
  UI.comboBoxUncertainty->addItem(RANDOM_NAME);
  UI.comboBoxUncertainty->addItem(SPHERE_NAME);
  UI.comboBoxUncertainty->addItem(CUBE_NAME);
  UI.comboBoxUncertainty->addItem(QUAD_SPHERE_NAME);

  // Get all the potential surfaces.
  mitk::TNodePredicateDataType<mitk::Surface>::Pointer surfacePredicate(mitk::TNodePredicateDataType<mitk::Surface>::New());
  mitk::DataStorage::SetOfObjects::ConstPointer allSurfaces = this->GetDataStorage()->GetSubset(surfacePredicate);

  // Add them all to the surface dropdown.
  mitk::DataStorage::SetOfObjects::ConstIterator surface = allSurfaces->Begin();
  while(surface != allSurfaces->End()) {
    QString name = QString::fromStdString(Util::StringFromStringProperty(surface->Value()->GetProperty("name")));
    UI.comboBoxSurface->addItem(name);
    ++surface;
  }

  // If our previous selections are still valid, select those again.
  int scanStillThere = UI.comboBoxScan->findText(scanName);
  if (scanStillThere != -1) {
    UI.comboBoxScan->setCurrentIndex(scanStillThere);
  }

  int uncertaintyStillThere = UI.comboBoxUncertainty->findText(uncertaintyName);
  if (uncertaintyStillThere != -1) {
    UI.comboBoxUncertainty->setCurrentIndex(uncertaintyStillThere);
  }

  int simulatedScanStillThere = UI.comboBoxSimulateScanVolume->findText(simulatedScanName);
  if (simulatedScanStillThere != -1) {
    UI.comboBoxSimulateScanVolume->setCurrentIndex(simulatedScanStillThere);
  }

  int surfaceStillThere = UI.comboBoxSurface->findText(surfaceName);
  if (surfaceStillThere != -1) {
    UI.comboBoxSurface->setCurrentIndex(surfaceStillThere);
  }
}

void Sams_View::ScanDropdownChanged(const QString & scanName) {
  mitk::DataNode::Pointer potentialScan = this->GetDataStorage()->GetNamedNode(scanName.toStdString());
  if (potentialScan.IsNotNull()) {
    // Update the visibility checkbox to match the visibility of the scan we've picked.
    UI.checkBoxScanVisible->setChecked(Util::BoolFromBoolProperty(potentialScan->GetProperty("visible")));
  }
  this->scan = potentialScan;
}

void Sams_View::UncertaintyDropdownChanged(const QString & uncertaintyName) {
  mitk::DataNode::Pointer potentialUncertainty = this->GetDataStorage()->GetNamedNode(uncertaintyName.toStdString());
  if (potentialUncertainty.IsNotNull()) {
    // Update the visibility checkbox to match the visibility of the uncertainty we've picked.
    UI.checkBoxUncertaintyVisible->setChecked(Util::BoolFromBoolProperty(potentialUncertainty->GetProperty("visible")));
  }
  this->uncertainty = potentialUncertainty;
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
      name << RANDOM_NAME.toStdString(); // "Random Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      generatedUncertainty = DemoUncertainty::generateRandomUncertainty(scanSize);
    }
    // If it's supposed to be a sphere.
    else if (QString::compare(uncertaintyName, SPHERE_NAME) == 0) {
      name << SPHERE_NAME.toStdString(); // "Sphere of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      float half = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 2;
      generatedUncertainty = DemoUncertainty::generateSphereUncertainty(scanSize, half);
    }
    // If it's supposed to be a cube.
    else if (QString::compare(uncertaintyName, CUBE_NAME) == 0) {
      name << CUBE_NAME.toStdString(); // "Cube of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      generatedUncertainty = DemoUncertainty::generateCubeUncertainty(scanSize, 10);
    }
    // If it's supposed to be a sphere in a quadrant.
    else if (QString::compare(uncertaintyName, QUAD_SPHERE_NAME) == 0) {
      name << QUAD_SPHERE_NAME.toStdString(); // "Quadsphere of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
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
  this->scan = scanNode;
  this->uncertainty = uncertaintyNode;

  // Preprocess the uncertainty.
  PreprocessNode(this->uncertainty);

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
  UI.widgetVisualizeThreshold->setEnabled(true);
  UI.widgetVisualizeSphere->setEnabled(true);
  UI.widgetVisualizeSurface->setEnabled(true);
  UI.widgetVisualizeNextScanPlane->setEnabled(true);
}

/**
  * Normalizes and Inverts (if enabled) a node.
  * Saves intermediate steps and result as a child node of the one given.
  */
void Sams_View::PreprocessNode(mitk::DataNode::Pointer node) {
  UncertaintyPreprocessor * preprocessor = new UncertaintyPreprocessor();
  preprocessor->setScan(GetMitkScan());
  preprocessor->setUncertainty(GetMitkUncertainty());
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
}

void Sams_View::ToggleScanVisible(bool checked) {
  if (this->scan.IsNull()) {
    return;
  }

  if (checked) {
    this->scan->SetProperty("visible", mitk::BoolProperty::New(true));
  }
  else {
    this->scan->SetProperty("visible", mitk::BoolProperty::New(false));
  }
  
  this->RequestRenderWindowUpdate();
}

void Sams_View::ToggleUncertaintyVisible(bool checked) {
  if (this->uncertainty.IsNull()) {
    return;
  }

  if (checked) {
    this->uncertainty->SetProperty("visible", mitk::BoolProperty::New(true));
  }
  else {
    this->uncertainty->SetProperty("visible", mitk::BoolProperty::New(false));
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
    UI.thresholdingEnabledIndicator1->setChecked(true);
    UI.thresholdingEnabledIndicator2->setChecked(true);
    thresholdingEnabled = true;
    thresholder = new UncertaintyThresholder();
    ThresholdUncertainty();
  }
  else {
    UI.thresholdingEnabledIndicator1->setChecked(false);
    UI.thresholdingEnabledIndicator2->setChecked(false);
    thresholdingEnabled = false;
    if (thresholder) {
      delete thresholder;
      thresholder = NULL;
    }
    RemoveThresholdedUncertainty();
  }
}

void Sams_View::ToggleUncertaintyThresholdingAutoUpdate(bool checked) {
  thresholdingAutoUpdate = checked;
  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
}

void Sams_View::ThresholdUncertaintyIfAutoUpdateEnabled() {
  if (thresholdingAutoUpdate) {
    ThresholdUncertainty();
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
  thresholdedUncertainty->SetProperty("layer", mitk::IntProperty::New(10));
  thresholdedUncertainty->SetProperty("opacity", mitk::FloatProperty::New(0.5));

  DisplayThreshold();
}

void Sams_View::RemoveThresholdedUncertainty() {
  RemoveDataNode("Thresholded", preprocessedUncertainty);
}

void Sams_View::LowerThresholdSliderMoved(int lower) {
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  LowerThresholdChanged(lower);
  thresholdingEnabled = wasEnabled;
}

void Sams_View::LowerThresholdChanged() {
  LowerThresholdChanged(UI.sliderMinThreshold->value());
}

/**
  * Set Lower Threshold (called by sliders)
  * - lower is between 0 and 1000
  */
void Sams_View::LowerThresholdChanged(int lower) {
  float translatedLowerValue = ((NORMALIZED_MAX - NORMALIZED_MIN) / 1000) * lower + NORMALIZED_MIN;
  if (translatedLowerValue > upperThreshold) {
    std::cout << "Lower (" << translatedLowerValue << ") higher than Upper (" << upperThreshold << ") - setting to (" << UI.sliderMaxThreshold->value() << ")" << std::endl;
    UI.sliderMinThreshold->setValue(UI.sliderMaxThreshold->value());
    return;
  }
  lowerThreshold = translatedLowerValue;
  UI.labelSliderLeft->setText(QString::fromStdString(Util::StringFromDouble(lowerThreshold)));

  if (thresholdingEnabled) {
    ThresholdUncertaintyIfAutoUpdateEnabled();
  }
}

void Sams_View::UpperThresholdSliderMoved(int upper) {
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UpperThresholdChanged(upper);
  thresholdingEnabled = wasEnabled;
}

void Sams_View::UpperThresholdChanged() {
  UpperThresholdChanged(UI.sliderMaxThreshold->value());
}

/**
  * Set Upper Threshold (called by sliders)
    * - upper is between 0 and 1000
  */
void Sams_View::UpperThresholdChanged(int upper) {
  float translatedUpperValue = ((NORMALIZED_MAX - NORMALIZED_MIN) / 1000) * upper + NORMALIZED_MIN;
  if (translatedUpperValue < lowerThreshold) {
    std::cout << "Upper (" << translatedUpperValue << ") higher than Lower (" << lowerThreshold << ") - setting to (" << UI.sliderMinThreshold->value() << ")" << std::endl;
    UI.sliderMaxThreshold->setValue(UI.sliderMinThreshold->value());
    return;
  }
  upperThreshold = translatedUpperValue;
  UI.labelSliderRight->setText(QString::fromStdString(Util::StringFromDouble(upperThreshold)));

  if (thresholdingEnabled) {
    ThresholdUncertaintyIfAutoUpdateEnabled();
  }
}

/**
  * Sets the lower threshold to be a float value (between 0.0 and 1.0)
  */
void Sams_View::SetLowerThreshold(double lower) {
  int sliderValue = round(((lower - NORMALIZED_MIN) / (NORMALIZED_MAX - NORMALIZED_MIN)) * 1000);
  sliderValue = std::min(sliderValue, 1000);
  sliderValue = std::max(sliderValue, 0);

  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UI.sliderMinThreshold->setValue(sliderValue);
  thresholdingEnabled = wasEnabled;
  LowerThresholdChanged(sliderValue);
}

/**
  * Sets the upper threshold to be a float value (between 0.0 and 1.0)
  */
void Sams_View::SetUpperThreshold(double upper) {
  int sliderValue = round(((upper - NORMALIZED_MIN) / (NORMALIZED_MAX - NORMALIZED_MIN)) * 1000);
  sliderValue = std::min(sliderValue, 1000);
  sliderValue = std::max(sliderValue, 0);

  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UI.sliderMaxThreshold->setValue(sliderValue);
  thresholdingEnabled = wasEnabled;
  UpperThresholdChanged(sliderValue);
}

void Sams_View::TopOnePercent() {
  TopXPercent(1.0);
}

void Sams_View::TopFivePercent() {
  TopXPercent(5.0);
}

void Sams_View::TopTenPercent() {
  TopXPercent(10.0);
}

void Sams_View::TopXPercent() {
  TopXPercent(UI.spinBoxTopXPercent->value());
}

/**
  * Shows the the worst uncertainty.
  *   percentage - between 0 and 100
  *   e.g. percentage = 10 will create a threshold to show the worst 10% of uncertainty.
  */
void Sams_View::TopXPercent(double percentage) {
  mitk::Image::Pointer uncertaintyImage = GetMitkPreprocessedUncertainty();

  // Update the spinBox.
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UI.spinBoxTopXPercent->setValue(percentage);
  UI.sliderTopXPercent->setValue(percentage);
  thresholdingEnabled = wasEnabled;

  if (thresholdingEnabled) {
    thresholder->setUncertainty(GetMitkPreprocessedUncertainty());
    thresholder->setIgnoreZeros(UI.checkBoxIgnoreZeros->isChecked());
    double min, max;
    thresholder->getTopXPercentThreshold(percentage / 100.0, min, max);

    // Filter the uncertainty to only show the top 10%. Avoid filtering twice by disabling thresholding.
    thresholdingEnabled = false;
    SetLowerThreshold(min);
    thresholdingEnabled = true;
    SetUpperThreshold(max);
  }
}

void Sams_View::TopXPercentSliderMoved(double percentage) {
  // Update the spinBox.
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UI.spinBoxTopXPercent->setValue(percentage);
  thresholdingEnabled = wasEnabled;
}

/**
  * Sets the layers and visibility required for the thresholded uncertainty to be displayed on top of the scan.
  */
void Sams_View::DisplayThreshold() {
  // Hide Everything
  HideAllDataNodes();

  // Make Scan and Thresholded Visible  
  ShowDataNode(this->scan);
  ShowDataNode(thresholdedUncertainty);

  // Put Scan behind Thresholded Uncertainty
  SetDataNodeLayer(this->scan, 0);
  SetDataNodeLayer(thresholdedUncertainty, 1);

  this->RequestRenderWindowUpdate();
}

void Sams_View::ResetThresholds() {
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  SetLowerThreshold(0.0);
  thresholdingEnabled = wasEnabled;
  SetUpperThreshold(1.0);
}

void Sams_View::VolumeRenderThreshold(bool checked) {
  // If we don't have any preprocessed uncertainty, do nothing.
  if (!this->preprocessedUncertainty) {
    return;
  }

  // Enable Volume Rendering.
  if (checked) {
    HideAllDataNodes();
    ShowDataNode(this->preprocessedUncertainty);
    this->preprocessedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));

    // Opacity Transfer Function
    double epsilon = 0.001;
    double lowest = UI.checkBoxIgnoreZeros->isChecked()? epsilon : 0.0;
    double lower = std::max(lowest, lowerThreshold);
    double upper = std::max(lowest, upperThreshold);
    mitk::TransferFunction::ControlPoints scalarOpacityPoints;
    if (UI.checkBoxIgnoreZeros->isChecked()) {
      scalarOpacityPoints.push_back(std::make_pair(0.0, 0.0));
    }
    scalarOpacityPoints.push_back(std::make_pair(std::max(lowest, lower - epsilon), 0.0));
    scalarOpacityPoints.push_back(std::make_pair(lower, 1.0));
    scalarOpacityPoints.push_back(std::make_pair(upper, 1.0));
    scalarOpacityPoints.push_back(std::make_pair(std::min(1.0, upper + epsilon), 0.0));

    std::cout << "(" << std::max(lowest, lower - epsilon) << ", " <<
                   lower << ", " <<
                   upper << ", " << 
                   std::min(1.0, upper + epsilon) << ")" << std::endl;

    // Gradient Opacity Transfer Function (to ignore sharp edges)
    mitk::TransferFunction::ControlPoints gradientOpacityPoints;
    gradientOpacityPoints.push_back(std::make_pair(0.0, 1.0));
    gradientOpacityPoints.push_back(std::make_pair(UI.double1->value(), 0.0));

    // Colour Transfer Function
    vtkSmartPointer<vtkColorTransferFunction> colorTransferFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    colorTransferFunction->AddRGBPoint(lower, 1.0, 0.0, 0.0);
    colorTransferFunction->AddRGBPoint(upper, 1.0, 0.0, 0.0);
    
    // Combine them.
    mitk::TransferFunction::Pointer transferFunction = mitk::TransferFunction::New();
    transferFunction->SetScalarOpacityPoints(scalarOpacityPoints);
    transferFunction->SetGradientOpacityPoints(gradientOpacityPoints);
    transferFunction->SetColorTransferFunction(colorTransferFunction);

    this->preprocessedUncertainty->SetProperty("TransferFunction", mitk::TransferFunctionProperty::New(transferFunction));
  }
  // Disable Volume Rendering.
  else {
    this->preprocessedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(false));
  }

  this->RequestRenderWindowUpdate();
}

// ------------ //
// ---- 3b ---- //
// ------------ //

double latLongRatio = 2.0;

void Sams_View::ThetaResolutionChanged(int value) {
  if (UI.buttonLocked->isChecked()) {
    UI.spinBoxSpherePhiResolution->setValue(value / latLongRatio);
  }

  latLongRatio = UI.spinBoxSphereThetaResolution->value() / UI.spinBoxSpherePhiResolution->value();
}

void Sams_View::PhiResolutionChanged(int value) {
  if (UI.buttonLocked->isChecked()) {
    UI.spinBoxSphereThetaResolution->setValue(value * latLongRatio);
  }

  latLongRatio = UI.spinBoxSphereThetaResolution->value() / UI.spinBoxSpherePhiResolution->value();
}

void Sams_View::GenerateUncertaintySphere() {
  std::ostringstream name;
  name << "Sphere Surface";
  mitk::Surface::Pointer generatedSurface = SurfaceGenerator::generateSphere(UI.spinBoxSphereThetaResolution->value(), UI.spinBoxSphereThetaResolution->value());
  mitk::DataNode::Pointer surfaceNode = SaveDataNode(name.str().c_str(), generatedSurface, true);
  
  // ---- Sampling Accumulator Options ---- //
  UncertaintySurfaceMapper::SAMPLING_ACCUMULATOR samplingAccumulator;
  if (UI.radioButtonSphereSampleAccumulatorAverage->isChecked()) {
    samplingAccumulator = UncertaintySurfaceMapper::AVERAGE;
  }
  else if (UI.radioButtonSphereSampleAccumulatorMin->isChecked()) {
    samplingAccumulator = UncertaintySurfaceMapper::MINIMUM;
  }
  else if (UI.radioButtonSphereSampleAccumulatorMax->isChecked()) {
    samplingAccumulator = UncertaintySurfaceMapper::MAXIMUM;
  }

    // ---- Sampling Distance Options ---- ///
  UncertaintySurfaceMapper::SAMPLING_DISTANCE samplingDistance;
  if (UI.radioButtonSphereSamplingDistanceFull->isChecked()) {
    samplingDistance = UncertaintySurfaceMapper::FULL;
  }
  else if (UI.radioButtonSphereSamplingDistanceHalf->isChecked()) {
    samplingDistance = UncertaintySurfaceMapper::HALF;
  }

  // ---- Scaling Options ---- //
  UncertaintySurfaceMapper::SCALING scaling;
  if (UI.radioButtonSphereScalingNone->isChecked()) {
   scaling = UncertaintySurfaceMapper::NONE;
  }
  else if (UI.radioButtonSphereScalingLinear->isChecked()) {
    scaling = UncertaintySurfaceMapper::LINEAR;
  }
  else if (UI.radioButtonSphereScalingHistogram->isChecked()) {
    scaling = UncertaintySurfaceMapper::HISTOGRAM;
  }

  // ---- Colour Options ---- //
  UncertaintySurfaceMapper::COLOUR colour;
  if (UI.radioButtonSphereColourBlackAndWhite->isChecked()) {
   colour = UncertaintySurfaceMapper::BLACK_AND_WHITE;
  }
  else if (UI.radioButtonSphereColourColour->isChecked()) {
    colour = UncertaintySurfaceMapper::BLACK_AND_RED;
  }

  UncertaintySurfaceMapper::REGISTRATION registration = UncertaintySurfaceMapper::SPHERE;

  bool invertNormals = UI.checkBoxSurfaceInvertNormals->isChecked();

  SurfaceMapping(surfaceNode, samplingAccumulator, samplingDistance, scaling, colour, registration, invertNormals);

  HideAllDataNodes();
  ShowDataNode(surfaceNode);
  this->RequestRenderWindowUpdate();
}

// /**
//   * Creates a sphere and maps a texture created from uncertainty to it.
//   */
// void Sams_View::GenerateUncertaintySphere() {
//   // Create Sphere
//   mitk::Surface::Pointer sphereToPutTextureOn = SurfaceGenerator::generateSphere(100, 20);
  
//   // Create Texture
//   UncertaintyTexture * texturerer = new UncertaintyTexture();
//   texturerer->setUncertainty(GetMitkPreprocessedUncertainty());
//   texturerer->setDimensions(UI.spinBoxSphereThetaResolution->value(), UI.spinBoxSpherePhiResolution->value());
//   texturerer->setScalingLinear(UI.radioButtonTextureScalingLinear->isChecked());
//   if (UI.radioButtonTextureSampleAverage->isChecked()) {
//     texturerer->setSamplingAverage();
//   }
//   else if (UI.radioButtonTextureSampleMinimum->isChecked()) {
//    texturerer->setSamplingMinimum(); 
//   }
//   else if (UI.radioButtonTextureSampleMaximum->isChecked()) {
//     texturerer->setSamplingMaximum();
//   }
//   mitk::Image::Pointer texture = texturerer->generateUncertaintyTexture();
  
//   // Adjust legend.
//   char colourLow[3];
//   texturerer->getLegendMinColour(colourLow);
//   char colourHigh[3];
//   texturerer->getLegendMaxColour(colourHigh);
//   SetLegend(texturerer->getLegendMinValue(), colourLow, texturerer->getLegendMaxValue(), colourHigh);
//   ShowLegend();

//   delete texturerer;

//   // Save texture.
//   mitk::DataNode::Pointer textureNode = SaveDataNode("Uncertainty Texture", texture, true);
//   textureNode->SetProperty("layer", mitk::IntProperty::New(3));
//   textureNode->SetProperty("color", mitk::ColorProperty::New(0.0, 1.0, 0.0));
//   textureNode->SetProperty("opacity", mitk::FloatProperty::New(1.0));

//   // Save sphere.
//   mitk::DataNode::Pointer surfaceNode = SaveDataNode("Uncertainty Sphere", sphereToPutTextureOn, true);
//   mitk::SmartPointerProperty::Pointer textureProperty = mitk::SmartPointerProperty::New(texture);
//   surfaceNode->SetProperty("Surface.Texture", textureProperty);
//   surfaceNode->SetProperty("layer", mitk::IntProperty::New(3));
//   surfaceNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
//   surfaceNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
//   surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));

//   HideAllDataNodes();
//   ShowDataNode(surfaceNode);
//   this->RequestRenderWindowUpdate();
// }

// ------------ //
// ---- 3c ---- //
// ------------ //

void Sams_View::SurfaceMapping() {
  mitk::DataNode::Pointer surfaceNode = this->GetDataStorage()->GetNamedNode(UI.comboBoxSurface->currentText().toStdString());
  
  // ---- Sampling Accumulator Options ---- //
  UncertaintySurfaceMapper::SAMPLING_ACCUMULATOR samplingAccumulator;
  if (UI.radioButtonSurfaceSampleAverage->isChecked()) {
    samplingAccumulator = UncertaintySurfaceMapper::AVERAGE;
  }
  else if (UI.radioButtonSurfaceSampleMinimum->isChecked()) {
    samplingAccumulator = UncertaintySurfaceMapper::MINIMUM;
  }
  else if (UI.radioButtonSurfaceSampleMaximum->isChecked()) {
    samplingAccumulator = UncertaintySurfaceMapper::MAXIMUM;
  }

    // ---- Sampling Distance Options ---- ///
  UncertaintySurfaceMapper::SAMPLING_DISTANCE samplingDistance;
  if (UI.radioButtonSamplingFull->isChecked()) {
    samplingDistance = UncertaintySurfaceMapper::FULL;
  }
  else if (UI.radioButtonSamplingHalf->isChecked()) {
    samplingDistance = UncertaintySurfaceMapper::HALF;
  }

  // ---- Scaling Options ---- //
  UncertaintySurfaceMapper::SCALING scaling;
  if (UI.radioButtonScalingNone->isChecked()) {
   scaling = UncertaintySurfaceMapper::NONE;
  }
  else if (UI.radioButtonScalingLinear->isChecked()) {
    scaling = UncertaintySurfaceMapper::LINEAR;
  }
  else if (UI.radioButtonScalingHistogram->isChecked()) {
    scaling = UncertaintySurfaceMapper::HISTOGRAM;
  }

  // ---- Colour Options ---- //
  UncertaintySurfaceMapper::COLOUR colour;
  if (UI.radioButtonColourBlackAndWhite->isChecked()) {
   colour = UncertaintySurfaceMapper::BLACK_AND_WHITE;
  }
  else if (UI.radioButtonColourColour->isChecked()) {
    colour = UncertaintySurfaceMapper::BLACK_AND_RED;
  }

  UncertaintySurfaceMapper::REGISTRATION registration;
  if (UI.radioButtonSurfaceRegistrationNone->isChecked()) {
    registration = UncertaintySurfaceMapper::IDENTITY;
  }
  else if (UI.radioButtonSurfaceRegistrationSimple->isChecked()) {
    registration = UncertaintySurfaceMapper::SIMPLE;
  }
  else if (UI.radioButtonSurfaceRegistrationBodge->isChecked()) {
    registration = UncertaintySurfaceMapper::BODGE;
  }

  bool invertNormals = UI.checkBoxSurfaceInvertNormals->isChecked();
  bool debugRegistration = UI.checkBoxSurfaceDebugRegistration->isChecked();

  SurfaceMapping(surfaceNode, samplingAccumulator, samplingDistance, scaling, colour, registration, invertNormals, debugRegistration);

  HideAllDataNodes();
  ShowDataNode(surfaceNode);
  this->RequestRenderWindowUpdate();
}

/**
  * Takes a surface and maps the uncertainty onto it based on the normal vector.
  */
void Sams_View::SurfaceMapping(
  mitk::DataNode::Pointer surfaceNode,
  UncertaintySurfaceMapper::SAMPLING_ACCUMULATOR samplingAccumulator,
  UncertaintySurfaceMapper::SAMPLING_DISTANCE samplingDistance,
  UncertaintySurfaceMapper::SCALING scaling,
  UncertaintySurfaceMapper::COLOUR colour,
  UncertaintySurfaceMapper::REGISTRATION registration,
  bool invertNormals,
  bool debugRegistration
) {
  if (surfaceNode.IsNull()) {
    std::cout << "Surface is null. Stopping." << std::endl;
    return;
  }

  // Stop it being specular in the rendering.
  surfaceNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  surfaceNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
  surfaceNode->SetProperty("scalar visibility", mitk::BoolProperty::New(true));

  // Lookup Table (2D Transfer Function)
  vtkSmartPointer<vtkLookupTable> vtkLUT = vtkSmartPointer<vtkLookupTable>::New();
  vtkLUT->SetRange(0, 255);
  vtkLUT->SetNumberOfTableValues(256);
  for (int i = 0; i < 256; i++) {
      vtkLUT->SetTableValue(
        i,
        Util::IntensityToRed(i),
        Util::IntensityToGreen(i),
        Util::IntensityToBlue(i)
      );
  }
  vtkLUT->Build();
  mitk::LookupTable::Pointer mitkLookupTable = mitk::LookupTable::New();
  mitkLookupTable->SetVtkLookupTable(vtkLUT);
  mitk::LookupTableProperty::Pointer LookupTableProp = mitk::LookupTableProperty::New(mitkLookupTable);
  surfaceNode->SetProperty("LookupTable", LookupTableProp);      

  // Cast it to an MITK surface.
  mitk::Surface::Pointer mitkSurface = dynamic_cast<mitk::Surface*>(surfaceNode->GetData());

  // Map the uncertainty to it.
  UncertaintySurfaceMapper * mapper = new UncertaintySurfaceMapper();
  mapper->setUncertainty(GetMitkPreprocessedUncertainty());
  mapper->setSurface(mitkSurface);
  mapper->setSamplingAccumulator(samplingAccumulator);
  mapper->setSamplingDistance(samplingDistance);
  mapper->setScaling(scaling);
  mapper->setColour(colour);
  mapper->setRegistration(registration);
  mapper->setInvertNormals(invertNormals);
  mapper->setDebugRegistration(debugRegistration);
  mapper->map();

  // Adjust legend.
  char colourLow[3];
  mapper->getLegendMinColour(colourLow);
  char colourHigh[3];
  mapper->getLegendMaxColour(colourHigh);
  SetLegend(mapper->getLegendMinValue(), colourLow, mapper->getLegendMaxValue(), colourHigh);
  ShowLegend();

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

// ------------ //
// ---- 3d ---- //
// ------------ //

void Sams_View::ComputeNextScanPlane() {
  // Compute the next scan plane.
  SVDScanPlaneGenerator * calculator = new SVDScanPlaneGenerator();
  calculator->setUncertainty(GetMitkPreprocessedUncertainty());
  vtkSmartPointer<vtkPlane> plane = calculator->calculateBestScanPlane();

  double * center = plane->GetOrigin();
  double * normal = plane->GetNormal();

  vtkVector<float, 3> vectorOrigin = vtkVector<float, 3>();
  vectorOrigin[0] = center[0];
  vectorOrigin[1] = center[1];
  vectorOrigin[2] = center[2];

  vtkVector<float, 3> vectorNormal = vtkVector<float, 3>();
  vectorNormal[0] = normal[0];
  vectorNormal[1] = normal[1];
  vectorNormal[2] = normal[2];
  vectorNormal.Normalize();

  // Create a surface to represent it.
  mitk::Surface::Pointer mitkPlane = SurfaceGenerator::generatePlane(100, 100, vectorOrigin, vectorNormal);

  // Align it with the scan.
  mitk::SlicedGeometry3D * scanSlicedGeometry = GetMitkScan()->GetSlicedGeometry();
  mitk::Point3D scanOrigin = scanSlicedGeometry->GetOrigin();
  mitk::AffineTransform3D * scanTransform = scanSlicedGeometry->GetIndexToWorldTransform();

  mitk::BaseGeometry * basePlaneGeometry = mitkPlane->GetGeometry();
  basePlaneGeometry->SetOrigin(scanOrigin);
  basePlaneGeometry->SetIndexToWorldTransform(scanTransform);

  // Save it.
  SaveDataNode("Scan Plane", mitkPlane, true);

  // Create a box to represent all the slices in the scan.
  vtkVector<float, 3> vectorOldNormal = vtkVector<float, 3>();
  vectorOldNormal[0] = 0;
  vectorOldNormal[1] = 0;
  vectorOldNormal[2] = 1;

  vtkVector<float, 3> newYAxis = Util::vectorCross(vectorOldNormal, vectorNormal);
  vtkVector<float, 3> newXAxis = Util::vectorCross(vectorNormal, newYAxis);
  vtkVector<float, 3> newZAxis = vectorNormal;

  newXAxis.Normalize();
  newYAxis.Normalize();
  newZAxis.Normalize();

  mitk::Surface::Pointer mitkScanBox = SurfaceGenerator::generateCuboid(120, 120, 300, vectorOrigin, newXAxis, newYAxis, newZAxis);

  // Align it with the scan
  mitk::BaseGeometry * baseBoxGeometry = mitkScanBox->GetGeometry();
  baseBoxGeometry->SetOrigin(scanOrigin);
  baseBoxGeometry->SetIndexToWorldTransform(scanTransform);

  mitk::DataNode::Pointer box = SaveDataNode("Scan Box", mitkScanBox, true);
  box->SetProperty("opacity", mitk::FloatProperty::New(0.5));

  UI.spinBoxNextBestPointX->setValue(center[0]);
  UI.spinBoxNextBestPointY->setValue(center[1]);
  UI.spinBoxNextBestPointZ->setValue(center[2]);

  UI.spinBoxNextBestXDirectionX->setValue(newXAxis[0]);
  UI.spinBoxNextBestXDirectionY->setValue(newXAxis[1]);
  UI.spinBoxNextBestXDirectionZ->setValue(newXAxis[2]);

  UI.spinBoxNextBestYDirectionX->setValue(newYAxis[0]);
  UI.spinBoxNextBestYDirectionY->setValue(newYAxis[1]);
  UI.spinBoxNextBestYDirectionZ->setValue(newYAxis[2]);

  UI.spinBoxNextBestZDirectionX->setValue(newZAxis[0]);
  UI.spinBoxNextBestZDirectionY->setValue(newZAxis[1]);
  UI.spinBoxNextBestZDirectionZ->setValue(newZAxis[2]);
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

void Sams_View::ToggleLegend(int state) {
  if (state > 0) {
    ShowLegend();
  }
  else {
    HideLegend();
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

void Sams_View::DebugOverlay() {
  mitk::OverlayManager::Pointer overlayManager = GetOverlayManager();

  // mitk::ScaleLegendOverlay::Pointer scaleOverlay = mitk::ScaleLegendOverlay::New();
  // scaleOverlay->SetLeftAxisVisibility(true);
  // overlayManager->AddOverlay(scaleOverlay.GetPointer());

  char colour1[3] = {255, 0, 0};
  char colour2[3] = {0, 255, 0};
  SetLegend(0.0, colour1, 1.0, colour2);
  ShowLegend();

  // //Create a textOverlay2D
  // mitk::TextOverlay2D::Pointer textOverlay = mitk::TextOverlay2D::New();
  // textOverlay->SetText("Test!"); //set UTF-8 encoded text to render
  // textOverlay->SetFontSize(40);
  // textOverlay->SetColor(1,0,0); //Set text color to red
  // textOverlay->SetOpacity(1);

  // //The position of the Overlay can be set to a fixed coordinate on the display.
  // mitk::Point2D pos;
  // pos[0] = 10,pos[1] = 20;
  // textOverlay->SetPosition2D(pos);
  
  // //Add the overlay to the overlayManager. It is added to all registered renderers automatically
  // overlayManager->AddOverlay(textOverlay.GetPointer());

  this->RequestRenderWindowUpdate();
}

void Sams_View::BodgeSurface() {
  mitk::DataNode::Pointer surfaceNode = this->GetDataStorage()->GetNamedNode(UI.comboBoxSurface->currentText().toStdString());
  mitk::Surface::Pointer mitkSurface = dynamic_cast<mitk::Surface*>(surfaceNode->GetData());
  mitk::BaseGeometry * surfaceBaseGeometry = mitkSurface->GetGeometry();
  //surfaceBaseGeometry->SetOrigin(uncertaintyOrigin);

  vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
  // Col 1
  matrix->SetElement(0, 0, -1);
  matrix->SetElement(1, 0, 0);
  matrix->SetElement(2, 0, 0);
  matrix->SetElement(3, 0, 0);
  // Col 2
  matrix->SetElement(0, 1, 0);
  matrix->SetElement(1, 1, -1);
  matrix->SetElement(2, 1, 0);
  matrix->SetElement(3, 1, 0);
  // Col 3
  matrix->SetElement(0, 2, 0);
  matrix->SetElement(1, 2, 0);
  matrix->SetElement(2, 2, 1);
  matrix->SetElement(3, 2, 0);
  // Col 4
  matrix->SetElement(0, 3, 0);
  matrix->SetElement(1, 3, 0);
  matrix->SetElement(2, 3, 0);
  matrix->SetElement(3, 3, 1);

  surfaceBaseGeometry->SetIndexToWorldTransformByVtkMatrix(matrix);
  this->RequestRenderWindowUpdate();
}

// ------------------------ //
// ---- Reconstruction ---- //
// ------------------------ //

#include <ctkCmdLineModuleManager.h>
#include <QDesktopServices>

#include <ctkCmdLineModuleBackend.h>
#include <ctkCmdLineModuleBackendLocalProcess.h>

#include <ctkCmdLineModuleReference.h>
#include <QUrl>
#include <ctkException.h>
#include <QDebug>

#include <ctkCmdLineModuleFrontendFactory.h>
#include <ctkCmdLineModuleFrontendFactoryQtGui.h>
#include <QWidget>

#include "QmitkCmdLineModuleFactoryGui.h"

#include <ctkCmdLineModuleFuture.h>
#include <ctkCmdLineModuleRunException.h>
#include <ctkCmdLineModuleTimeoutException.h>

#include <QPlainTextEdit>

#include <QFileDialog>

ctkCmdLineModuleManager* moduleManager;
ctkCmdLineModuleBackend* processBackend;
ctkCmdLineModuleFrontend* frontend;

void Sams_View::ReconstructGUI() {
  // MODULE MANAGER
  moduleManager = new ctkCmdLineModuleManager(
      // Use "weak" validation mode.
      ctkCmdLineModuleManager::WEAK_VALIDATION,
      // Use the default cache location for this application
      #if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
        QDesktopServices::storageLocation(QDesktopServices::CacheLocation)
      #else
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
      #endif
  );

  // BACKEND
  processBackend = new ctkCmdLineModuleBackendLocalProcess();
  // Register the back-end with the module manager.
  moduleManager->registerBackend(processBackend);

  const QString reconstructionExecutableName = QFileDialog::getOpenFileName(UI.widgetReconstructExecutableWrapper, tr("Where is reconstruction_GPU2_wrapper?"), "~");
  UI.textEditReconstructionExecutablePath->setPlainText(reconstructionExecutableName);

  // REGISTER
  ctkCmdLineModuleReference moduleRef;
  bool moduleValid = false;
  try {
    // Register a local executable as a module, the ctkCmdLineModuleBackendLocalProcess
    // can handle it.
    moduleRef = moduleManager->registerModule(QUrl::fromLocalFile(reconstructionExecutableName));
    moduleValid = true;
  }
  catch (const ctkInvalidArgumentException& e) {
    // Module validation failed.
    cerr << "Module validation failed: " << reconstructionExecutableName.toStdString() << endl;
    qDebug() << e;
  }
  catch (ctkCmdLineModuleTimeoutException) {
    cerr << "Retrieving XML from module timed out...: " << reconstructionExecutableName.toStdString() << endl;
  }
  catch (ctkCmdLineModuleRunException) {
    cerr << "There was a problem running the module. Check it exists: " << reconstructionExecutableName.toStdString() << endl;
  } 

  // If the module couldn't be loaded, show an error and clear the widget.
  if (!moduleValid) {
    ClearReconstructionUI();
    UI.labelReconstructExecutableError->setVisible(true);
    UI.labelReconstructExecutableSuccess->setVisible(false);
    return;
  }
  else {
    UI.labelReconstructExecutableError->setVisible(false);
    UI.labelReconstructExecutableSuccess->setVisible(true);
  }

  // FRONTEND
  // We use the "Qt Gui" frontend factory.
  QScopedPointer<ctkCmdLineModuleFrontendFactory> frontendFactory(new QmitkCmdLineModuleFactoryGui(this->GetDataStorage()));
  frontend = frontendFactory->create(moduleRef);
  // Create the actual GUI representation.
  QWidget* gui = qobject_cast<QWidget*>(frontend->guiHandle());

  // ADD TO UI
  ClearReconstructionUI();
  UI.widgetPutGUIHere->layout()->addWidget(gui);
}

#include <QList>
#include <ctkCmdLineModuleParameter.h>
#include <QString>
#include <mitkIOUtil.h>

#include <QLayoutItem>

void Sams_View::ReconstructGo() {
  try {
    ctkCmdLineModuleFuture future = moduleManager->run(frontend);
    future.waitForFinished();
    qDebug() << "Console output:";
    qDebug() << future.readAllOutputData();
    qDebug() << "Error output:";
    qDebug() << future.readAllErrorData();
    qDebug() << "Results:";
    qDebug() << future.results();
  }
  catch (const ctkCmdLineModuleRunException& e) {
    qWarning() << e;
  }

  // Read in the output files.
  QList<ctkCmdLineModuleParameter> parameters;
  parameters = frontend->parameters("image", ctkCmdLineModuleFrontend::Output);
  parameters << frontend->parameters("file", ctkCmdLineModuleFrontend::Output);

  foreach (ctkCmdLineModuleParameter parameter, parameters) {
    QString parameterName = parameter.name();
    QString outputFileName = frontend->value(parameterName, ctkCmdLineModuleFrontend::DisplayRole).toString();
    mitk::IOUtil::Load(outputFileName.toStdString(), *(this->GetDataStorage()));
  }
}

void Sams_View::ClearReconstructionUI() {
  QLayout *layout = UI.widgetPutGUIHere->layout();
  if (layout != NULL) {
    delete layout;
    qDeleteAll(UI.widgetPutGUIHere->children());
  }
  layout = new QVBoxLayout(UI.widgetPutGUIHere);
  layout->setContentsMargins(0, 0, 0, 0);
}

// ------------------------ //
// ---- Simulated Scan ---- //
// ------------------------ //

void Sams_View::ScanSimulationDropdownChanged(const QString & scanSimulationName) {
  mitk::DataNode::Pointer potentialScanSimulation = this->GetDataStorage()->GetNamedNode(scanSimulationName.toStdString());
  this->scanSimulationVolume = potentialScanSimulation;
}

void Sams_View::ScanSimulationSetPointCenter() {
  if (scanSimulationVolume.IsNotNull()) {
    bool previousEnabled = scanPreviewEnabled;
    scanPreviewEnabled = false;
    mitk::Image::Pointer mitkSimulationVolume = Util::MitkImageFromNode(scanSimulationVolume);
    UI.doubleSpinBoxScanPointX->setValue(mitkSimulationVolume->GetDimension(0) / 2.0);
    UI.doubleSpinBoxScanPointY->setValue(mitkSimulationVolume->GetDimension(1) / 2.0);
    scanPreviewEnabled = previousEnabled;
    UI.doubleSpinBoxScanPointZ->setValue(mitkSimulationVolume->GetDimension(2) / 2.0);
  }
}

void Sams_View::ScanSimulationSetDirectionAxial() {
  bool previousEnabled = scanPreviewEnabled;
  scanPreviewEnabled = false;

  // X is X
  UI.doubleSpinBoxScanXAxisX->setValue(1.0);
  UI.doubleSpinBoxScanXAxisY->setValue(0.0);
  UI.doubleSpinBoxScanXAxisZ->setValue(0.0);

  // Y is -Z
  UI.doubleSpinBoxScanYAxisX->setValue(0.0);
  UI.doubleSpinBoxScanYAxisY->setValue(0.0);
  UI.doubleSpinBoxScanYAxisZ->setValue(-1.0);

  // Z is Y
  UI.doubleSpinBoxScanZAxisX->setValue(0.0);
  UI.doubleSpinBoxScanZAxisY->setValue(1.0);
  scanPreviewEnabled = previousEnabled;
  UI.doubleSpinBoxScanZAxisZ->setValue(0.0);
}

void Sams_View::ScanSimulationSetDirectionCoronal() {
  bool previousEnabled = scanPreviewEnabled;
  scanPreviewEnabled = false;

  // X is X
  UI.doubleSpinBoxScanXAxisX->setValue(1.0);
  UI.doubleSpinBoxScanXAxisY->setValue(0.0);
  UI.doubleSpinBoxScanXAxisZ->setValue(0.0);

  // Y is Y
  UI.doubleSpinBoxScanYAxisX->setValue(0.0);
  UI.doubleSpinBoxScanYAxisY->setValue(1.0);
  UI.doubleSpinBoxScanYAxisZ->setValue(0.0);

  // Z is Z
  UI.doubleSpinBoxScanZAxisX->setValue(0.0);
  UI.doubleSpinBoxScanZAxisY->setValue(0.0);
  scanPreviewEnabled = previousEnabled;
  UI.doubleSpinBoxScanZAxisZ->setValue(1.0);
}

void Sams_View::ScanSimulationSetDirectionSagittal() {
  bool previousEnabled = scanPreviewEnabled;
  scanPreviewEnabled = false;
  
  // X is -Z
  UI.doubleSpinBoxScanXAxisX->setValue(0.0);
  UI.doubleSpinBoxScanXAxisY->setValue(0.0);
  UI.doubleSpinBoxScanXAxisZ->setValue(-1.0);

  // Y is Y
  UI.doubleSpinBoxScanYAxisX->setValue(0.0);
  UI.doubleSpinBoxScanYAxisY->setValue(1.0);
  UI.doubleSpinBoxScanYAxisZ->setValue(0.0);

  // Z is X
  UI.doubleSpinBoxScanZAxisX->setValue(1.0);
  UI.doubleSpinBoxScanZAxisY->setValue(0.0);
  scanPreviewEnabled = previousEnabled;
  UI.doubleSpinBoxScanZAxisZ->setValue(0.0);
}

void Sams_View::ScanSimulationSimulateScan() {
  vtkVector<float, 3> xAxis = vtkVector<float, 3>();
  xAxis[0] = UI.doubleSpinBoxScanXAxisX->value();
  xAxis[1] = UI.doubleSpinBoxScanXAxisY->value();
  xAxis[2] = UI.doubleSpinBoxScanXAxisZ->value();

  vtkVector<float, 3> yAxis = vtkVector<float, 3>();
  yAxis[0] = UI.doubleSpinBoxScanYAxisX->value();
  yAxis[1] = UI.doubleSpinBoxScanYAxisY->value();
  yAxis[2] = UI.doubleSpinBoxScanYAxisZ->value();

  vtkVector<float, 3> zAxis = vtkVector<float, 3>();
  zAxis[0] = UI.doubleSpinBoxScanZAxisX->value();
  zAxis[1] = UI.doubleSpinBoxScanZAxisY->value();
  zAxis[2] = UI.doubleSpinBoxScanZAxisZ->value();

  vtkVector<float, 3> center = vtkVector<float, 3>();
  center[0] = UI.doubleSpinBoxScanPointX->value();
  center[1] = UI.doubleSpinBoxScanPointY->value();
  center[2] = UI.doubleSpinBoxScanPointZ->value();

  ScanSimulator * simulator = new ScanSimulator();
  simulator->setVolume(GetMitkScanVolume());
  simulator->setScanAxes(xAxis, yAxis, zAxis);
  simulator->setScanResolution(1.0, 1.0, 2.0);
  simulator->setScanSize(UI.spinBoxScanDimensionX->value(), UI.spinBoxScanDimensionY->value(), UI.spinBoxScanDimensionZ->value());
  simulator->setMotionCorruption(UI.checkBoxMotionCorruptionEnabled->isChecked());
  simulator->setMotionCorruptionMaxAngle(UI.spinBoxMotionCorruptionAngle->value());
  simulator->setScanCenter(center);

  mitk::Image::Pointer sliceStack = simulator->scan();

  // Store it as a DataNode.
  mitk::DataNode::Pointer cubeNode = SaveDataNode("Slice Stack", sliceStack, true);
}

const std::string SCAN_PREVIEW_NAME = "Scan Preview";

void Sams_View::ToggleScanSimulationPreview(bool checked) {
  scanPreviewEnabled = checked;
  if (scanPreviewEnabled) {
    ScanSimulationPreview();
  }
  else {
    ScanSimulationRemovePreview();
  }
}

void Sams_View::ScanSimulationPreview() {
  if (scanPreviewEnabled) {
    vtkVector<float, 3> center = vtkVector<float, 3>();
    center[0] = UI.doubleSpinBoxScanPointX->value();
    center[1] = UI.doubleSpinBoxScanPointY->value();
    center[2] = UI.doubleSpinBoxScanPointZ->value();

    vtkVector<float, 3> xAxis = vtkVector<float, 3>();
    xAxis[0] = UI.doubleSpinBoxScanXAxisX->value();
    xAxis[1] = UI.doubleSpinBoxScanXAxisY->value();
    xAxis[2] = UI.doubleSpinBoxScanXAxisZ->value();

    vtkVector<float, 3> yAxis = vtkVector<float, 3>();
    yAxis[0] = UI.doubleSpinBoxScanYAxisX->value();
    yAxis[1] = UI.doubleSpinBoxScanYAxisY->value();
    yAxis[2] = UI.doubleSpinBoxScanYAxisZ->value();

    vtkVector<float, 3> zAxis = vtkVector<float, 3>();
    zAxis[0] = UI.doubleSpinBoxScanZAxisX->value();
    zAxis[1] = UI.doubleSpinBoxScanZAxisY->value();
    zAxis[2] = UI.doubleSpinBoxScanZAxisZ->value();

    mitk::Surface::Pointer scanPreview = SurfaceGenerator::generateCuboid(
        UI.spinBoxScanDimensionY->value(),
        UI.spinBoxScanDimensionX->value(),
        UI.spinBoxScanDimensionZ->value() * 2.0,
        center,
        xAxis,
        yAxis,
        zAxis
    );

    // Align it with the ground truth volume.
    if (scanSimulationVolume.IsNotNull()) {
      mitk::SlicedGeometry3D * scanSlicedGeometry = GetMitkScanVolume()->GetSlicedGeometry();
      mitk::Point3D scanOrigin = scanSlicedGeometry->GetOrigin();
      mitk::AffineTransform3D * scanTransform = scanSlicedGeometry->GetIndexToWorldTransform();

      mitk::BaseGeometry * baseBoxGeometry = scanPreview->GetGeometry();
      baseBoxGeometry->SetOrigin(scanOrigin);
      baseBoxGeometry->SetIndexToWorldTransform(scanTransform);
    }

    mitk::DataNode::Pointer previewBox = SaveDataNode(SCAN_PREVIEW_NAME.c_str(), scanPreview, true);
    previewBox->SetProperty("opacity", mitk::FloatProperty::New(0.5));
  }
}

void Sams_View::ScanSimulationRemovePreview() {
  RemoveDataNode(SCAN_PREVIEW_NAME.c_str());
}