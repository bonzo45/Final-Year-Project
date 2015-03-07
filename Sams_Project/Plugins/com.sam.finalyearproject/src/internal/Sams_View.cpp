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

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

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
  connect(m_Controls.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));

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
    m_Controls.scanLabel->setText("Pick a Scan (Ctrl + Click)");
  }
  if (imagesSelected <= 1) {
    m_Controls.uncertaintyLabel->setText("Pick an Uncertainty (Ctrl + Click)");
    m_Controls.buttonSwapScanUncertainty->setEnabled(false);
  }
  else {
    m_Controls.buttonSwapScanUncertainty->setEnabled(true);
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

void Sams_View::SwapScanUncertainty() {
  mitk::DataNode::Pointer temp = this->scan;
  this->SetScan(this->uncertainty);
  this->SetUncertainty(temp);
}

void Sams_View::SetScan(mitk::DataNode::Pointer scan) {
  this->scan = scan;

  mitk::BaseProperty * nameProperty = scan->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();

  m_Controls.scanLabel->setText(QString::fromStdString(name));
}

void Sams_View::SetUncertainty(mitk::DataNode::Pointer uncertainty) {
  this->uncertainty = uncertainty;

  mitk::BaseProperty * nameProperty = uncertainty->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();

  m_Controls.uncertaintyLabel->setText(QString::fromStdString(name));
}

// QList<mitk::DataNode::Pointer> nodes = this->GetDataManagerSelection();
// if (nodes.empty()) {
//   return;
// }

// mitk::DataNode* node = nodes.front();

// if (!node) {
//   // Nothing selected. Inform the user and return
//   QMessageBox::information( NULL, "Template", "Please load and select an image before starting image processing.");
//   return;
// }

// // here we have a valid mitk::DataNode

// // a node itself is not very useful, we need its data item (the image)
// mitk::BaseData* data = node->GetData();
// if (data) {
//   // test if this data item is an image or not (could also be a surface or something totally different)
//   mitk::Image* image = dynamic_cast<mitk::Image*>(data);
//   if (image) {
//     std::stringstream message;
//     std::string name;
//     message << "Performing image processing for image ";
//     if (node->GetName(name)) {
//       // a property called "name" was found for this DataNode
//       message << "'" << name << "'";
//     }
//     message << ".";
//     MITK_INFO << message.str();

//     // -----------------------------
//     // actually do something here...
//     // -----------------------------
//   }
// }