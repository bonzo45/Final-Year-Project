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

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qmitk
#include "Sams_View.h"

// Qt
#include <QMessageBox>

// Stuff I've included.
#include <mitkBaseProperty.h>

// for interacting with the render window
#include <mitkIRenderWindowPart.h>
#include <mitkILinkedRenderWindowPart.h>

// for volume rendering
#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>

// Operation 'Overlay'
#include <mitkBaseRenderer.h>
#include <QmitkRenderWindow.h>
#include <mitkOverlayManager.h>
#include <mitkTextOverlay2D.h>

// for thresholding
#include <mitkImageAccessByItk.h>
#include <itkBinaryThresholdImageFilter.h>
#include "mitkImageCast.h"
#include <itkMinimumMaximumImageCalculator.h>

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

float minUncertaintyIntensity = 0;
float maxUncertaintyIntensity = 0;
float minUncertaintyThreshold = 0;
float maxUncertaintyThreshold = 0;

mitk::DataNode::Pointer thresholdedUncertainty = 0;

/**
  * Create the UI
  */
void Sams_View::CreateQtPartControl(QWidget *parent) {
  // Create Qt UI
  m_Controls.setupUi(parent);

  // Add click handler.
  connect(m_Controls.buttonSwapScanUncertainty, SIGNAL(clicked()), this, SLOT(SwapScanUncertainty()));
  connect(m_Controls.buttonOverlayText, SIGNAL(clicked()), this, SLOT(ShowTextOverlay()));
  connect(m_Controls.buttonSetLayers, SIGNAL(clicked()), this, SLOT(SetLayers()));
  connect(m_Controls.buttonThreshold, SIGNAL(clicked()), this, SLOT(ThresholdUncertainty()));
  connect(m_Controls.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(m_Controls.sliderMinThreshold, SIGNAL(sliderMoved (int)), this, SLOT(LowerThresholdChanged(int)));
  connect(m_Controls.sliderMaxThreshold, SIGNAL(sliderMoved (int)), this, SLOT(UpperThresholdChanged(int)));

  SetNumberOfImagesSelected(0);
}

/**
  * What to do when focus is set.
  */
void Sams_View::SetFocus() {
  // Focus on the button.
  //m_Controls.buttonOverlayText->setFocus();
}

/**
  * What to do when a data node or selection changes.
  */
void Sams_View::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList<mitk::DataNode::Pointer>& nodes) { 
  int imagesSelected = 0;

  // Go through all of the nodes, checking if one is an image.
  foreach(mitk::DataNode::Pointer node, nodes) {
    // If it's an image.
    mitk::Image::Pointer image = dynamic_cast<mitk::Image*>(node->GetData());
    if(node.IsNotNull() && image) {
      imagesSelected++;

      // Get it's name.
      if (imagesSelected == 1) {
        SetScan(node);

      }
      else if (imagesSelected == 2) {
        SetUncertainty(node);
      }

      // Volume Render it!
      node->SetProperty("volumerendering", mitk::BoolProperty::New(true));
      
      // Create a transfer function.
      mitk::TransferFunction::Pointer tf = mitk::TransferFunction::New();
      tf->InitializeByMitkImage(image);

      // Colour Transfer Function: AddRGBPoint(double x, double r, double g, double b)
      tf->GetColorTransferFunction()->AddRGBPoint(tf->GetColorTransferFunction()->GetRange() [0], 1.0, 0.0, 0.0);
      tf->GetColorTransferFunction()->AddRGBPoint(tf->GetColorTransferFunction()->GetRange() [1], 1.0, 1.0, 0.0);

      // Opacity Transfer Function: AddPoint(double x, double y)
      tf->GetScalarOpacityFunction()->AddPoint(0, 0);
      tf->GetScalarOpacityFunction()->AddPoint(tf->GetColorTransferFunction()->GetRange() [1], 1);

      node->SetProperty ("TransferFunction", mitk::TransferFunctionProperty::New(tf.GetPointer()));
    }
  }

  SetNumberOfImagesSelected(imagesSelected);
}

/**
  * Updates the UI to reflect the number of images selected.
  * Shows messages, disables buttons etc.
  */
void Sams_View::SetNumberOfImagesSelected(int imagesSelected) {
  if (imagesSelected == 0) {
    ScanPicked(false);
    UncertaintyPicked(false);
    BothPicked(false);
  }
  else if (imagesSelected == 1) {
    ScanPicked(true);
    UncertaintyPicked(false);
    BothPicked(false);
  }
  else {
    ScanPicked(true);
    UncertaintyPicked(true);
    BothPicked(true);
  }
}

void Sams_View::ScanPicked(bool test) {
  if (test) {

  }
  else {
    m_Controls.labelScanName->setText("Pick a Scan (Ctrl + Click)");
  }
}

void Sams_View::UncertaintyPicked(bool test) {
  if (test) {
    m_Controls.buttonThreshold->setEnabled(true);
    m_Controls.sliderMinThreshold->setEnabled(true);
    m_Controls.sliderMaxThreshold->setEnabled(true);
  }
  else {
    m_Controls.labelUncertaintyName->setText("Pick an Uncertainty (Ctrl + Click)");
    m_Controls.buttonThreshold->setEnabled(false);
    m_Controls.sliderMinThreshold->setEnabled(false);
    m_Controls.sliderMaxThreshold->setEnabled(false);
  }
}

void Sams_View::BothPicked(bool test) {
  if (test) {
    m_Controls.buttonSwapScanUncertainty->setEnabled(true);
    m_Controls.buttonSetLayers->setEnabled(true);
  }
  else {
    m_Controls.buttonSwapScanUncertainty->setEnabled(false);
    m_Controls.buttonSetLayers->setEnabled(false);
  }
}

/**
  * Sets the Uncertainty to be above the Scan.
  */
void Sams_View::SetLayers() {
  mitk::IntProperty::Pointer behindProperty = mitk::IntProperty::New(0);
  mitk::IntProperty::Pointer infrontProperty = mitk::IntProperty::New(1);

  this->scan->SetProperty("layer", behindProperty);
  this->uncertainty->SetProperty("layer", infrontProperty);

  this->RequestRenderWindowUpdate();
}

/**
  * Show Overlay
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

/**
  * Creates a copy of the uncertainty data with all values less than a threshold removed.
  */
void Sams_View::ThresholdUncertainty() {
  mitk::DataNode::Pointer uncertaintyNode = this->uncertainty;
  mitk::BaseData * uncertaintyData = uncertaintyNode->GetData();
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertaintyData);

  AccessByItk_2(uncertaintyImage, ItkThresholdUncertainty, minUncertaintyThreshold, maxUncertaintyThreshold);
}

/**
  * Uses ITK to do the thresholding.
  */
template <typename TPixel, unsigned int VImageDimension>
void Sams_View::ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, float min, float max) {
  typedef itk::Image<TPixel, VImageDimension> ImageType;
  typedef itk::BinaryThresholdImageFilter<ImageType, ImageType> BinaryThresholdImageFilterType;
  
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
  mitk::Image::Pointer resultImage;
  mitk::CastToMitkImage(thresholdedImage, resultImage);

  // Wrap it up in a Data Node
  if (thresholdedUncertainty) {
    this->GetDataStorage()->Remove(thresholdedUncertainty);
  }
  thresholdedUncertainty = mitk::DataNode::New();
  thresholdedUncertainty->SetData(resultImage);
  thresholdedUncertainty->SetProperty("binary", mitk::BoolProperty::New(true));
  thresholdedUncertainty->SetProperty("name", mitk::StringProperty::New("Uncertainty Thresholded"));
  thresholdedUncertainty->SetProperty("color", mitk::ColorProperty::New(1.0,0.0,0.0));
  thresholdedUncertainty->SetProperty("volumerendering", mitk::BoolProperty::New(true));
  thresholdedUncertainty->SetProperty("layer", mitk::IntProperty::New(1));
  thresholdedUncertainty->SetProperty("opacity", mitk::FloatProperty::New(0.5));
  this->GetDataStorage()->Add(thresholdedUncertainty);

  this->RequestRenderWindowUpdate();
}

/**
  * Display Lower Threshold
  */
void Sams_View::LowerThresholdChanged(int lower) {
  
  float temp = ((maxUncertaintyIntensity - minUncertaintyIntensity) / 1000) * lower + minUncertaintyIntensity;
  if (temp > maxUncertaintyThreshold) {
    m_Controls.sliderMinThreshold->setValue(m_Controls.sliderMaxThreshold->value());
    return;
  }
  minUncertaintyThreshold = temp;
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << minUncertaintyThreshold;
  m_Controls.labelSliderLeft->setText(ss.str().c_str());

  ThresholdUncertainty();
}

/**
  * Display Upper Threshold
  */
void Sams_View::UpperThresholdChanged(int upper) {
  float temp = ((maxUncertaintyIntensity - minUncertaintyIntensity) / 1000) * upper + minUncertaintyIntensity;
  if (temp < minUncertaintyThreshold) {
    m_Controls.sliderMaxThreshold->setValue(m_Controls.sliderMinThreshold->value());
    return;
  }
  maxUncertaintyThreshold = temp;
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << maxUncertaintyThreshold;
  m_Controls.labelSliderRight->setText(ss.str().c_str());

  ThresholdUncertainty();
}

/**
  * If state > 0 then crosshairs are enabled. Otherwise they are disabled.
  */
void Sams_View::ToggleCrosshairs(int state) {
  // Disable Crosshairs.
  mitk::ILinkedRenderWindowPart* linkedRenderWindowPart = dynamic_cast<mitk::ILinkedRenderWindowPart*>(this->GetRenderWindowPart());
  if (linkedRenderWindowPart != NULL) {
    linkedRenderWindowPart->EnableSlicingPlanes(state > 0);
  }
}

/**
  * Swaps the scan and the uncertainty data over.
  */
void Sams_View::SwapScanUncertainty() {
  mitk::DataNode::Pointer temp = this->scan;
  this->SetScan(this->uncertainty);
  this->SetUncertainty(temp);
}

/**
  * Sets the scan to work with.
  */
void Sams_View::SetScan(mitk::DataNode::Pointer scan) {
  this->scan = scan;

  mitk::BaseProperty * nameProperty = scan->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();

  m_Controls.labelScanName->setText(QString::fromStdString(name));
}

/**
  * Sets the uncertainty to work with.
  */
void Sams_View::SetUncertainty(mitk::DataNode::Pointer uncertainty) {
  this->uncertainty = uncertainty;

  // Update Label
  mitk::BaseProperty * nameProperty = uncertainty->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();
  m_Controls.labelUncertaintyName->setText(QString::fromStdString(name));

  // Calculate intensity range.
  mitk::BaseData * uncertaintyData = uncertainty->GetData();
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertaintyData);
  AccessByItk_2(uncertaintyImage, ItkGetRange, minUncertaintyIntensity, maxUncertaintyIntensity);

  m_Controls.sliderMinThreshold->setRange(0, 1000);
  m_Controls.sliderMinThreshold->setValue(0);
  LowerThresholdChanged(0);
  m_Controls.sliderMaxThreshold->setRange(0, 1000);
  m_Controls.sliderMaxThreshold->setValue(1000);
  UpperThresholdChanged(1000);

  // Update the labels.
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << minUncertaintyIntensity;
  m_Controls.labelSliderLeftLimit->setText(ss.str().c_str());
  ss.str("");
  ss << std::setprecision(2) << std::fixed << maxUncertaintyIntensity;
  m_Controls.labelSliderRightLimit->setText(ss.str().c_str());
  cout << "Here!!!" << ss.str().c_str() << endl;
}

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