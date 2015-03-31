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
#include <mitkIRenderingManager.h>
#include <mitkTimeSlicedGeometry.h>

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
#include <mitkImageCast.h>
#include <itkMinimumMaximumImageCalculator.h>

// for sphere
#include <vtkSphereSource.h>
#include <vtkTextureMapToSphere.h>
#include <mitkSurface.h>
#include <mitkSmartPointerProperty.h>
#include <mitkNodePredicateDataType.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
// for sphere texture
#include <itkImage.h>
#include <mitkITKImageImport.h>
#include <cstdlib>
#include <cmath>
#include <mitkShaderProperty.h>
#include <mitkImagePixelReadAccessor.h>

// for model mapping
#include <vtkIdList.h>
#include <vtkCellArray.h>
#include <vtkMath.h>

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

float minUncertaintyIntensity = 0;
float maxUncertaintyIntensity = 0;
float minUncertaintyThreshold = 0;
float maxUncertaintyThreshold = 0;

mitk::DataNode::Pointer thresholdedUncertainty = 0;

typedef itk::Image<unsigned char, 2>  TextureImageType;
typedef itk::Image<unsigned char, 3>  UncertaintyImageType;

TextureImageType::Pointer uncertaintyTexture = TextureImageType::New();
UncertaintyImageType::Pointer cubeUncertainty;
UncertaintyImageType::Pointer randomUncertainty;
UncertaintyImageType::Pointer sphereUncertainty;

bool thresholdingEnabled = false;

/**
  * Create the UI
  */
void Sams_View::CreateQtPartControl(QWidget *parent) {
  // Create Qt UI
  UI.setupUi(parent);

  // Add click handler.
  connect(UI.buttonSwapScanUncertainty, SIGNAL(clicked()), this, SLOT(SwapScanUncertainty()));
  connect(UI.buttonOverlayText, SIGNAL(clicked()), this, SLOT(ShowTextOverlay()));
  connect(UI.buttonSetLayers, SIGNAL(clicked()), this, SLOT(SetLayers()));
  connect(UI.radioButtonEnableThreshold, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyThresholding(bool)));
  connect(UI.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(UI.sliderMinThreshold, SIGNAL(sliderMoved (int)), this, SLOT(LowerThresholdChanged(int)));
  connect(UI.sliderMaxThreshold, SIGNAL(sliderMoved (int)), this, SLOT(UpperThresholdChanged(int)));
  connect(UI.buttonSphere, SIGNAL(clicked()), this, SLOT(ShowMeASphere()));
  connect(UI.buttonResetViews, SIGNAL(clicked()), this, SLOT(ResetViews()));
  connect(UI.buttonRandomUncertainty, SIGNAL(clicked()), this, SLOT(GenerateRandomUncertainty()));
  connect(UI.buttonCubeUncertainty, SIGNAL(clicked()), this, SLOT(GenerateCubeUncertainty()));
  connect(UI.buttonSphereUncertainty, SIGNAL(clicked()), this, SLOT(GenerateSphereUncertainty()));
  connect(UI.buttonBrainSurfaceTest, SIGNAL(clicked()), this, SLOT(BrainSurfaceTest()));

  SetNumberOfImagesSelected(0);
}

/**
  * What to do when focus is set.
  */
void Sams_View::SetFocus() {
  // Focus on the button.
  //UI.buttonOverlayText->setFocus();
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

void Sams_View::ScanPicked(bool picked) {
  if (picked) {

  }
  else {
    UI.labelScanName->setText("Pick a Scan (Ctrl + Click)");
  }
}

void Sams_View::UncertaintyPicked(bool picked) {
  if (picked) {
    UI.radioButtonEnableThreshold->setEnabled(true);
    UI.sliderMinThreshold->setEnabled(true);
    UI.sliderMaxThreshold->setEnabled(true);
    UI.buttonSphere->setEnabled(true);
  }
  else {
    UI.labelUncertaintyName->setText("Pick an Uncertainty (Ctrl + Click)");
    UI.radioButtonEnableThreshold->setEnabled(false);
    UI.sliderMinThreshold->setEnabled(false);
    UI.sliderMaxThreshold->setEnabled(false);
    UI.buttonSphere->setEnabled(false);
  }
}

void Sams_View::BothPicked(bool picked) {
  if (picked) {
    UI.buttonSwapScanUncertainty->setEnabled(true);
    UI.buttonSetLayers->setEnabled(true);
  }
  else {
    UI.buttonSwapScanUncertainty->setEnabled(false);
    UI.buttonSetLayers->setEnabled(false);
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
  * Toggles Thresholding
  */
void Sams_View::ToggleUncertaintyThresholding(bool checked) {
  if (checked) {
    cout << "Checked." << endl;
    thresholdingEnabled = true;
    ThresholdUncertainty();
  }
  else {
    cout << "NOT Checked." << endl;
    thresholdingEnabled = false;
  }
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
  thresholdedUncertainty->SetProperty("color", mitk::ColorProperty::New(1.0, 0.0, 0.0));
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
    UI.sliderMinThreshold->setValue(UI.sliderMaxThreshold->value());
    return;
  }
  minUncertaintyThreshold = temp;
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << minUncertaintyThreshold;
  UI.labelSliderLeft->setText(ss.str().c_str());

  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
}

/**
  * Display Upper Threshold
  */
void Sams_View::UpperThresholdChanged(int upper) {
  float temp = ((maxUncertaintyIntensity - minUncertaintyIntensity) / 1000) * upper + minUncertaintyIntensity;
  if (temp < minUncertaintyThreshold) {
    UI.sliderMaxThreshold->setValue(UI.sliderMinThreshold->value());
    return;
  }
  maxUncertaintyThreshold = temp;
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << maxUncertaintyThreshold;
  UI.labelSliderRight->setText(ss.str().c_str());

  if (thresholdingEnabled) {
    ThresholdUncertainty();
  }
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

  UI.labelScanName->setText(QString::fromStdString(name));
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
  UI.labelUncertaintyName->setText(QString::fromStdString(name));

  // Calculate intensity range.
  mitk::BaseData * uncertaintyData = uncertainty->GetData();
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertaintyData);
  AccessByItk_2(uncertaintyImage, ItkGetRange, minUncertaintyIntensity, maxUncertaintyIntensity);

  UI.sliderMinThreshold->setRange(0, 1000);
  UI.sliderMinThreshold->setValue(0);
  LowerThresholdChanged(0);
  UI.sliderMaxThreshold->setRange(0, 1000);
  UI.sliderMaxThreshold->setValue(1000);
  UpperThresholdChanged(1000);

  // Update the labels.
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << minUncertaintyIntensity;
  UI.labelSliderLeftLimit->setText(ss.str().c_str());
  ss.str("");
  ss << std::setprecision(2) << std::fixed << maxUncertaintyIntensity;
  UI.labelSliderRightLimit->setText(ss.str().c_str());
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

/**
  * Resets all the cameras.
  */
void Sams_View::ResetViews() {
  // Compute optimal bounds.
  const mitk::TimeGeometry::Pointer bounds = this->GetDataStorage()->ComputeVisibleBoundingGeometry3D();

  // Set them.
  mitk::IRenderWindowPart* renderWindowPart = this->GetRenderWindowPart();
  mitk::IRenderingManager* renderManager = renderWindowPart->GetRenderingManager();
  renderManager->InitializeViews(bounds);
}

/**
  * Creates a sphere and maps a texture created from uncertainty to it.
  */
void Sams_View::ShowMeASphere() {
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
  mitk::DataNode::Pointer textureNode = mitk::DataNode::New();
  textureNode->SetData(textureImage);
  textureNode->SetProperty("name", mitk::StringProperty::New("Uncertainty Texture"));
  textureNode->SetProperty("layer", mitk::IntProperty::New(3));
  textureNode->SetProperty("color", mitk::ColorProperty::New(0.0, 1.0, 0.0));
  textureNode->SetProperty("opacity", mitk::FloatProperty::New(1.0));
  this->GetDataStorage()->Add(textureNode);

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
  mitk::DataNode::Pointer surfaceNode = mitk::DataNode::New();
  surfaceNode->SetData(surfaceToPutTextureOn);
  surfaceNode->SetProperty("name", mitk::StringProperty::New("Uncertainty Sphere"));

  mitk::SmartPointerProperty::Pointer textureProperty = mitk::SmartPointerProperty::New(textureImage);
  surfaceNode->SetProperty("Surface.Texture", textureProperty);
  surfaceNode->SetProperty("layer", mitk::IntProperty::New(3));
  surfaceNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));

  this->GetDataStorage()->Add(surfaceNode);
}

void Sams_View::GenerateRandomUncertainty() {
  GenerateRandomUncertainty(50);
}

void Sams_View::GenerateCubeUncertainty() {
  GenerateCubeUncertainty(50, 10);
}

void Sams_View::GenerateSphereUncertainty() {
  GenerateSphereUncertainty(50, 30);
}

/**
  * Generates uncertainty data (size * size * size).
  * Each voxel is a random uncertainty value between 0 and 255.
  */
void Sams_View::GenerateRandomUncertainty(unsigned int size) {
  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = size;
  uncertaintySize[1] = size;
  uncertaintySize[2] = size;
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  randomUncertainty = UncertaintyImageType::New();
  randomUncertainty->SetRegions(region);
  randomUncertainty->Allocate();

  // Go through each voxel and set a random value.
  for (unsigned int r = 0; r < size; r++) {
    for (unsigned int c = 0; c < size; c++) {
      for (unsigned int d = 0; d < size; d++) {
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
  
  mitk::DataNode::Pointer randomUncertaintyNode = mitk::DataNode::New();
  randomUncertaintyNode->SetData(mitkImage);
  randomUncertaintyNode->SetProperty("name", mitk::StringProperty::New("Random Uncertainty"));
  this->GetDataStorage()->Add(randomUncertaintyNode);
}

/**
  * Generates uncertainty data (totalSize * totalSize * totalSize).
  * It's zero everywhere, apart from a cube placed at the center with size cubeSize that is 1.
  */
void Sams_View::GenerateCubeUncertainty(unsigned int totalSize, unsigned int cubeSize) {
  // Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = totalSize;
  uncertaintySize[1] = totalSize;
  uncertaintySize[2] = totalSize;
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  cubeUncertainty = UncertaintyImageType::New();
  cubeUncertainty->SetRegions(region);
  cubeUncertainty->Allocate();

  // Make sure the cube is within the total size.
  if (cubeSize > totalSize) {
    cubeSize = totalSize;
  }

  // Compute the cube location. At first to float precision, then round to pixels.
  float halfCubeWidth = (float) cubeSize / 2.0;
  float uncertaintyCenter = (((float) (totalSize - 1)) / 2.0);
  unsigned int cubeStartPixel = ceil(uncertaintyCenter - halfCubeWidth);
  unsigned int cubeEndPixel = floor(uncertaintyCenter + halfCubeWidth);

  // Go through each voxel and set uncertainty according to whether it's in the cube.
  for (unsigned int r = 0; r < totalSize; r++) {
    for (unsigned int c = 0; c < totalSize; c++) {
      for (unsigned int d = 0; d < totalSize; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        // If this pixel is in the cube.
        if ((cubeStartPixel <= r) && (r <= cubeEndPixel) &&
            (cubeStartPixel <= c) && (c <= cubeEndPixel) &&
            (cubeStartPixel <= d) && (d <= cubeEndPixel)) {
          cubeUncertainty->SetPixel(pixelIndex, 255);
        }
        // If it's not.
        else {
          cubeUncertainty->SetPixel(pixelIndex, 0);
        }
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(cubeUncertainty);
  
  mitk::DataNode::Pointer cubeUncertaintyNode = mitk::DataNode::New();
  cubeUncertaintyNode->SetData(mitkImage);
  cubeUncertaintyNode->SetProperty("name", mitk::StringProperty::New("Cube of Uncertainty"));
  this->GetDataStorage()->Add(cubeUncertaintyNode);
}

/**
  * Generates uncertainty data (totalSize * totalSize * totalSize).
  * It's zero everywhere, apart from a sphere of radius sphereRadius that has uncertainty 255 at the center and fades linearly to the edges.
  */
void Sams_View::GenerateSphereUncertainty(unsigned int totalSize, unsigned int sphereRadius) {
// Create a blank ITK image.
  UncertaintyImageType::RegionType region;
  UncertaintyImageType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
 
  UncertaintyImageType::SizeType uncertaintySize;
  uncertaintySize[0] = totalSize;
  uncertaintySize[1] = totalSize;
  uncertaintySize[2] = totalSize;
 
  region.SetSize(uncertaintySize);
  region.SetIndex(start);

  sphereUncertainty = UncertaintyImageType::New();
  sphereUncertainty->SetRegions(region);
  sphereUncertainty->Allocate();

  // Make sure the sphere is within the total size.
  if (sphereRadius > totalSize) {
    sphereRadius = totalSize;
  }

  cout << sphereRadius << "/" << totalSize << endl;

  // Calculate center of sphere.
  float sphereCenter = (float) (totalSize - 1) / 2.0;

  // Go through each voxel and weight uncertainty by distance from center of sphere.
  for (unsigned int r = 0; r < totalSize; r++) {
    for (unsigned int c = 0; c < totalSize; c++) {
      for (unsigned int d = 0; d < totalSize; d++) {
        UncertaintyImageType::IndexType pixelIndex;
        pixelIndex[0] = r;
        pixelIndex[1] = c;
        pixelIndex[2] = d;

        // Compute distance from center.
        float distanceFromCenter = 
          sqrt(
              pow(((float) r - sphereCenter), 2) +
              pow(((float) c - sphereCenter), 2) + 
              pow(((float) d - sphereCenter), 2)
          );

        // Get normalized 0-1 weighting.
        float uncertaintyValue = std::max(0.0f, ((float) sphereRadius - distanceFromCenter) / (float) sphereRadius);

        //cout << "Distance: " << distanceFromCenter << ", Uncertainty: " << uncertaintyValue << endl;

        // Scale by 255.
        sphereUncertainty->SetPixel(pixelIndex, uncertaintyValue * 255);
        
      }
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(sphereUncertainty);
  
  mitk::DataNode::Pointer sphereUncertaintyNode = mitk::DataNode::New();
  sphereUncertaintyNode->SetData(mitkImage);
  sphereUncertaintyNode->SetProperty("name", mitk::StringProperty::New("Sphere of Uncertainty"));
  this->GetDataStorage()->Add(sphereUncertaintyNode);
}

/**
  * Generates a texture that represents the uncertainty of the uncertainty volume.
  * It works by projecting a point in the center of the volume outwards, onto a sphere.
  */
mitk::Image::Pointer Sams_View::GenerateUncertaintyTexture() {
  unsigned int width = 150;
  unsigned int height = 50;

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

  // Get the uncertainty data to sample.
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertainty->GetData());
  unsigned int dimensions = uncertaintyImage->GetDimension();
  unsigned int uncertaintyHeight = uncertaintyImage->GetDimension(0);
  unsigned int uncertaintyWidth = uncertaintyImage->GetDimension(1);
  unsigned int uncertaintyDepth = uncertaintyImage->GetDimension(2);
  cout << dimensions << "D: " << uncertaintyHeight << ", " << uncertaintyWidth << ", " << uncertaintyDepth << endl;

  // Use an image accessor to read values from the image, rather than the image itself.
  try  {
    mitk::ImagePixelReadAccessor<unsigned char, 3> readAccess(uncertaintyImage);
    
    // Compute texture.
    for (unsigned int r = 0; r < height; r++) {
      for (unsigned int c = 0; c < width; c++) {
        bool print;

        if (r == 25)
          print = true;
        else
          print = false;

        if (print)
          cout << "(r, c): (" << r << ", " << c << ")" << endl;
        // Compute spherical coordinates, phi, theta from pixel value.
        // Latitude
        float theta = ((float) r / (float) height) * M_PI;
        // Longitude
        float phi = ((float) c / (float) width) * (2 * M_PI);
        if (print)
          cout << "- (theta, phi): " << "(" << theta << ", " << phi << ")" << endl;

        // Compute point on sphere with radius 1. This is also the vector from the center of the sphere to the point.
        float xDir = cos(phi) * sin(theta);
        float yDir = cos(theta);
        float zDir = sin(phi) * sin(theta);
        if (print)
          cout << "- (x, y, z): " << "(" << xDir << ", " << yDir << ", " << zDir << ")" << endl;
        
        float normalization = sqrt(pow(xDir, 2) + pow(yDir, 2) + pow(zDir, 2));
        xDir /= normalization;
        yDir /= normalization;
        zDir /= normalization;
        
        if (print)
          cout << "- n(x, y, z): " << "(" << xDir << ", " << yDir << ", " << zDir << ")" << endl;
        
        // Compute the center of the uncertainty data.
        float x = ((float) uncertaintyHeight - 1) / 2.0;
        float y = ((float) uncertaintyWidth - 1) / 2.0;
        float z = ((float) uncertaintyDepth - 1) / 2.0;
        if (print)
          cout << "- center: " << x << ", " << y << ", " << z << endl;

        // Start at the center and follow the vector outwards whilst sampling points.
        double uncertaintyAccumulator = 0;
        unsigned int numSamples = 0;
        while (x <= uncertaintyHeight - 1 && x >= 0 && y <= uncertaintyWidth - 1 && y >= 0 && z <= uncertaintyDepth - 1 && z >= 0) {
          // Sample the neighbourhood about a point.
          double totalAccumulator = 0.0;
          double distanceAccumulator = 0.0;
          for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) { 
              for (int k = -1; k <= 1; k++) {
                int xNeighbour = round(x) + i;
                int yNeighbour = round(y) + j;
                int zNeighbour = round(z) + k;

                // If we're on the edge and the neighbour doesn't exist, skip it.
                if (xNeighbour < 0 || (int) uncertaintyHeight <= xNeighbour ||
                    yNeighbour < 0 || (int) uncertaintyWidth <= yNeighbour ||
                    zNeighbour < 0 || (int) uncertaintyDepth <= zNeighbour) {
                  continue;
                }

                itk::Index<3> index;
                index[0] = xNeighbour;
                index[1] = yNeighbour;
                index[2] = zNeighbour;

                float neighbourSample = readAccess.GetPixelByIndex(index);

                float distanceToSample = 
                  sqrt(
                    pow((x - xNeighbour), 2) +
                    pow((y - yNeighbour), 2) + 
                    pow((z - zNeighbour), 2)
                  );

                if (distanceToSample == 0.0) {
                  totalAccumulator = neighbourSample;
                  distanceAccumulator = 1;
                  goto BREAK_ALL_LOOPS;
                }

                distanceAccumulator += 1.0 / distanceToSample;
                totalAccumulator += neighbourSample / distanceToSample;
              }
            }
          }
          BREAK_ALL_LOOPS:

          if (print) {
            cout << "Distance Accumulator: " << distanceAccumulator << endl;
            cout << "Total Accumulator: " << totalAccumulator << endl;
          }
          double sample = totalAccumulator / distanceAccumulator;

          if (print)
            cout << "-- sample: " << "(" << x << ", " << y << ", " << z << "): " << sample << endl;

          // Add sample, but ignore if it is background.
          if (sample != 0.0) {
            // Accumulate result.
            uncertaintyAccumulator += sample;

            // Count how many samples we've taken.
            numSamples++;
          }

          // Move along.
          x += xDir;
          y += yDir;
          z += zDir;
        }

        if (print) {
          cout << "- uncertaintyAccumulator: " << uncertaintyAccumulator << endl;
          cout << "- numSamples: " << numSamples << endl;
        }

        int pixelValue = round(uncertaintyAccumulator / numSamples);
        if (print)
          cout << "- pixel value: " << pixelValue << endl;
        // Set texture value.
        TextureImageType::IndexType pixelIndex;
        pixelIndex[0] = c;
        pixelIndex[1] = r;

        if (c == 0) {
          pixelValue = 1;
        }
        else if (c == round(width / 4)) {
          pixelValue = 255;
        }

        uncertaintyTexture->SetPixel(pixelIndex, pixelValue);
      }
    }

    // Convert from ITK to MITK.
    mitk::Image::Pointer mitkImage = mitk::ImportItkImage(uncertaintyTexture);
    return mitkImage;
  }
  catch (mitk::Exception & e) {
    cerr << "Hmmm... it appears we can't get access to the uncertainty image." << e << endl;
    return NULL;
  }
}

/**
  * Takes a surface and maps the uncertainty onto it based on the normal vector.
  */
void Sams_View::BrainSurfaceTest() {
  // Get the surface.
  mitk::DataNode::Pointer brainModelNode = this->GetDataStorage()->GetNode(mitk::NodePredicateDataType::New("Surface"));
  if (brainModelNode == (void*)NULL) {
    cout << "No Surface Found." << endl;
    return;
  }

  // Cast it to an MITK surface.
  mitk::BaseData * brainModelData = brainModelNode->GetData();
  mitk::Surface::Pointer brainModelSurface = dynamic_cast<mitk::Surface*>(brainModelData);

  // Extract the vtkPolyData.
  vtkPolyData * brainModelVtk = brainModelSurface->GetVtkPolyData();

  // Randomly colour each point: red, green or blue.
  unsigned char red[3] = {255, 0, 0};
  unsigned char green[3] = {0, 255, 0};
  unsigned char blue[3] = {0, 0, 255};
 
  // Generate a list of colours - one for each point.
  vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
  colors->SetNumberOfComponents(3);
  colors->SetName ("Colors");
  for (vtkIdType i = 0; i < brainModelVtk->GetNumberOfPoints(); i++) {
    int random = rand() % 3;
    switch (random) {
      case 0:
        colors->InsertNextTupleValue(red);
        break;
      case 1:
        colors->InsertNextTupleValue(green);
        break;
      case 2:
        colors->InsertNextTupleValue(blue);
        break;
    }
  }
  
  // Set the colours to be the scalar value of each point.
  brainModelVtk->GetPointData()->SetScalars(colors);
  cout << "Well, that works." << endl;
}