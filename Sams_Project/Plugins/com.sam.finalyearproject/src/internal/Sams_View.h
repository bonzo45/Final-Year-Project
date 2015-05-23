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

#ifndef Sams_View_h
#define Sams_View_h

#include <berryISelectionListener.h>

#include <QmitkAbstractView.h>

#include "ui_Sams_ViewControls.h"

// Classes required for method definitions.
#include <mitkImage.h>
#include <vtkVector.h>
#include <mitkOverlayManager.h>
#include "UncertaintySurfaceMapper.h"
#include "UncertaintyThresholder.h"
#include "ColourLegendOverlay.h"
#include <mitkPointSet.h>
#include <mitkPointSetDataInteractor.h>
#include <ctkCmdLineModuleManager.h>
#include <ctkCmdLineModuleBackend.h>
#include <ctkCmdLineModuleFrontendFactory.h>

/*!
  \brief Sams_View
    Sam's Final Year Project. An MITK plugin for visualizing uncertainty in MRI scans.
    
  \sa QmitkFunctionality
  \ingroup ${plugin_target}_internal
*/
class Sams_View : public QmitkAbstractView {  
  Q_OBJECT
  
  public:
    static const std::string VIEW_ID;
    virtual void CreateQtPartControl(QWidget *parent);

    // Callback for SamsPointSet.
    void PointSetChanged(mitk::PointSet::Pointer pointSet);

  protected slots:
    void Initialize();

    // ------------------------- //
    // ---- SCAN SIMULATION ---- //
    // ------------------------- //
    mitk::Image::Pointer GetMitkScanVolume();
    void ScanSimulationDropdownChanged(const QString & scanName);
    void ScanSimulationSetPointCenter();
    void ScanSimulationSetDirectionAxial();
    void ScanSimulationSetDirectionCoronal();
    void ScanSimulationSetDirectionSagittal();
    void ScanSimulationResetRotations();
    void ScanSimulationRotateX(int angle);
    void ScanSimulationRotateY(int angle);
    void ScanSimulationRotateZ(int angle);
    void ScanSimulationPreview();
    void ScanSimulationRemovePreview();
    void ScanSimulationTogglePreview(bool checked);
    void ScanSimulationSimulateScan();

    // ------------------------ //
    // ---- RECONSTRUCTION ---- //
    // ------------------------ //
    void ReconstructGUI();
    void ReconstructInitializeLandmarkList();
    void ReconstructLandmarksNumStacksChanged(int numStacks);
    void ReconstructLandmarksAddStack(unsigned int index);
    void ReconstructLandmarksRemoveStack(unsigned int index);
    void LandmarkingStart();
    void LandmarkSelect();
    void LandmarkDelete();
    void ClearReconstructionUI();
    void ReconstructGo();
    
    // ----------------------- //
    // ---- VISUALIZATION ---- //
    // ----------------------- //
    // Convenience functions...
    mitk::Image::Pointer GetMitkScan();
    mitk::Image::Pointer GetMitkUncertainty();
    mitk::Image::Pointer GetMitkPreprocessedUncertainty();
    mitk::DataNode::Pointer SaveDataNode(const char * name, mitk::BaseData * data, bool overwrite = false, mitk::DataNode::Pointer parent = 0);
    void RemoveDataNode(const char * name, mitk::DataNode::Pointer parent = 0);
    void ToggleScanVisible(bool checked);
    void ToggleUncertaintyVisible(bool checked);

    // Picking files...
    void ScanDropdownChanged(const QString & scanName);
    void UncertaintyDropdownChanged(const QString & scanName);
    void UpdateSelectionDropDowns();
    void ConfirmSelection();

    // Preprocessing...
    void ResetPreprocessingSettings();
    void ToggleErosionEnabled(bool checked);
    void PreprocessNode(mitk::DataNode::Pointer node);

    // ---- Thresholding ---- //
    void ToggleUncertaintyThresholding(bool checked);  
    void ToggleUncertaintyThresholdingAutoUpdate(bool checked);

    void SetLowerThreshold(double);
    void LowerThresholdSliderMoved(int lower);
    void LowerThresholdChanged();
    void LowerThresholdChanged(int lower);

    void SetUpperThreshold(double);
    void UpperThresholdSliderMoved(int upper);
    void UpperThresholdChanged();
    void UpperThresholdChanged(int upper);

    void TopOnePercent();
    void TopFivePercent();
    void TopTenPercent();
    void TopXPercentSliderMoved(double percentage);
    void TopXPercent();
    void TopXPercent(double percentage);

    void DisplayThreshold();
    void RemoveThresholdedUncertainty();
    void ResetThresholds();

    void ThresholdUncertaintyIfAutoUpdateEnabled();
    void ThresholdUncertainty();

    // ---- Uncertainty Sphere ---- //
    void ThetaResolutionChanged(int);
    void PhiResolutionChanged(int);
    void GenerateUncertaintySphere();

    // ---- Uncertainty Surface ---- //
    void SurfaceMapping();
    void SurfaceMapping(
      mitk::DataNode::Pointer surfaceNode,
      UncertaintySurfaceMapper::SAMPLING_ACCUMULATOR samplingAccumulator,
      UncertaintySurfaceMapper::SAMPLING_DISTANCE samplingDistance,
      UncertaintySurfaceMapper::SCALING scaling,
      UncertaintySurfaceMapper::COLOUR colour,
      UncertaintySurfaceMapper::REGISTRATION registration,
      bool invertNormals,
      bool debugRegistration = false
    );

    // ---- Next Scan Plane ---- //
    void NextScanPlaneShowThresholded();
    void ComputeNextScanPlane();

    // ----------------- //
    // ---- Options ---- //
    // ----------------- //
    void ToggleCrosshairs(int state);
    void ToggleLegend(int state);
    void ResetViews();

    // ---------------- //
    // ---- Legend ---- //
    // ---------------- //
    mitk::OverlayManager::Pointer GetOverlayManager();
    void SetLegend(double value1, char * colour1, double value2, char * colour2);
    void ShowLegend();
    void HideLegend();

    // ------------ //
    // ---- UI ---- //
    // ------------ //
    void SetDataNodeLayer(mitk::DataNode::Pointer node, int layer);
    void ShowDataNode(mitk::DataNode::Pointer node);
    void HideDataNode(mitk::DataNode::Pointer node);
    void HideAllDataNodes();
    
    void ToggleOptions();
    void ToggleDebug();

    void ShowVisualizeSelect();
    void ShowVisualizeThreshold();
    void ShowVisualizeSphere();
    void ShowVisualizeSurface();
    void ShowVisualizeNextScanPlane();
    void HideVisualizeAll();
    
    // --------------- //
    // ---- Debug ---- //
    // --------------- //
    void GenerateRandomUncertainty();
    void GenerateCubeUncertainty();
    void GenerateSphereUncertainty();
    void GenerateQuadrantSphereUncertainty();
    void debug1();
    void debug2();

  protected:  
    virtual void SetFocus();
    virtual void OnSelectionChanged(berry::IWorkbenchPart::Pointer source, const QList<mitk::DataNode::Pointer>& nodes);

    Ui::Sams_ViewControls UI;

  private:
    // ------------------------- //
    // ---- SCAN SIMULATION ---- //
    // ------------------------- //
    // Scan Simulation Volume 
    mitk::DataNode::Pointer scanSimulationVolume;
    bool scanPreviewEnabled;

    // Direction of scan.
    vtkVector<float, 3> directionX;
    vtkVector<float, 3> directionY;
    vtkVector<float, 3> directionZ;

    // Rotation of scan.
    int rotationX;
    int rotationY;
    int rotationZ;

    // ------------------------ //
    // ---- RECONSTRUCTION ---- //
    // ------------------------ //
    ctkCmdLineModuleManager* moduleManager;
    ctkCmdLineModuleBackend* processBackend;
    ctkCmdLineModuleFrontend* frontend;

    std::list<std::string> * landmarkNameList;
    unsigned int numberOfLandmarks;
    unsigned int numSliceStacks;
    std::vector<QComboBox *> * landmarkComboBoxVector;
    std::map<unsigned int, std::vector<QPushButton *> *> * landmarkIndicatorMap;
    std::map<unsigned int, mitk::PointSet::Pointer> * landmarkPointSetMap;
    unsigned int currentLandmarkSliceStack;
    unsigned int currentLandmark;

    mitk::PointSetDataInteractor::Pointer pointSetInteractor;
    mitk::DataNode::Pointer pointSetNode;

    // ----------------------- //
    // ---- VISUALIZATION ---- //
    // ----------------------- //
    // Scan
    mitk::DataNode::Pointer scan;
    unsigned int scanHeight;
    unsigned int scanWidth;
    unsigned int scanDepth;

    // Uncertainty
    mitk::DataNode::Pointer uncertainty;
    mitk::DataNode::Pointer preprocessedUncertainty;
    unsigned int uncertaintyHeight;
    unsigned int uncertaintyWidth;
    unsigned int uncertaintyDepth;

    // Preprocessing
    static const double NORMALIZED_MAX = 1.0;
    static const double NORMALIZED_MIN = 0.0;

    // Thresholding
    UncertaintyThresholder * thresholder;
    mitk::DataNode::Pointer thresholdedUncertainty = 0;
    bool thresholdingEnabled = false;
    bool thresholdingAutoUpdate = true;
    double lowerThreshold = 0;
    double upperThreshold = 0;

    // Uncertainty Sphere
    double latLongRatio = 2.0;

    // Next Scan Plane
    mitk::DataNode::Pointer scanPlane;
    mitk::DataNode::Pointer scanBox;

    // Overlay
    ColourLegendOverlay * legendOverlay = NULL;

    // -------------------- //
    // ---- Deprecated ---- //
    // -------------------- //
    void VolumeRenderThreshold(bool checked);
};

#endif // Sams_View_h