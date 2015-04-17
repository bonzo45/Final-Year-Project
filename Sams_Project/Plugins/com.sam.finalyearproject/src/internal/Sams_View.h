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

/*!
  \brief Sams_View

  \warning  This class is not yet documented. Use "git blame" and ask the author to provide basic documentation.

  \sa QmitkFunctionality
  \ingroup ${plugin_target}_internal
*/
class Sams_View : public QmitkAbstractView {  
  // Needed for all Qt objects that want signals/slots.
  Q_OBJECT
  
  public:  
    static const std::string VIEW_ID;
    virtual void CreateQtPartControl(QWidget *parent);

  protected slots:
    // 1
    void ScanDropdownChanged(const QString & scanName);
    void UncertaintyDropdownChanged(const QString & scanName);
    void ConfirmSelection();
    void ToggleScanVisible(bool checked);
    void ToggleUncertaintyVisible(bool checked);

    // 3
    //  a
    void ToggleUncertaintyThresholding(bool checked);    
    void ThresholdUncertainty();
    void LowerThresholdChanged(int lower);
    void UpperThresholdChanged(int upper);
    void TopOnePercent();
    void TopFivePercent();
    void TopTenPercent();
    void TopXPercent(int percentage);
    void OverlayThreshold();

    //  b
    void TextureWidthChanged(int);
    void TextureHeightChanged(int);
    void GenerateUncertaintySphere();

    //  c
    void GenerateSphereSurface();
    void GenerateCubeSurface();
    void GenerateCylinderSurface();
    void SurfaceMapping();

    // 4
    void ToggleCrosshairs(int state);
    void ResetViews();

    // 5

    // UI
    void ToggleMinimize1();
    void ToggleMinimize2();
    void ToggleMinimize3();
    void ToggleMinimize4();
    void ToggleErosionEnabled(bool checked);

    // Debug
    void ToggleDebug();
    void GenerateRandomUncertainty();
    void GenerateCubeUncertainty();
    void GenerateSphereUncertainty();
    void GenerateQuadrantSphereUncertainty();

  protected:  
    virtual void SetFocus();
    virtual void OnSelectionChanged(berry::IWorkbenchPart::Pointer source, const QList<mitk::DataNode::Pointer>& nodes);

    Ui::Sams_ViewControls UI;

  private:
    // General
    void InitializeUI();
    mitk::DataNode::Pointer SaveDataNode(const char * name, mitk::BaseData * data, bool overwrite = false, mitk::DataNode::Pointer parent = 0);
    
    // 1
    void UpdateSelectionDropDowns();
    void PreprocessNode(mitk::DataNode::Pointer node);

    // 2
    //  a
    void SetLowerThreshold(double);
    void SetUpperThreshold(double);

    //  b
    mitk::Image::Pointer GenerateUncertaintyTexture();

    // c
};

#endif // Sams_View_h