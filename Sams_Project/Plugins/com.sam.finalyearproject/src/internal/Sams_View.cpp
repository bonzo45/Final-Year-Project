#include "Sams_View.h"

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qt
#include <QMessageBox>

// Sam's Helper Functions
#include "Util.h"
#include "UncertaintyPreprocessor.h"
#include "UncertaintySampler.h"
#include "SurfaceGenerator.h"
#include "UncertaintySurfaceMapper.h"
#include "UncertaintyGenerator.h"
#include "RANSACScanPlaneGenerator.h"
#include "SVDScanPlaneGenerator.h"
#include "ScanSimulator.h"
#include "NoPointsException.h"

// MITK Interaction
#include <mitkIRenderWindowPart.h>
#include <mitkILinkedRenderWindowPart.h>
#include <mitkIRenderingManager.h>
#include <mitkNodePredicateProperty.h>
#include <mitkNodePredicateDataType.h>

// C Libraries
#include <algorithm> // for min/max
#include <cstdlib>

// Reconstruction Landmarks
#include <mitkPointSet.h>
#include <mitkPoint.h>
#include <mitkInteractionConst.h>
#include "SamsPointSet.h"

// ------------------------ //
// ---- Reconstruction ---- //
// ------------------------ //
#include <QDebug>
#include <QUrl>
#include <QWidget>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QList>
#include <QString>
#include <QLayoutItem>
#include <QDesktopServices>

#include <ctkCmdLineModuleReference.h>
#include <ctkCmdLineModuleParameter.h>
#include <ctkCmdLineModuleBackendLocalProcess.h>
#include <ctkCmdLineModuleFrontendFactoryQtGui.h>
#include <QmitkCmdLineModuleFactoryGui.h>
#include <ctkCmdLineModuleFuture.h>

#include <ctkException.h>
#include <ctkCmdLineModuleRunException.h>
#include <ctkCmdLineModuleTimeoutException.h>

#include <mitkIOUtil.h>

// ---------------------- //
// ---- Thresholding ---- //
// ---------------------- //
#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>
#include <mitkRenderingModeProperty.h>

// ---------------------------- //
// ---- Uncertainty Sphere ---- //
// ---------------------------- //
#include <mitkSurface.h>
#include <mitkSmartPointerProperty.h>

// ---------------------------- //
// --- Uncertainty Surface ---- //
// ---------------------------- //
#include <vtkLookupTable.h>
#include <mitkLookupTableProperty.h>

// ------------------ //
// ---- Overlays ---- //
// ------------------ //
#include <QmitkRenderWindow.h>
#include <mitkScaleLegendOverlay.h>

const std::string VIEW_ID = "org.mitk.views.sams_view";

// ------------------------ //
// ---- Strings (Text) ---- //
// ------------------------ //
const QString RANDOM_NAME = QString::fromStdString("Random (Demo)");
const QString SPHERE_NAME = QString::fromStdString("Sphere (Demo)");
const QString CUBE_NAME = QString::fromStdString("Cube (Demo)");
const QString QUAD_SPHERE_NAME = QString::fromStdString("Sphere in Quadrant (Demo)");

const QString SPHERE_SURFACE_NAME = QString::fromStdString("Sphere");
const QString CUBE_SURFACE_NAME = QString::fromStdString("Cube");
const QString CYLINDER_SURFACE_NAME = QString::fromStdString("Cylinder");

const std::string SCAN_PREVIEW_NAME = "Scan Preview";

/**
  * Create the UI, connects up Signals and Slots.
  */
void Sams_View::CreateQtPartControl(QWidget *parent) {
  // Create Qt UI
  UI.setupUi(parent);

  // Add event handlers.
  // Select Scan & Uncertainty
  connect(UI.comboBoxScan, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(ScanDropdownChanged(const QString &)));
  connect(UI.comboBoxUncertainty, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(UncertaintyDropdownChanged(const QString &)));  
  connect(UI.buttonConfirmSelection, SIGNAL(clicked()), this, SLOT(ConfirmSelection()));
  connect(UI.checkBoxScanVisible, SIGNAL(toggled(bool)), this, SLOT(ToggleScanVisible(bool)));
  connect(UI.checkBoxUncertaintyVisible, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyVisible(bool)));

  // Preprocessing
  connect(UI.checkBoxErosionEnabled, SIGNAL(toggled(bool)), this, SLOT(ToggleErosionEnabled(bool)));

  // Thresholding
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
  connect(UI.buttonThresholdingReset, SIGNAL(clicked()), this, SLOT(ResetThresholds()));

  // Texture Mapping
  connect(UI.spinBoxSphereThetaResolution, SIGNAL(valueChanged(int)), this, SLOT(ThetaResolutionChanged(int)));
  connect(UI.spinBoxSpherePhiResolution, SIGNAL(valueChanged(int)), this, SLOT(PhiResolutionChanged(int)));
  connect(UI.buttonSphere, SIGNAL(clicked()), this, SLOT(GenerateUncertaintySphere()));

  // Surface Mapping
  connect(UI.buttonSurfaceMapping, SIGNAL(clicked()), this, SLOT(SurfaceMapping()));

  // Next Scan Plane
  connect(UI.buttonNextScanPlaneShowThresholded, SIGNAL(clicked()), this, SLOT(NextScanPlaneShowThresholded()));
  connect(UI.buttonNextScanPlane, SIGNAL(clicked()), this, SLOT(ComputeNextScanPlane()));

  // Options
  connect(UI.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(UI.checkBoxLegend, SIGNAL(stateChanged(int)), this, SLOT(ToggleLegend(int)));
  connect(UI.buttonResetViews, SIGNAL(clicked()), this, SLOT(ResetViews()));

  // UI
  connect(UI.buttonMinimize4, SIGNAL(clicked()), this, SLOT(ToggleOptions()));
  connect(UI.buttonReset2, SIGNAL(clicked()), this, SLOT(ResetPreprocessingSettings()));
  connect(UI.buttonVisualizeSelect, SIGNAL(clicked()), this, SLOT(ShowVisualizeSelect()));
  connect(UI.buttonVisualizeThreshold, SIGNAL(clicked()), this, SLOT(ShowVisualizeThreshold()));
  connect(UI.buttonVisualizeSphere, SIGNAL(clicked()), this, SLOT(ShowVisualizeSphere()));
  connect(UI.buttonVisualizeSurface, SIGNAL(clicked()), this, SLOT(ShowVisualizeSurface()));
  connect(UI.buttonVisualizeNextScanPlane, SIGNAL(clicked()), this, SLOT(ShowVisualizeNextScanPlane()));

  // Debug
  connect(UI.buttonToggleDebug, SIGNAL(clicked()), this, SLOT(ToggleDebug()));
  connect(UI.buttonDebugRandom, SIGNAL(clicked()), this, SLOT(GenerateRandomUncertainty()));
  connect(UI.buttonDebugCube, SIGNAL(clicked()), this, SLOT(GenerateCubeUncertainty()));
  connect(UI.buttonDebugSphere, SIGNAL(clicked()), this, SLOT(GenerateSphereUncertainty()));
  connect(UI.buttonDebugQuadSphere, SIGNAL(clicked()), this, SLOT(GenerateQuadrantSphereUncertainty()));
  connect(UI.buttonDebug1, SIGNAL(clicked()), SLOT(debug1()));
  connect(UI.buttonDebug2, SIGNAL(clicked()), SLOT(debug2()));

  // RECONSTRUCTION
  connect(UI.buttonReconstructGUI, SIGNAL(clicked()), this, SLOT(ReconstructGUI()));
  connect(UI.spinBoxMarkLandmarksNumStacks, SIGNAL(valueChanged(int)), this, SLOT(ReconstructLandmarksNumStacksChanged(int)));
  connect(UI.buttonReconstructGo, SIGNAL(clicked()), this, SLOT(ReconstructGo()));

  // SCAN SIMULATION
  connect(UI.comboBoxSimulateScanVolume, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(ScanSimulationDropdownChanged(const QString &)));
  connect(UI.buttonScanPointCenter, SIGNAL(clicked()), this, SLOT(ScanSimulationSetPointCenter()));
  connect(UI.buttonDirectionAxial, SIGNAL(clicked()), this, SLOT(ScanSimulationSetDirectionAxial()));
  connect(UI.buttonDirectionCoronal, SIGNAL(clicked()), this, SLOT(ScanSimulationSetDirectionCoronal()));
  connect(UI.buttonDirectionSagittal, SIGNAL(clicked()), this, SLOT(ScanSimulationSetDirectionSagittal()));
  connect(UI.buttonSimulateScan, SIGNAL(clicked()), this, SLOT(ScanSimulationSimulateScan()));
  connect(UI.buttonScanPreview, SIGNAL(clicked()), this, SLOT(ScanSimulationPreview()));

  connect(UI.buttonScanPreview, SIGNAL(toggled(bool)), this, SLOT(ScanSimulationTogglePreview(bool)));
  connect(UI.spinBoxScanDimensionX, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanDimensionY, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanDimensionZ, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanResolutionX, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanResolutionY, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
  connect(UI.spinBoxScanResolutionZ, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationPreview()));
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
  
  connect(UI.dialScanRotationX, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationRotateX(int)));
  connect(UI.dialScanRotationY, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationRotateY(int)));
  connect(UI.dialScanRotationZ, SIGNAL(valueChanged(int)), this, SLOT(ScanSimulationRotateZ(int)));
  
  Initialize();
}

/**
  * Sets up the UI and generally sorts out the app when it starts.
  */
void Sams_View::Initialize() {
  // Hide Debug Widget.
  UI.widgetDebug->setVisible(false);

  // Hide Options Widget
  UI.widget4Minimizable->setVisible(false);

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

  // Hide SVD error
  UI.labelNextScanSVDError->setVisible(false);
  
  ShowVisualizeSelect();

  UI.buttonScanPreview->setChecked(false);
  scanPreviewEnabled = false;

  ReconstructInitializeLandmarkList();

  numSliceStacks = 0;
  UI.toolBoxMarkLandmarks->removeItem(0);
  ReconstructLandmarksNumStacksChanged(1);

  // Initialize Drop-Down boxes.
  UpdateSelectionDropDowns();

  directionX = vtkVector<float, 3>(0.0f);
  directionY = vtkVector<float, 3>(0.0f);
  directionZ = vtkVector<float, 3>(0.0f);
}

// ------------------------ //
// ---- Simulated Scan ---- //
// ------------------------ //

/**
  * Convenience method to get the volume we're scanning.
  */
mitk::Image::Pointer Sams_View::GetMitkScanVolume() {
  return Util::MitkImageFromNode(scanSimulationVolume);
}

/**
  * Called when the scan volume selection box changes.
  */
void Sams_View::ScanSimulationDropdownChanged(const QString & scanSimulationName) {
  mitk::DataNode::Pointer potentialScanSimulation = this->GetDataStorage()->GetNamedNode(scanSimulationName.toStdString());
  this->scanSimulationVolume = potentialScanSimulation;
}

/**
  * Sets the scan center to be the center of the volume.
  */
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

/**
  * Sets the scan direction to axial.
  */
void Sams_View::ScanSimulationSetDirectionAxial() {
  // X is X
  directionX[0] = 1.0;
  directionX[1] = 0.0;
  directionX[2] = 0.0;

  // Y is -Z
  directionY[0] = 0.0;
  directionY[1] = 0.0;
  directionY[2] = -1.0;

  // Z is Y
  directionZ[0] = 0.0;
  directionZ[1] = 1.0;
  directionZ[2] = 0.0;

  ScanSimulationResetRotations();
  ScanSimulationPreview();
}

/**
  * Sets the scan direction to coronal.
  */
void Sams_View::ScanSimulationSetDirectionCoronal() {
  // X is X
  directionX[0] = 1.0;
  directionX[1] = 0.0;
  directionX[2] = 0.0;

  // Y is Y
  directionY[0] = 0.0;
  directionY[1] = 1.0;
  directionY[2] = 0.0;

  // Z is Z
  directionZ[0] = 0.0;
  directionZ[1] = 0.0;
  directionZ[2] = 1.0;

  ScanSimulationResetRotations();
  ScanSimulationPreview();
}

/**
  * Sets the scan direction to sagittal.
  */
void Sams_View::ScanSimulationSetDirectionSagittal() {  
  // X is -Z
  directionX[0] = 0.0;
  directionX[1] = 0.0;
  directionX[2] = -1.0;

  // Y is Y
  directionY[0] = 0.0;
  directionY[1] = 1.0;
  directionY[2] = 0.0;

  // Z is X
  directionZ[0] = 1.0;
  directionZ[1] = 0.0;
  directionZ[2] = 0.0;

  ScanSimulationResetRotations();
  ScanSimulationPreview();
}

/**
  * Resets all of the rotations to be 0 degrees.
  */
void Sams_View::ScanSimulationResetRotations() {
  bool wasEnabled = scanPreviewEnabled;
  scanPreviewEnabled = false;
  
  UI.dialScanRotationX->setValue(0);
  UI.dialScanRotationY->setValue(0);
  UI.dialScanRotationZ->setValue(0);

  scanPreviewEnabled = wasEnabled;
}

/**
  * Rotates the scan around the X-axis.
  */
void Sams_View::ScanSimulationRotateX(int angle) {
  rotationX = angle;
  ScanSimulationPreview();
}

/**
  * Rotates the scan around the Y-axis.
  */
void Sams_View::ScanSimulationRotateY(int angle) {
  rotationY = angle;
  ScanSimulationPreview();
}

/**
  * Rotates the scan around the Z-axis.
  */
void Sams_View::ScanSimulationRotateZ(int angle) {
  rotationZ = angle;
  ScanSimulationPreview();
}

/**
  * Show scan settings preview. (a 3D box)
  */
void Sams_View::ScanSimulationPreview() {
  if (scanPreviewEnabled) {
    vtkVector<float, 3> center = vtkVector<float, 3>();
    center[0] = UI.doubleSpinBoxScanPointX->value();
    center[1] = UI.doubleSpinBoxScanPointY->value();
    center[2] = UI.doubleSpinBoxScanPointZ->value();

    // Update scan direction in UI by combining rotation and direction.
    vtkVector<float, 3> xAxis = vtkVector<float, 3>(directionX);
    vtkVector<float, 3> yAxis = vtkVector<float, 3>(directionY);
    vtkVector<float, 3> zAxis = vtkVector<float, 3>(directionZ);

    // TODO: work out the rotations...
    vtkSmartPointer<vtkTransform> rotation = vtkSmartPointer<vtkTransform>::New();
    rotation->RotateX(rotationX);
    rotation->RotateY(rotationY);
    rotation->RotateZ(rotationZ);

    rotation->TransformPoint(&(xAxis[0]), &(xAxis[0]));
    rotation->TransformPoint(&(yAxis[0]), &(yAxis[0]));
    rotation->TransformPoint(&(zAxis[0]), &(zAxis[0]));

    UI.doubleSpinBoxScanXAxisX->setValue(xAxis[0]);
    UI.doubleSpinBoxScanXAxisY->setValue(xAxis[1]);
    UI.doubleSpinBoxScanXAxisZ->setValue(xAxis[2]);

    UI.doubleSpinBoxScanYAxisX->setValue(yAxis[0]);
    UI.doubleSpinBoxScanYAxisY->setValue(yAxis[1]);
    UI.doubleSpinBoxScanYAxisZ->setValue(yAxis[2]);

    UI.doubleSpinBoxScanZAxisX->setValue(zAxis[0]);
    UI.doubleSpinBoxScanZAxisY->setValue(zAxis[1]);
    UI.doubleSpinBoxScanZAxisZ->setValue(zAxis[2]);

    mitk::Surface::Pointer scanPreview = SurfaceGenerator::generateCuboid(
        UI.spinBoxScanDimensionX->value() * UI.spinBoxScanResolutionX->value(),
        UI.spinBoxScanDimensionY->value() * UI.spinBoxScanResolutionY->value(),
        UI.spinBoxScanDimensionZ->value() * UI.spinBoxScanResolutionZ->value(),
        center,
        xAxis,
        yAxis,
        zAxis
    );

    // If the the ground truth volume exists then align the preview and volume render the volume.
    if (scanSimulationVolume.IsNotNull()) {
      mitk::SlicedGeometry3D * scanSlicedGeometry = GetMitkScanVolume()->GetSlicedGeometry();
      mitk::Point3D scanOrigin = scanSlicedGeometry->GetOrigin();
      mitk::AffineTransform3D * scanTransform = scanSlicedGeometry->GetIndexToWorldTransform();

      mitk::BaseGeometry * baseBoxGeometry = scanPreview->GetGeometry();
      baseBoxGeometry->SetOrigin(scanOrigin);
      baseBoxGeometry->SetIndexToWorldTransform(scanTransform);

      scanSimulationVolume->SetProperty("volumerendering", mitk::BoolProperty::New(true));
    }

    mitk::DataNode::Pointer previewBox = SaveDataNode(SCAN_PREVIEW_NAME.c_str(), scanPreview, true);
    previewBox->SetProperty("opacity", mitk::FloatProperty::New(0.5));
  }
}

/**
  * Hide the scan settings preview.
  */
void Sams_View::ScanSimulationRemovePreview() {
  RemoveDataNode(SCAN_PREVIEW_NAME.c_str());
  
  if (scanSimulationVolume.IsNotNull()) {
    scanSimulationVolume->SetProperty("volumerendering", mitk::BoolProperty::New(false));
  }
}

/**
  * Toggles the scan settings preview.
  */
void Sams_View::ScanSimulationTogglePreview(bool checked) {
  scanPreviewEnabled = checked;
  if (scanPreviewEnabled) {
    ScanSimulationPreview();
  }
  else {
    ScanSimulationRemovePreview();
  }
}

/**
  * Simulates the scan.
  */
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
  simulator->setScanResolution(UI.spinBoxScanResolutionX->value(), UI.spinBoxScanResolutionY->value(), UI.spinBoxScanResolutionZ->value());
  simulator->setScanSize(UI.spinBoxScanDimensionX->value(), UI.spinBoxScanDimensionY->value(), UI.spinBoxScanDimensionZ->value());
  simulator->setMotionCorruption(UI.checkBoxMotionCorruptionEnabled->isChecked());
  simulator->setMotionCorruptionMaxAngle(UI.spinBoxMotionCorruptionAngle->value());
  simulator->setScanCenter(center);

  mitk::Image::Pointer sliceStack = simulator->scan();

  // Store it as a DataNode.
  std::ostringstream stackName;
  stackName << UI.lineEditScanName->text().toStdString() << UI.spinBoxScanNameNumber->value();
  SaveDataNode(stackName.str().c_str(), sliceStack, true);

  UI.spinBoxScanNameNumber->setValue(UI.spinBoxScanNameNumber->value() + 1);
}

// ------------------------ //
// ---- Reconstruction ---- //
// ------------------------ //

/**
  * Opens a file picker to locate the reconstruction executable and
  * then builds the UI.
  */
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

/**
  * Creates the list of landmarks used in the optional annotation stage.
  */
void Sams_View::ReconstructInitializeLandmarkList() {
  landmarkNameList = new std::list<std::string>();
  landmarkNameList->push_back("Left Eye");
  landmarkNameList->push_back("Right Eye");
  landmarkNameList->push_back("Thing");
  landmarkNameList->push_back("Something");
  landmarkNameList->push_back("Sausage");
  landmarkNameList->push_back("Croissant");

  numberOfLandmarks = landmarkNameList->size();

  landmarkComboBoxVector = new std::vector<QComboBox *>();

  landmarkPointSetMap = new std::map<unsigned int, mitk::PointSet::Pointer>();

  landmarkIndicatorMap = new std::map<unsigned int, std::vector<QPushButton *> * >();
}

/**
  * Called when the number of stacks being annotated changes.
  */
void Sams_View::ReconstructLandmarksNumStacksChanged(int numStacks) {
  // Cannot set it to less than one.
  if (numStacks < 1) {
    UI.spinBoxMarkLandmarksNumStacks->setValue(this->numSliceStacks);
    return;
  }

  // If we've added a stack(s).
  if (this->numSliceStacks < numStacks) {
    unsigned int stacksToAdd = numStacks - this->numSliceStacks;
    for (unsigned int i = 0; i < stacksToAdd; i++) {
      // Add stack.
      ReconstructLandmarksAddStack(this->numSliceStacks + i);
    }
  }
  // If we've removed a stack(s).
  else {
    unsigned int stacksToRemove = this->numSliceStacks - numStacks;
    for (unsigned int i = 0; i < stacksToRemove; i++) {
      // Remove stack
      ReconstructLandmarksRemoveStack((this->numSliceStacks - 1) - i);
    }
  }

  this->numSliceStacks = numStacks;
}

/**
  * Adds a stack to the annotation toolbox.
  */
void Sams_View::ReconstructLandmarksAddStack(unsigned int index) {
  // Create the widget to put in the toolbox.
  QWidget * widget = new QWidget();
  QVBoxLayout * layout = new QVBoxLayout(widget);
  layout->setContentsMargins(9, 0, 9, 9);
  
  // Add a label/combo box/button to pick which volume this stack is.
  QLabel * titleLable = new QLabel(QString::fromStdString("Pick a stack. Then click 'mark' to begin."));
  titleLable->setStyleSheet(
    "font-weight: bold;"
  );
  layout->addWidget(titleLable);

  QWidget * comboButtonWidget = new QWidget();
  QHBoxLayout * comboButtonLayout = new QHBoxLayout(comboButtonWidget);
  comboButtonLayout->setContentsMargins(0, 0, 0, 0);

  QComboBox * comboBox = new QComboBox();
  comboButtonLayout->addWidget(comboBox);

  QPushButton * pushButton = new QPushButton();
  std::ostringstream buttonName;
  buttonName << "markStack-" << index;
  pushButton->setObjectName(QString::fromStdString(buttonName.str()));
  pushButton->setText("Mark");
  pushButton->setMaximumWidth(60);
  connect(pushButton, SIGNAL(clicked()), this, SLOT(LandmarkingStart()));
  comboButtonLayout->addWidget(pushButton);

  layout->addWidget(comboButtonWidget);

  // Add the combo box to our list of all combo boxes.
  std::vector<QComboBox *>::iterator it = landmarkComboBoxVector->begin();
  landmarkComboBoxVector->insert(it + index, comboBox);  

  // Go through each landmark and add their name and an indicator.
  //new std::map<unsigned int, std::vector<QPushButton *> * >()
  std::vector<QPushButton *> * buttonVector = new std::vector<QPushButton *>();
  landmarkIndicatorMap->insert(std::pair<unsigned int, std::vector<QPushButton *> *>(index, buttonVector));
  int landmarkCounter = 0;
  for (std::list<std::string>::const_iterator iterator = landmarkNameList->begin(); iterator != landmarkNameList->end(); ++iterator) {
    QWidget * innerWidget = new QWidget();
    QHBoxLayout * innerLayout = new QHBoxLayout(innerWidget);
    innerLayout->setContentsMargins(0, 0, 0, 0);

    std::string landmarkName = *iterator;
    QLabel * label = new QLabel(QString::fromStdString(landmarkName));
    innerLayout->addWidget(label);

    QPushButton * statusButton = new QPushButton();
    statusButton->setProperty("class", "");
    //statusButton->setEnabled(false);
    statusButton->setText("--->");
    statusButton->setStyleSheet(
      "QPushButton {"
        "font-size: 7pt;"
        "font-weight: bold;"
        "color: rgb(200, 200, 200);"
        "background-color: rgb(200, 200, 200);"
        "border: 2px solid rgb(128, 128, 128);"
        "max-width: 32px;"
        "max-height: 8px;"
        "min-width: 32px;"
        "min-height: 8px;"
        "border-radius: 4px;"
        "border-style: outset;"
      "}"
      ""
      "QPushButton.set {"
        "border-style: inset;"
        "color: rgb(0, 128, 0);"
        "background-color: rgb(0, 128, 0);"
      "}"
      ""
      "QPushButton.selected {"
        "border-style: inset;"
        "background-color: rgb(0, 128, 0);"
        "color: rgb(255, 255, 255);"
      "}"
      ""
      "QPushButton.next {"
        "border-style: inset;"
        "color: rgb(128, 0, 128);"
        "background-color: rgb(128, 0, 128);"
      "}"
    );
    std::ostringstream statusButtonName;
    statusButtonName << "selectLandmark-" << index << "-" << landmarkCounter;
    statusButton->setObjectName(QString::fromStdString(statusButtonName.str()));
    connect(statusButton, SIGNAL(clicked()), this, SLOT(LandmarkSelect()));
    buttonVector->push_back(statusButton);
    std::cout << "Added button " << statusButton << " to index " << index << std::endl;
    innerLayout->addWidget(statusButton);

    QPushButton * deleteButton = new QPushButton();
    deleteButton->setProperty("class", "");
    //deleteButton->setEnabled(false);
    deleteButton->setStyleSheet(
      "QPushButton {"
      "  background-color: rgb(50, 50, 50);"
      "  border: 2px solid rgb(128, 128, 128);"
      "  max-width: 8px;"
      "  max-height: 8px;"
      "  min-width: 8px;"
      "  min-height: 8px;"
      "  border-radius: 4px;"
      "    border-style: outset;"
      "}"
      ""
      "QPushButton:pressed {"
      "  border-style: inset;"
      "  background-color: rgb(0, 0, 0);"
      "}"
      ""
      "QPushButton:disabled {"
      "  border-style: outset;"
      "  background-color: rgb(200,200, 200);"
      "}"
    );
    std::ostringstream deleteButtonName;
    deleteButtonName << "deleteLandmark-" << index << "-" << landmarkCounter;
    deleteButton->setObjectName(QString::fromStdString(deleteButtonName.str()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(LandmarkDelete()));

    innerLayout->addWidget(deleteButton);

    layout->addWidget(innerWidget);
    landmarkCounter++;
  }

  layout->insertStretch(-1);

  // Add the page to the toolbox.
  std::ostringstream stackName;
  stackName << "Slice Stack #" << index + 1;
  UI.toolBoxMarkLandmarks->insertItem(index, widget, QString::fromStdString(stackName.str()));
}

/**
  * Removes a stack to the annotation toolbox.
  */
void Sams_View::ReconstructLandmarksRemoveStack(unsigned int index) {
  // Get the widget.
  QWidget * widget = UI.toolBoxMarkLandmarks->widget(index);
  if (widget) {
    // Remove it from the tool box.
    UI.toolBoxMarkLandmarks->removeItem(index);
    // Remove it's comboBox from the list of comboBoxes.
    std::vector<QComboBox *>::iterator it = landmarkComboBoxVector->begin();
    landmarkComboBoxVector->erase(it + index);
    // Delete it's layout.
    QLayout *layout = UI.widgetPutGUIHere->layout();
    if (layout != NULL) {
      delete layout;
    }
    // Delete it's children.
    qDeleteAll(widget->children());
    // Delete it.
    delete widget;
  }
}

/**
  * Called when the landmarking for a particular stack is started/continued.
  * The stack being landmarked is determined by the name of the button that
  * called this method.
  */
void Sams_View::LandmarkingStart() {
  // Get the slice stack index that we're landmarking.
  QString buttonName = sender()->objectName();
  QStringList pieces = buttonName.split("-");
  QString indexString = pieces.value(pieces.length() - 1);
  int index = indexString.toInt();
  std::cout << "Starting Landmarking for " << index << std::endl;
  currentLandmarkSliceStack = index;

  mitk::PointSet::Pointer stackPointSet;
  // See if we have a partially completed PointSet.
  std::map<unsigned int, mitk::PointSet::Pointer>::const_iterator it = landmarkPointSetMap->find(index);
  if (it != landmarkPointSetMap->end()) {
    stackPointSet = it->second;
  }
  // If we don't then create one.
  else {
    SamsPointSet::Pointer samsPointSet = SamsPointSet::New();
    samsPointSet->SetSamsView(this);
    stackPointSet = samsPointSet;
    landmarkPointSetMap->insert(std::pair<unsigned int, mitk::PointSet::Pointer>(index, stackPointSet));
  }

  // Set up interactor
  pointSetInteractor = mitk::PointSetDataInteractor::New();
  pointSetInteractor->SetMaxPoints(numberOfLandmarks);
  pointSetInteractor->LoadStateMachine("PointSet.xml");
  pointSetInteractor->SetEventConfig("PointSetConfig.xml");
  pointSetNode = SaveDataNode("Point Set", stackPointSet, true);
  pointSetInteractor->SetDataNode(pointSetNode);

  PointSetChanged(stackPointSet);
}

void Sams_View::LandmarkSelect() {
  QString buttonName = sender()->objectName();
  QStringList pieces = buttonName.split("-");
  QString sliceStackIndexString = pieces.value(pieces.length() - 2);
  QString landmarkIndexString = pieces.value(pieces.length() - 1);
  int sliceStackIndex = sliceStackIndexString.toInt();
  int landmarkIndex = landmarkIndexString.toInt();

  std::cout << "Selecting stack " << sliceStackIndex << " -> landmark " << landmarkIndex << std::endl;

  mitk::PointSet::Pointer pointSet = landmarkPointSetMap->find(sliceStackIndex)->second;
  
  // Deselect all points.
  mitk::Point3D * pointToDeselect = new mitk::Point3D(0);
  for (unsigned int i = 0; i < numberOfLandmarks; i++) {
    if (pointSet->GetPointIfExists(i, pointToDeselect)) {
      mitk::PointOperation* deselectOp = new mitk::PointOperation(mitk::OpDESELECTPOINT, *pointToDeselect, i);
      pointSet->ExecuteOperation(deselectOp);
    }
  }

  // Select one.
  // Create an MITK Operation
  // With OperationType mitk::OpSELECTPOINT
  // Index 'landmarkIndex'
  mitk::Point3D pointToSelect = pointSet->GetPoint(landmarkIndex);
  mitk::PointOperation* selectOp = new mitk::PointOperation(mitk::OpSELECTPOINT, pointToSelect, landmarkIndex);
  pointSet->ExecuteOperation(selectOp);

  std::cout << "Immediately after selection: " << std::endl;
  int selectedPoint = pointSet->SearchSelectedPoint();
  std::cout << "  - Selected Point: " << selectedPoint << std::endl;

  this->RequestRenderWindowUpdate();

  PointSetChanged(pointSet);
}

void Sams_View::LandmarkDelete() {
  QString buttonName = sender()->objectName();
  QStringList pieces = buttonName.split("-");
  QString sliceStackIndexString = pieces.value(pieces.length() - 2);
  QString landmarkIndexString = pieces.value(pieces.length() - 1);
  int sliceStackIndex = sliceStackIndexString.toInt();
  int landmarkIndex = landmarkIndexString.toInt();

  std::cout << "Deleting stack " << sliceStackIndex << " -> landmark " << landmarkIndex << std::endl;

  mitk::PointSet::Pointer pointSet = landmarkPointSetMap->find(sliceStackIndex)->second;
  
  // Create an MITK Operation
  // With OperationType mitk::OpREMOVE
  // Index 'landmarkIndex'
  mitk::Point3D point = pointSet->GetPoint(landmarkIndex);
  mitk::PointOperation* deleteOp = new mitk::PointOperation(mitk::OpREMOVE, point, landmarkIndex);
  pointSet->ExecuteOperation(deleteOp);

  this->RequestRenderWindowUpdate();
}

/**
  * Called by the current point set being created when it is changed.
  */
void Sams_View::PointSetChanged(mitk::PointSet::Pointer pointSet) {
  bool firstNotSet = true;
  std::vector<QPushButton *> * buttons = landmarkIndicatorMap->find(currentLandmarkSliceStack)->second;
  for (unsigned int i = 0; i < numberOfLandmarks; i++) {
    if (pointSet->IndexExists(i)) {
      buttons->at(i)->setProperty("class", "set");
      buttons->at(i)->style()->unpolish(buttons->at(i));
      buttons->at(i)->style()->polish(buttons->at(i));
      buttons->at(i)->update();
      std::cout << "set" << std::endl;
    }
    else {
      if (firstNotSet) {
        firstNotSet = false;
        buttons->at(i)->setProperty("class", "next");
        buttons->at(i)->style()->unpolish(buttons->at(i));
        buttons->at(i)->style()->polish(buttons->at(i));
        buttons->at(i)->update();
        std::cout << "next" << std::endl;        
      }
      else {
        buttons->at(i)->setProperty("class", "");
        buttons->at(i)->style()->unpolish(buttons->at(i));
        buttons->at(i)->style()->polish(buttons->at(i));
        buttons->at(i)->update();
        std::cout << "not set" << std::endl;
      }
    }
  }

  int selectedPoint = pointSet->SearchSelectedPoint();
  std::cout << "Selected Point: " << selectedPoint << std::endl;
  if (selectedPoint != -1) {
    buttons->at(selectedPoint)->setProperty("class", "selected");
    buttons->at(selectedPoint)->style()->unpolish(buttons->at(selectedPoint));
    buttons->at(selectedPoint)->style()->polish(buttons->at(selectedPoint));
    buttons->at(selectedPoint)->update();
  }
}

/**
  * Calls the reconstruction executable.
  */
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

/**
  * Clears the reconstruction UI.
  */
void Sams_View::ClearReconstructionUI() {
  QLayout *layout = UI.widgetPutGUIHere->layout();
  if (layout != NULL) {
    delete layout;
    qDeleteAll(UI.widgetPutGUIHere->children());
  }
  layout = new QVBoxLayout(UI.widgetPutGUIHere);
  layout->setContentsMargins(0, 0, 0, 0);
}

// ----------------------- //
// ---- Visualization ---- //
// ----------------------- //

/**
  * Convenience method to get the scan we're visualizing.
  */
mitk::Image::Pointer Sams_View::GetMitkScan() {
  return Util::MitkImageFromNode(this->scan);
}

/**
  * Convenience method to get the uncertainty we're visualizing.
  */
mitk::Image::Pointer Sams_View::GetMitkUncertainty() {
  return Util::MitkImageFromNode(uncertainty);
}

/**
  * Convenience method to get the preprocessed version of the uncertainty we're visualizing.
  */
mitk::Image::Pointer Sams_View::GetMitkPreprocessedUncertainty() {
  return Util::MitkImageFromNode(preprocessedUncertainty);
}

/**
  * Convenience method to save data (e.g. mitk::Image or mitk::Surface) to the data storage.
  */
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

/**
  * Convenience method to remove a data node from the data storage.
  */
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

/**
  * Show/Hide the current uncertainty we're visualizing.
  */
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

/**
  * Show/Hide the current scan we're visualizing.
  */
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

/**
  * Called when the scan selection box changes.
  */
void Sams_View::ScanDropdownChanged(const QString & scanName) {
  mitk::DataNode::Pointer potentialScan = this->GetDataStorage()->GetNamedNode(scanName.toStdString());
  if (potentialScan.IsNotNull()) {
    // Update the visibility checkbox to match the visibility of the scan we've picked.
    UI.checkBoxScanVisible->setChecked(Util::BoolFromBoolProperty(potentialScan->GetProperty("visible")));
  }
  this->scan = potentialScan;
}

/**
  * Called when the uncertainty selection box changes.
  */
void Sams_View::UncertaintyDropdownChanged(const QString & uncertaintyName) {
  mitk::DataNode::Pointer potentialUncertainty = this->GetDataStorage()->GetNamedNode(uncertaintyName.toStdString());
  if (potentialUncertainty.IsNotNull()) {
    // Update the visibility checkbox to match the visibility of the uncertainty we've picked.
    UI.checkBoxUncertaintyVisible->setChecked(Util::BoolFromBoolProperty(potentialUncertainty->GetProperty("visible")));
  }
  this->uncertainty = potentialUncertainty;
}

/**
  * Updates the options in all the drop-down boxes.
  */
void Sams_View::UpdateSelectionDropDowns() {
  // Remember our previous selection.
  QString scanName = UI.comboBoxScan->currentText();
  QString uncertaintyName = UI.comboBoxUncertainty->currentText();
  QString simulatedScanName = UI.comboBoxSimulateScanVolume->currentText();
  QString surfaceName = UI.comboBoxSurface->currentText();
  std::list<QString> * landmarkNamesRemembered = new std::list<QString>();
  for (std::vector<QComboBox *>::iterator it = landmarkComboBoxVector->begin(); it != landmarkComboBoxVector->end(); it++) {
    landmarkNamesRemembered->push_back((*it)->currentText());
  }

  // Clear all the dropdowns.
  UI.comboBoxScan->clear();
  UI.comboBoxUncertainty->clear();
  UI.comboBoxSimulateScanVolume->clear();
  UI.comboBoxSurface->clear();
  for (std::vector<QComboBox *>::iterator it = landmarkComboBoxVector->begin(); it != landmarkComboBoxVector->end(); it++) {
    (*it)->clear();
  }

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
    for (std::vector<QComboBox *>::iterator it = landmarkComboBoxVector->begin(); it != landmarkComboBoxVector->end(); it++) {
      (*it)->addItem(name);
    }
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

  std::list<QString>::iterator rememberedIt = landmarkNamesRemembered->begin();
  for (std::vector<QComboBox *>::iterator comboIt = landmarkComboBoxVector->begin(); comboIt != landmarkComboBoxVector->end(); comboIt++) {
    int sliceStackStillThere = (*comboIt)->findText(*rememberedIt);
    if (sliceStackStillThere != -1) {
      (*comboIt)->setCurrentIndex(sliceStackStillThere);
    }
    rememberedIt++;
  }

  delete landmarkNamesRemembered;
}

/**
  * Once the scan and uncertainty have been picked and the user confirms they are 
  * correct, we do some pre-processing on the data.
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
      generatedUncertainty = UncertaintyGenerator::generateRandomUncertainty(scanSize);
    }
    // If it's supposed to be a sphere.
    else if (QString::compare(uncertaintyName, SPHERE_NAME) == 0) {
      name << SPHERE_NAME.toStdString(); // "Sphere of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      float half = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 2;
      generatedUncertainty = UncertaintyGenerator::generateSphereUncertainty(scanSize, half);
    }
    // If it's supposed to be a cube.
    else if (QString::compare(uncertaintyName, CUBE_NAME) == 0) {
      name << CUBE_NAME.toStdString(); // "Cube of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      generatedUncertainty = UncertaintyGenerator::generateCubeUncertainty(scanSize, 10);
    }
    // If it's supposed to be a sphere in a quadrant.
    else if (QString::compare(uncertaintyName, QUAD_SPHERE_NAME) == 0) {
      name << QUAD_SPHERE_NAME.toStdString(); // "Quadsphere of Uncertainty (" << scanSize[0] << "x" << scanSize[1] << "x" << scanSize[2] << ")";
      float quarter = std::min(std::min(scanSize[0], scanSize[1]), scanSize[2]) / 4;
      generatedUncertainty = UncertaintyGenerator::generateSphereUncertainty(scanSize, quarter, vtkVector<float, 3>(quarter));
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

// ----------------------- //
// ---- Preprocessing ---- //
// ----------------------- //

/**
  * Restores the settings to the default.
  */
void Sams_View::ResetPreprocessingSettings() {
  UI.checkBoxAligningEnabled->setChecked(false);
  UI.checkBoxInversionEnabled->setChecked(false);
  UI.checkBoxErosionEnabled->setChecked(false);
  UI.spinBoxErodeThickness->setValue(2);
  UI.spinBoxErodeThreshold->setValue(0.2);
  UI.spinBoxDilateThickness->setValue(2);
}

/**
  * Shows/Hides the erosion settings.
  */
void Sams_View::ToggleErosionEnabled(bool checked) {
  UI.widgetVisualizeSelectErodeOptions->setVisible(checked);
}

/**
  * Normalizes, Inverts (if enabled), Erodes (if enabled) the uncertainty and
  * saves the result as a child of the original.
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

// ---------------------- //
// ---- Thresholding ---- //
// ---------------------- //

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

/**
  * Toggles automatic updating of the threshold when a setting changes.
  */
void Sams_View::ToggleUncertaintyThresholdingAutoUpdate(bool checked) {
  thresholdingAutoUpdate = checked;
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

  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UI.sliderMinThreshold->setValue(sliderValue);
  thresholdingEnabled = wasEnabled;
  LowerThresholdChanged(sliderValue);
}

/**
  * Called when the minimum threshold slider moves.
  */
void Sams_View::LowerThresholdSliderMoved(int lower) {
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  LowerThresholdChanged(lower);
  thresholdingEnabled = wasEnabled;
}

/**
  * Called when the lower slider is released.
  */
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

/**
  * Called when the maximum threshold slider moves.
  */
void Sams_View::UpperThresholdSliderMoved(int upper) {
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UpperThresholdChanged(upper);
  thresholdingEnabled = wasEnabled;
}

/**
  * Called when the upper slider is released.
  */
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

void Sams_View::TopOnePercent() {
  TopXPercent(1.0);
}

void Sams_View::TopFivePercent() {
  TopXPercent(5.0);
}

void Sams_View::TopTenPercent() {
  TopXPercent(10.0);
}

/**
  * Called when the top x percent slider changes.
  */
void Sams_View::TopXPercentSliderMoved(double percentage) {
  // Update the spinBox.
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  UI.spinBoxTopXPercent->setValue(percentage);
  thresholdingEnabled = wasEnabled;
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

/**
  * Shows the threshold.
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

/**
  * Hides the threshold.
  */
void Sams_View::RemoveThresholdedUncertainty() {
  RemoveDataNode("Thresholded", preprocessedUncertainty);
}

/**
  * Resets the threshold sliders.
  */
void Sams_View::ResetThresholds() {
  bool wasEnabled = thresholdingEnabled;
  thresholdingEnabled = false;
  SetLowerThreshold(0.0);
  thresholdingEnabled = wasEnabled;
  SetUpperThreshold(1.0);
}

/**
  * Called when a setting is changed. Only actually updates the threshold
  * when auto update is enabled.
  */
void Sams_View::ThresholdUncertaintyIfAutoUpdateEnabled() {
  if (thresholdingAutoUpdate) {
    ThresholdUncertainty();
  }
}

/**
  * Thresholds the uncertainty.
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

// ---------------------------- //
// ---- Uncertainty Sphere ---- //
// ---------------------------- //

/**
  * Called when the horizontal resolution of the sphere changes.
  */
void Sams_View::ThetaResolutionChanged(int value) {
  if (UI.buttonLocked->isChecked()) {
    UI.spinBoxSpherePhiResolution->setValue(value / latLongRatio);
  }

  latLongRatio = UI.spinBoxSphereThetaResolution->value() / UI.spinBoxSpherePhiResolution->value();
}

/**
  * Called when the vertical resolution of the sphere changes.
  */
void Sams_View::PhiResolutionChanged(int value) {
  if (UI.buttonLocked->isChecked()) {
    UI.spinBoxSphereThetaResolution->setValue(value * latLongRatio);
  }

  latLongRatio = UI.spinBoxSphereThetaResolution->value() / UI.spinBoxSpherePhiResolution->value();
}

/**
  * Maps the uncertainty to the surface of a sphere.
  */
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

  bool invertNormals = true;

  SurfaceMapping(surfaceNode, samplingAccumulator, samplingDistance, scaling, colour, registration, invertNormals, false);

  HideAllDataNodes();
  ShowDataNode(surfaceNode);
  this->RequestRenderWindowUpdate();
}

// ----------------------------- //
// ---- Uncertainty Surface ---- //
// ----------------------------- //

/**
  * Maps the uncertainty to the surface selected by the user.
  */
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
  * Takes a surface and maps the uncertainty onto it based on the position and
  * normal vector of each point.
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

// ------------------------- //
// ---- Next Scan Plane ---- //
// ------------------------- //

void Sams_View::NextScanPlaneShowThresholded() {
  thresholdingEnabled = false;
  SetLowerThreshold(0.0);
  thresholdingEnabled = true;
  SetUpperThreshold(UI.spinBoxNextScanPlaneSVDThreshold->value());

  ShowDataNode(this->scanPlane);
  ShowDataNode(this->scanBox);
}

void Sams_View::ComputeNextScanPlane() {
  // Compute the next scan plane.
  SVDScanPlaneGenerator * calculator = new SVDScanPlaneGenerator();
  calculator->setUncertainty(GetMitkPreprocessedUncertainty());
  calculator->setThreshold(UI.spinBoxNextScanPlaneSVDThreshold->value());
  calculator->setIgnoreZeros(UI.checkBoxNextScanPlaneIgnoreZeros->isChecked());
  
  vtkSmartPointer<vtkPlane> plane;
  try {
    plane = calculator->calculateBestScanPlane();
    UI.labelNextScanSVDError->setVisible(false);
  }
  // There were no points that matched the threshold to do SVD with.
 catch (NoPointsException & e) {
    UI.labelNextScanSVDError->setVisible(true);
    return;
  }

  double * center = plane->GetOrigin();
  double * normal = plane->GetNormal();

  vtkVector<float, 3> vectorCenter = vtkVector<float, 3>();
  vectorCenter[0] = center[0];
  vectorCenter[1] = center[1];
  vectorCenter[2] = center[2];

  vtkVector<float, 3> vectorNormal = vtkVector<float, 3>();
  vectorNormal[0] = normal[0];
  vectorNormal[1] = normal[1];
  vectorNormal[2] = normal[2];
  vectorNormal.Normalize();

  // Create a surface to represent it.
  mitk::Surface::Pointer mitkScanPlane = SurfaceGenerator::generateCircle(100, vectorCenter, vectorNormal);

  // Align it with the scan.
  mitk::SlicedGeometry3D * scanSlicedGeometry = GetMitkScan()->GetSlicedGeometry();
  mitk::Point3D scanOrigin = scanSlicedGeometry->GetOrigin();
  mitk::AffineTransform3D * scanTransform = scanSlicedGeometry->GetIndexToWorldTransform();

  mitk::BaseGeometry * basePlaneGeometry = mitkScanPlane->GetGeometry();
  basePlaneGeometry->SetOrigin(scanOrigin);
  basePlaneGeometry->SetIndexToWorldTransform(scanTransform);

  // Save it.
  this->scanPlane = SaveDataNode("Scan Plane", mitkScanPlane, true);
  scanPlane->SetProperty("opacity", mitk::FloatProperty::New(0.75));

  // // Create a cylinder to represent the direction of the scan
  vtkVector<float, 3> scaledNormal = Util::vectorScale(vectorNormal, 150.0f);
  vtkVector<float, 3> startPoint = Util::vectorSubtract(vectorCenter, scaledNormal);
  vtkVector<float, 3> endPoint = Util::vectorAdd(vectorCenter, scaledNormal);
  mitk::Surface::Pointer mitkScanBox = SurfaceGenerator::generateCylinder2(30, startPoint, endPoint);

  // Align it with the scan
  mitk::BaseGeometry * baseBoxGeometry = mitkScanBox->GetGeometry();
  baseBoxGeometry->SetOrigin(scanOrigin);
  baseBoxGeometry->SetIndexToWorldTransform(scanTransform);

  this->scanBox = SaveDataNode("Scan Box", mitkScanBox, true);
  scanBox->SetProperty("opacity", mitk::FloatProperty::New(0.5));

  UI.spinBoxNextBestPointX->setValue(vectorCenter[0]);
  UI.spinBoxNextBestPointY->setValue(vectorCenter[1]);
  UI.spinBoxNextBestPointZ->setValue(vectorCenter[2]);

  UI.spinBoxNextBestNormalX->setValue(vectorNormal[0]);
  UI.spinBoxNextBestNormalY->setValue(vectorNormal[1]);
  UI.spinBoxNextBestNormalZ->setValue(vectorNormal[2]);
}

// ----------------- //
// ---- Options ---- //
// ----------------- //

/**
  * Shows/Hides the crosshairs in the viewing area.
  * If state > 0 then crosshairs are enabled. Otherwise they are disabled.
  */
void Sams_View::ToggleCrosshairs(int state) {
  mitk::ILinkedRenderWindowPart* linkedRenderWindowPart = dynamic_cast<mitk::ILinkedRenderWindowPart*>(this->GetRenderWindowPart());
  if (linkedRenderWindowPart != NULL) {
    linkedRenderWindowPart->EnableSlicingPlanes(state > 0);
  }
}

/**
  * Shows/Hides the legend in the viewing area.
  * If state > 0 then it is enabled. Otherwise it is disabled.
  */
void Sams_View::ToggleLegend(int state) {
  if (state > 0) {
    ShowLegend();
  }
  else {
    HideLegend();
  }
}

/**
  * Resets all the cameras.
  * TODO: Works for the most part but doesn't call reinit on all the datanodes which it appears
  * the reset views button in the viewing area does.
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

// ---------------- //
// ---- Legend ---- //
// ---------------- //

/**
  * Returns the application's instance of the overlay manager.
  */
mitk::OverlayManager::Pointer Sams_View::GetOverlayManager() {
  mitk::ILinkedRenderWindowPart* renderWindowPart = dynamic_cast<mitk::ILinkedRenderWindowPart*>(this->GetRenderWindowPart());
  QmitkRenderWindow * renderWindow = renderWindowPart->GetActiveQmitkRenderWindow();
  mitk::BaseRenderer * renderer = mitk::BaseRenderer::GetInstance(renderWindow->GetVtkRenderWindow());
  return renderer->GetOverlayManager();
}

/**
  * Updates the contents of the legend.
  */
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

/**
  * Makes the legend visible.
  */
void Sams_View::ShowLegend() {
  mitk::OverlayManager::Pointer overlayManager = GetOverlayManager();
  overlayManager->AddOverlay(legendOverlay);
  UI.checkBoxLegend->setChecked(true);
  this->RequestRenderWindowUpdate();
}

/**
  * Hides the legend.
  */
void Sams_View::HideLegend() {
  mitk::OverlayManager::Pointer overlayManager = GetOverlayManager();
  overlayManager->RemoveOverlay(legendOverlay);
  UI.checkBoxLegend->setChecked(false);
  this->RequestRenderWindowUpdate();
}

// ------------ //
// ---- UI ---- //
// ------------ //

/**
  * Sets the layer for a DataNode.
  * Used to show some nodes on top of others.
  */
void Sams_View::SetDataNodeLayer(mitk::DataNode::Pointer node, int layer) {
  mitk::IntProperty::Pointer layerProperty = mitk::IntProperty::New(layer);
  if (node) {
    node->SetProperty("layer", layerProperty);
  }
}

/**
  * Makes a data node visible.
  */
void Sams_View::ShowDataNode(mitk::DataNode::Pointer node) {
  if (node) {
    node->SetProperty("visible", mitk::BoolProperty::New(true));
  }
}

/**
  * Makes a data node invisible.
  */
void Sams_View::HideDataNode(mitk::DataNode::Pointer node) {
  if (node) {
    node->SetProperty("visible", mitk::BoolProperty::New(false));
  }
}

/**
  * Hides all data nodes.
  */
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

/**
  * Shows/Hides the options menu.
  */
void Sams_View::ToggleOptions() {
  UI.widget4Minimizable->setVisible(!UI.widget4Minimizable->isVisible());
}

/**
  * Shows/Hides the debug menu.
  */
void Sams_View::ToggleDebug() {
  UI.widgetDebug->setVisible(!UI.widgetDebug->isVisible());
}

/**
  * Shows the 'Visualize -> Select' UI.
  */
void Sams_View::ShowVisualizeSelect() {
  HideVisualizeAll();
  UI.widgetVisualizeSelect->setVisible(true);
  UI.buttonVisualizeSelect->setChecked(true);
}

/**
  * Shows the 'Visualize -> Threshold' UI.
  */
void Sams_View::ShowVisualizeThreshold() {
  HideVisualizeAll();
  UI.widgetVisualizeThreshold->setVisible(true);
  UI.buttonVisualizeThreshold->setChecked(true);
}

/**
  * Shows the 'Visualize -> Uncertainty Sphere' UI.
  */
void Sams_View::ShowVisualizeSphere() {
  HideVisualizeAll();
  UI.widgetVisualizeSphere->setVisible(true); 
  UI.buttonVisualizeSphere->setChecked(true);  
}

/**
  * Shows the 'Visualize -> Uncertainty Surface' UI.
  */
void Sams_View::ShowVisualizeSurface() {
  HideVisualizeAll();
  UI.widgetVisualizeSurface->setVisible(true); 
  UI.buttonVisualizeSurface->setChecked(true);  
}

/**
  * Shows the 'Visualize -> Next Scan Plane' UI.
  */
void Sams_View::ShowVisualizeNextScanPlane() {
  HideVisualizeAll();
  UI.widgetVisualizeNextScanPlane->setVisible(true); 
  UI.buttonVisualizeNextScanPlane->setChecked(true);  
}

/**
  * Hides all of the Visualize UIs.
  */
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

// --------------- //
// ---- Debug ---- //
// --------------- //
// These methods are just used for convenience when testing or debugging. //

/**
  * Creates random uncertainty with a fixed size.
  */
void Sams_View::GenerateRandomUncertainty() {
  vtkVector<float, 3> uncertaintySize = vtkVector<float, 3>(50);
  mitk::Image::Pointer random = UncertaintyGenerator::generateRandomUncertainty(uncertaintySize);
  SaveDataNode("Random Uncertainty", random);
}

/**
  * Creates cube random uncertainty with a fixed size.
  */
void Sams_View::GenerateCubeUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  mitk::Image::Pointer cube = UncertaintyGenerator::generateCubeUncertainty(imageSize, 10);
  SaveDataNode("Cube Uncertainty", cube);
}

/**
  * Creates sphere uncertainty with a fixed size.
  */
void Sams_View::GenerateSphereUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  mitk::Image::Pointer sphere = UncertaintyGenerator::generateSphereUncertainty(imageSize, std::min(std::min(imageSize[0], imageSize[1]), imageSize[2]) / 2);
  SaveDataNode("Sphere Uncertainty", sphere);
}

/**
  * Creates quadrant sphere uncertainty with a fixed size.
  */
void Sams_View::GenerateQuadrantSphereUncertainty() {
  vtkVector<float, 3> imageSize = vtkVector<float, 3>(50);
  float quarter = std::min(std::min(imageSize[0], imageSize[1]), imageSize[2]) / 4;
  mitk::Image::Pointer quadsphere = UncertaintyGenerator::generateSphereUncertainty(imageSize, quarter, vtkVector<float, 3>(quarter));
  SaveDataNode("Quadsphere Uncertainty", quadsphere);
}

/**
  * A convenient method (attached to button 1 in the debug menu) to test out functionality.
  */
void Sams_View::debug1() {
  
}

mitk::PointSetDataInteractor::Pointer m_CurrentInteractor;
mitk::PointSet::Pointer m_TestPointSet;
mitk::DataNode::Pointer m_TestPointSetNode;

/**
  * A convenient method (attached to button 2 in the debug menu) to test out functionality.
  */
void Sams_View::debug2() {
  // Set up interactor
  m_CurrentInteractor = mitk::PointSetDataInteractor::New();
  m_CurrentInteractor->LoadStateMachine("PointSet.xml");
  m_CurrentInteractor->SetEventConfig("PointSetConfig.xml");
  //Create new PointSet which will receive the interaction input
  m_TestPointSet = SamsPointSet::New();
  // Add the point set to the mitk::DataNode *before* the DataNode is added to the mitk::PointSetDataInteractor
  m_TestPointSetNode = SaveDataNode("Point Set", m_TestPointSet, true);
  // finally add the mitk::DataNode (which already is added to the mitk::DataStorage) to the mitk::PointSetDataInteractor
  m_CurrentInteractor->SetDataNode(m_TestPointSetNode);
}

// ------------------- //
// ---- Inherited ---- //
// ------------------- //

/**
  * What to do when the plugin window is selected.
  */
void Sams_View::SetFocus() {
  // Focus on something useful?
  //    e.g. UI.buttonOverlayText->setFocus();
}

/**
  * What to do when a data node or selection changes.
  */
void Sams_View::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList<mitk::DataNode::Pointer>& /*nodes*/) { 
  UpdateSelectionDropDowns();
}

// -------------------- //
// ---- Deprecated ---- //
// -------------------- //

/**
  * Instead of volume rendering the binary thresholded version of the uncertainty this applies the
  * same volume rendering to the uncertainty itself. It wasn't as clear and so isn't used.
  */
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