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
    void ConfirmSelection();
    void SwapScanUncertainty();
    void ToggleScanVisible(bool checked);
    void ToggleUncertaintyVisible(bool checked);

    // 2
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
    void SurfaceMapping();

    // 3
    void ToggleCrosshairs(int state);
    void ResetViews();

    // 4
    void ShowTextOverlay();

    // 5
    void GenerateRandomUncertainty();
    void GenerateCubeUncertainty();
    void GenerateSphereUncertainty();
    void GenerateQuadrantSphereUncertainty();

  protected:  
    virtual void SetFocus();
    virtual void OnSelectionChanged(berry::IWorkbenchPart::Pointer source, const QList<mitk::DataNode::Pointer>& nodes);

    Ui::Sams_ViewControls UI;

  private:
    // Util
    mitk::DataNode::Pointer SaveDataNode(const char * name, mitk::BaseData * data, bool overwrite = false, mitk::DataNode::Pointer parent = 0);

    // 1
    void SelectScan(mitk::DataNode::Pointer scan);
    void SelectUncertainty(mitk::DataNode::Pointer uncertainty);
    void SetNumberOfImagesSelected(int imagesSelected);
    void ScanSelected(bool test);
    void UncertaintySelected(bool test);
    void BothSelected(bool test);
    void PreprocessNode(mitk::DataNode::Pointer node);

    template <typename TPixel, unsigned int VImageDimension>
    void ItkNormalizeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkInvertUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result);

    // 2
    //  a
    void SetLowerThreshold(double);
    void SetUpperThreshold(double);

    template <typename TPixel, unsigned int VImageDimension>
    void ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, double min, double max, mitk::Image::Pointer & result);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkTopXPercent(itk::Image<TPixel, VImageDimension>* itkImage, double percentage, double & lowerThreshold, double & upperThreshold);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkErodeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, int erodeThickness, double threshold, mitk::Image::Pointer & result);

    //  b
    mitk::Image::Pointer GenerateUncertaintyTexture();
    double SampleUncertainty(vtkVector<float, 3> startPoint, vtkVector<float, 3> direction, int percentage = 100);
    double InterpolateUncertaintyAtPosition(vtkVector<float, 3> position);

    //  c

    // 3

    // 4

    // 5
    void GenerateRandomUncertainty(unsigned int height, unsigned int width, unsigned int depth);
    void GenerateCubeUncertainty(unsigned int height, unsigned int width, unsigned int depth, unsigned int cubeSize);
    void GenerateSphereUncertainty(vtkVector<float, 3> imageSize, unsigned int sphereRadius, vtkVector<float, 3> sphereCenter = vtkVector<float, 3>(-1.0f));
};

#endif // Sams_View_h