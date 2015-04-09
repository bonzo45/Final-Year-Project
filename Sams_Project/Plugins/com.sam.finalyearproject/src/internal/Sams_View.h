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

// Stuff I've included.
#include <mitkImage.h>
#include <vtkVector.h>
// #include "itkBinaryBallStructuringElement.h"
// #include "itkGrayscaleErodeImageFilter.h"


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
    void SwapScanUncertainty();

    // 2
    //  a
    void ThresholdUncertainty();
    void ToggleUncertaintyThresholding(bool checked);    
    void LowerThresholdChanged(int lower);
    void UpperThresholdChanged(int upper);
    void ErodeUncertainty();
    void TopTenPercent();

    //  b
    void GenerateUncertaintySphere();

    //  c
    void GenerateSphereSurface();
    void SurfaceMapping();

    // 3
    void ToggleCrosshairs(int state);
    void ResetViews();

    // 4
    void ShowTextOverlay();
    void SetLayers();

    // 5
    void GenerateRandomUncertainty();
    void GenerateCubeUncertainty();
    void GenerateSphereUncertainty();
    void GenerateQuadrantSphereUncertainty();

  protected:  
    virtual void SetFocus();
    virtual void OnSelectionChanged(berry::IWorkbenchPart::Pointer source, const QList<mitk::DataNode::Pointer>& nodes);

    // The current scan and uncertainty images.
    mitk::DataNode::Pointer scan;
    mitk::DataNode::Pointer uncertainty;

    Ui::Sams_ViewControls UI;

  private:
    // 1
    void SetScan(mitk::DataNode::Pointer scan);
    void SetUncertainty(mitk::DataNode::Pointer uncertainty);
    void SetNumberOfImagesSelected(int imagesSelected);
    void ScanPicked(bool test);
    void UncertaintyPicked(bool test);
    void BothPicked(bool test);

    // 2
    //  a
    template <typename TPixel, unsigned int VImageDimension>
    void ItkGetRange(itk::Image<TPixel, VImageDimension>* itkImage, float &min, float &max);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension >* itkImage, float min, float max);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkErodeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkTopTenPercent(itk::Image<TPixel, VImageDimension>* itkImage);

    //  b
    mitk::Image::Pointer GenerateUncertaintyTexture();
    int SampleUncertainty(vtkVector<float, 3> startPoint, vtkVector<float, 3> direction);

    //  c

    // 3

    // 4

    // 5
    void GenerateRandomUncertainty(unsigned int size);
    void GenerateCubeUncertainty(unsigned int totalSize, unsigned int cubeSize);
    void GenerateSphereUncertainty(unsigned int totalSize, unsigned int sphereRadius, vtkVector<float, 3> sphereCenter = vtkVector<float, 3>(-1.0f));
};

#endif // Sams_View_h