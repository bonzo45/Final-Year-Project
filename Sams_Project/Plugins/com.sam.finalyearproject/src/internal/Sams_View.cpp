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
#include <mitkNodePredicateProperty.h>

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
#include <vtkFloatArray.h>

const std::string Sams_View::VIEW_ID = "org.mitk.views.sams_view";

// ------------------ //
// ---- TYPEDEFS ---- //
// ------------------ //
typedef itk::Image<unsigned char, 2>  TextureImageType;
typedef itk::Image<unsigned char, 3>  UncertaintyImageType;

// ------------------------- //
// ---- GLOBAL VARIABLES --- //
// ------------------------- //

// Uncertainty dimensions.
unsigned int uncertaintyHeight;
unsigned int uncertaintyWidth;
unsigned int uncertaintyDepth;

// 2a. Uncertainty Thresholding
mitk::DataNode::Pointer thresholdedUncertainty = 0;
bool thresholdingEnabled = false;
float minUncertaintyIntensity = 0;
float maxUncertaintyIntensity = 0;
float minUncertaintyThreshold = 0;
float maxUncertaintyThreshold = 0;

// 2b. Uncertainty Sphere
TextureImageType::Pointer uncertaintyTexture = TextureImageType::New();

// 5. Test Uncertainties
UncertaintyImageType::Pointer cubeUncertainty;
UncertaintyImageType::Pointer randomUncertainty;
UncertaintyImageType::Pointer sphereUncertainty;

// ----------------------- //
// ---- IMPLEMENTATION --- //
// ----------------------- //

/**
  * Create the UI
  */
void Sams_View::CreateQtPartControl(QWidget *parent) {
  // Create Qt UI
  UI.setupUi(parent);

  // Add event handlers.
  // 1. Select Scan & Uncertainty
  connect(UI.buttonSwapScanUncertainty, SIGNAL(clicked()), this, SLOT(SwapScanUncertainty()));

  // 2.
  //  a. Thresholding
  connect(UI.radioButtonEnableThreshold, SIGNAL(toggled(bool)), this, SLOT(ToggleUncertaintyThresholding(bool)));
  connect(UI.sliderMinThreshold, SIGNAL(sliderMoved (int)), this, SLOT(LowerThresholdChanged(int)));
  connect(UI.sliderMaxThreshold, SIGNAL(sliderMoved (int)), this, SLOT(UpperThresholdChanged(int)));

  //  b. Uncertainty Sphere
  connect(UI.buttonSphere, SIGNAL(clicked()), this, SLOT(GenerateUncertaintySphere()));

  // 3. Options
  connect(UI.checkBoxCrosshairs, SIGNAL(stateChanged(int)), this, SLOT(ToggleCrosshairs(int)));
  connect(UI.buttonResetViews, SIGNAL(clicked()), this, SLOT(ResetViews()));

  // 4. Random
  connect(UI.buttonSetLayers, SIGNAL(clicked()), this, SLOT(SetLayers()));
  connect(UI.buttonOverlayText, SIGNAL(clicked()), this, SLOT(ShowTextOverlay()));
  connect(UI.buttonBrainSurfaceTest, SIGNAL(clicked()), this, SLOT(BrainSurfaceTest()));
  connect(UI.buttonSphereSurface, SIGNAL(clicked()), this, SLOT(GenerateSphereSurface()));

  // 5. Test Uncertainties
  connect(UI.buttonRandomUncertainty, SIGNAL(clicked()), this, SLOT(GenerateRandomUncertainty()));
  connect(UI.buttonCubeUncertainty, SIGNAL(clicked()), this, SLOT(GenerateCubeUncertainty()));
  connect(UI.buttonSphereUncertainty, SIGNAL(clicked()), this, SLOT(GenerateSphereUncertainty()));

  SetNumberOfImagesSelected(0);
}

/**
  * What to do when the plugin window is selected.
  */
void Sams_View::SetFocus() {
  // Focus on something useful?
  //    e.g. UI.buttonOverlayText->setFocus();
}

// ---------------------------------------------------------- //
// ---- I really shouldn't have to have written these... ---- //
// ---------------------------------------------------------- //

vtkVector<float, 3> vectorAdd(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] + b[0];
  c[1] = a[1] + b[1];
  c[2] = a[2] + b[2];
  return c;
}

vtkVector<float, 3> vectorSubtract(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  c[2] = a[2] - b[2];
  return c;
}

// ----------- //
// ---- 1 ---- //
// ----------- //

/**
  * Swaps the scan and the uncertainty.
  */
void Sams_View::SwapScanUncertainty() {
  mitk::DataNode::Pointer temp = this->scan;
  this->SetScan(this->uncertainty);
  this->SetUncertainty(temp);
}

/**
  * Sets the scan we're working with.
  */
void Sams_View::SetScan(mitk::DataNode::Pointer scan) {
  this->scan = scan;

  mitk::BaseProperty * nameProperty = scan->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();

  UI.labelScanName->setText(QString::fromStdString(name));
}

/**
  * Sets the uncertainty we're working with.
  */
void Sams_View::SetUncertainty(mitk::DataNode::Pointer uncertainty) {
  this->uncertainty = uncertainty;

  // Update name label.
  mitk::BaseProperty * nameProperty = uncertainty->GetProperty("name");
  mitk::StringProperty::Pointer nameStringProperty = dynamic_cast<mitk::StringProperty*>(nameProperty);
  std::string name = nameStringProperty->GetValueAsString();
  UI.labelUncertaintyName->setText(QString::fromStdString(name));

  // Calculate dimensions.
  mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertainty->GetData());
  uncertaintyHeight = uncertaintyImage->GetDimension(0);
  uncertaintyWidth = uncertaintyImage->GetDimension(1);
  uncertaintyDepth = uncertaintyImage->GetDimension(2);

  // 2a. Thresholding - Calculate Intensity Range
  AccessByItk_2(uncertaintyImage, ItkGetRange, minUncertaintyIntensity, maxUncertaintyIntensity);

  UI.sliderMinThreshold->setRange(0, 1000);
  UI.sliderMinThreshold->setValue(0);
  LowerThresholdChanged(0);
  UI.sliderMaxThreshold->setRange(0, 1000);
  UI.sliderMaxThreshold->setValue(1000);
  UpperThresholdChanged(1000);

  // Update thresholding sliders.
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << minUncertaintyIntensity;
  UI.labelSliderLeftLimit->setText(ss.str().c_str());
  ss.str("");
  ss << std::setprecision(2) << std::fixed << maxUncertaintyIntensity;
  UI.labelSliderRightLimit->setText(ss.str().c_str());
}

/**
  * What to do when a data node or selection changes.
  */
void Sams_View::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*source*/, const QList<mitk::DataNode::Pointer>& nodes) { 
  int imagesSelected = 0;

  // Go through all of the nodes that have been selected.
  foreach(mitk::DataNode::Pointer node, nodes) {
    // If it's an image.
    mitk::Image::Pointer image = dynamic_cast<mitk::Image*>(node->GetData());
    if(node.IsNotNull() && image) {
      imagesSelected++;

      // Set the first image selected to be the scan.
      if (imagesSelected == 1) {
        SetScan(node);
      }
      // Set the second one to be the uncertainty.
      else if (imagesSelected == 2) {
        SetUncertainty(node);
      }
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

/**
  * Enables/Disables parts of the UI based on whether we have selected a scan.
  */
void Sams_View::ScanPicked(bool picked) {
  if (picked) {

  }
  else {
    UI.labelScanName->setText("Pick a Scan (Ctrl + Click)");
  }
}

/**
  * Enables/Disables parts of the UI based on whether we have selected an uncertainty.
  */
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

/**
  * Enables/Disables parts of the UI based on whether we have selected both a scan and an uncertainty.
  */
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

// ------------ //
// ---- 2a ---- //
// ------------ //

/**
  * Toggles thresholding
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
  * Creates a copy of the uncertainty data with values not between minUncertaintyThreshold and maxUncertaintyThreshold removed.
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

// ------------ //
// ---- 2b ---- //
// ------------ //

/**
  * Creates a sphere and maps a texture created from uncertainty to it.
  */
void Sams_View::GenerateUncertaintySphere() {
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
  surfaceNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  surfaceNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));

  this->GetDataStorage()->Add(surfaceNode);
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

  // Compute center of uncertainty data.
  vtkVector<float, 3> center = vtkVector<float, 3>();
  center[0] = ((float) uncertaintyHeight - 1) / 2.0;
  center[1] = ((float) uncertaintyWidth - 1) / 2.0;
  center[2] = ((float) uncertaintyDepth - 1) / 2.0;

  // For each pixel in the texture, sample the uncertainty data.
  for (unsigned int r = 0; r < height; r++) {
    for (unsigned int c = 0; c < width; c++) {
      // Compute spherical coordinates: phi (longitude) & theta (latitude).
      float theta = ((float) r / (float) height) * M_PI;
      float phi = ((float) c / (float) width) * (2 * M_PI);
      
      // Compute point on sphere with radius 1. This is also the vector from the center of the sphere to the point.
      vtkVector<float, 3> direction = vtkVector<float, 3>();
      direction[0] = cos(phi) * sin(theta);
      direction[1] = cos(theta);
      direction[2] = sin(phi) * sin(theta);
      direction.Normalize();

      // Sample the uncertainty data.
      int pixelValue = SampleUncertainty(center, direction);

      // Set texture value.
      TextureImageType::IndexType pixelIndex;
      pixelIndex[0] = c;
      pixelIndex[1] = r;

      uncertaintyTexture->SetPixel(pixelIndex, pixelValue);
    }
  }

  // Convert from ITK to MITK.
  mitk::Image::Pointer mitkImage = mitk::ImportItkImage(uncertaintyTexture);
  return mitkImage;
}

/**
  * Returns the average uncertainty along a vector in the uncertainty.
  * startPosition - the vector to begin tracing from
  * direction - the vector of the direction to trace in
  */
int Sams_View::SampleUncertainty(vtkVector<float, 3> startPosition, vtkVector<float, 3> direction) {
  // Use an image accessor to read values from the uncertainty.
  try  {
    mitk::Image::Pointer uncertaintyImage = dynamic_cast<mitk::Image*>(uncertainty->GetData());
    mitk::ImagePixelReadAccessor<unsigned char, 3> readAccess(uncertaintyImage);

    // Start at the startPosition, moving along.
    vtkVector<float, 3> position = vtkVector<float, 3>(startPosition);

    double uncertaintyAccumulator = 0;
    unsigned int numSamples = 0;
    while (0 <= position[0] && position[0] <= uncertaintyHeight - 1 &&
           0 <= position[1] && position[1] <= uncertaintyWidth - 1 &&
           0 <= position[2] && position[2] <= uncertaintyDepth - 1) {
      // Linearly interpolate the uncertainty at this position using all neighbouring voxels.
      double interpolationAccumulator = 0.0;
      double distanceAccumulator = 0.0;
      for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) { 
          for (int k = -1; k <= 1; k++) {
            // Get the position of the neighbour.
            vtkVector<float, 3> neighbour = vtkVector<float, 3>();
            neighbour[0] = round(position[0] + i);
            neighbour[1] = round(position[1] + j);
            neighbour[2] = round(position[2] + k);

            // If the neighbour doesn't exist (we're on the edge), skip it.
            if (neighbour[0] < 0.0f || uncertaintyHeight <= neighbour[0] ||
                neighbour[1] < 0.0f || uncertaintyWidth <= neighbour[1] ||
                neighbour[2] < 0.0f || uncertaintyDepth <= neighbour[2]) {
              continue;
            }

            // Get the uncertainty of the neighbour.
            itk::Index<3> index;
            index[0] = neighbour[0];
            index[1] = neighbour[1];
            index[2] = neighbour[2];
            float neighbourUncertainty = readAccess.GetPixelByIndex(index);

            // Get the distance to this neighbour (to weight by)
            vtkVector<float, 3> difference = vectorSubtract(position, neighbour);
            double distanceToSample = difference.Norm();

            // If the distance turns out to be zero, we have perfectly hit the sample.
            //  to avoid division by zero we take this value and ignore the rest.
            if (distanceToSample == 0.0) {
              interpolationAccumulator = neighbourUncertainty;
              distanceAccumulator = 1;
              goto BREAK_ALL_LOOPS;
            }

            // Otherwise weight the uncertainty by the inverse distance and add the inverse distance to the total weight.
            distanceAccumulator += 1.0 / distanceToSample;
            interpolationAccumulator += neighbourUncertainty / distanceToSample;
          }
        }
      }
      BREAK_ALL_LOOPS:

      // Interpolate the actual sample.
      double interpolatedSample = interpolationAccumulator / distanceAccumulator;

      // Include sample if it's not background.
      if (interpolatedSample != 0.0) {
        uncertaintyAccumulator += interpolatedSample;
        numSamples++;
      }

      // Move along.
      position = vectorAdd(position, direction);
    }

    return round(uncertaintyAccumulator / numSamples);
  }

  catch (mitk::Exception & e) {
    cerr << "Hmmm... it appears we can't get read access to the uncertainty image. Maybe it's gone? Maybe it's type isn't unsigned char? (I've assumed it is)" << e << endl;
    return -1;
  }
}

// ----------- //
// ---- 3 ---- //
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

  // OLD: Compute optimal bounds.
  //   const mitk::TimeGeometry::Pointer bounds = this->GetDataStorage()->ComputeVisibleBoundingGeometry3D();

  // Set them.
  mitk::IRenderWindowPart* renderWindowPart = this->GetRenderWindowPart();
  mitk::IRenderingManager* renderManager = renderWindowPart->GetRenderingManager();
  renderManager->InitializeViews(bounds);

  this->RequestRenderWindowUpdate();
}

// ----------- //
// ---- 4 ---- //
// ----------- //

/**
  * Sets the uncertainty to be rendered in front of the scan (for overlaying in 2D).
  */
void Sams_View::SetLayers() {
  mitk::IntProperty::Pointer behindProperty = mitk::IntProperty::New(0);
  mitk::IntProperty::Pointer infrontProperty = mitk::IntProperty::New(1);

  this->scan->SetProperty("layer", behindProperty);
  this->uncertainty->SetProperty("layer", infrontProperty);

  this->RequestRenderWindowUpdate();
}

/**
  * Puts some useless text on the render windows.
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
  * Takes a surface and maps the uncertainty onto it based on the normal vector.
  */
void Sams_View::BrainSurfaceTest() {
  // Get the surface.
  mitk::DataNode::Pointer brainModelNode = this->GetDataStorage()->GetNode(mitk::NodePredicateDataType::New("Surface"));
  if (brainModelNode == (void*)NULL) {
    cout << "No Surface Found." << endl;
    return;
  }

  // Stop the specular in the rendering.
  brainModelNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  brainModelNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  brainModelNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));
  brainModelNode->SetProperty("scalar visibility", mitk::BoolProperty::New(true));

  // Cast it to an MITK surface.
  mitk::BaseData * brainModelData = brainModelNode->GetData();
  mitk::Surface::Pointer brainModelSurface = dynamic_cast<mitk::Surface*>(brainModelData);

  // Extract the vtkPolyData.
  vtkPolyData * brainModelVtk = brainModelSurface->GetVtkPolyData();

  // From this we can the array containing all the normals.
  vtkSmartPointer<vtkFloatArray> normals = vtkFloatArray::SafeDownCast(brainModelVtk->GetPointData()->GetNormals());
  if (!normals) {
    cerr << "Couldn't seem to find any normals." << endl;
    return;
  }

  // Compute the bounding box of the surface (for simple registration between surface and uncertainty volume)
  double bounds[6];
  brainModelVtk->GetBounds(bounds); // NOTE: Apparently this isn't thread safe.
  double xMin = bounds[0];
  double xMax = bounds[1];
  double xRange = xMax - xMin;
  double yMin = bounds[2];
  double yMax = bounds[3];
  double yRange = yMax - yMin;
  double zMin = bounds[4];
  double zMax = bounds[5];
  double zRange = zMax - zMin;
  cout << "Ranges:" << endl;
  cout << "x: (" << xMin << ", " << xMax << ") - " << xRange << endl;
  cout << "y: (" << yMin << ", " << yMax << ") - " << yRange << endl;
  cout << "z: (" << zMin << ", " << zMax << ") - " << zRange << endl;

  // Generate a list of colours, one for each point, based on the normal at that point.
  vtkSmartPointer<vtkUnsignedCharArray> colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
  colors->SetNumberOfComponents(3);
  colors->SetName ("Colors");  
  for (vtkIdType i = 0; i < brainModelVtk->GetNumberOfPoints(); i++) {
    // Get the position of point i
    double positionOfPoint[3];
    brainModelVtk->GetPoint(i, positionOfPoint);

    // TODO: Better registration step. Unfortunately MITK appears not to be able to do pointwise registration between surface and image.
    // Simple scaling. Normalise the point to 0-1 based on the bounding box of the surface. Scale by uncertainty volume size.
    vtkVector<float, 3> position = vtkVector<float, 3>();
    position[0] = ((positionOfPoint[0] - xMin) / xRange) * (uncertaintyHeight - 1);
    position[1] = ((positionOfPoint[1] - yMin) / yRange) * (uncertaintyWidth - 1);
    position[2] = ((positionOfPoint[2] - zMin) / zRange) * (uncertaintyDepth - 1);

    // Get the normal of point i
    double normalAtPoint[3];
    normals->GetTuple(i, normalAtPoint);
    vtkVector<float, 3> normal = vtkVector<float, 3>();
    normal[0] = -normalAtPoint[0];
    normal[1] = -normalAtPoint[1];
    normal[2] = -normalAtPoint[2];

    // Use the position and normal to sample the uncertainty data.
    int intensity = SampleUncertainty(position, normal);

    cout << "Intensity from (" << position[0] << ", " << position[1] << ", " << position[2] << ") -> (" << normal[0] << ", " << normal[1] << ", " << normal[2] << ") is " << intensity <<   "." << endl;

    // Set the colour to be a grayscale version of this sampling.
    unsigned char normalColour[3] = {static_cast<unsigned char>(intensity), static_cast<unsigned char>(intensity), static_cast<unsigned char>(intensity)};
    colors->InsertNextTupleValue(normalColour);
  }
  
  // Set the colours to be the scalar value of each point.
  brainModelVtk->GetPointData()->SetScalars(colors);
  
  cout << "Well, that works." << endl;
}

void Sams_View::GenerateSphereSurface() {
  // Create a simple (VTK) sphere.
  vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
  sphere->SetThetaResolution(50);
  sphere->SetPhiResolution(150);
  sphere->SetRadius(20.0);
  sphere->SetCenter(0, 0, 0);
  sphere->Update();

  // Wrap it in some MITK.
  mitk::Surface::Pointer sphereSurface = mitk::Surface::New();
  sphereSurface->SetVtkPolyData(static_cast<vtkPolyData*>(sphere->GetOutput()));

  // Store it as a DataNode.
  mitk::DataNode::Pointer sphereNode = mitk::DataNode::New();
  sphereNode->SetData(sphereSurface);
  sphereNode->SetProperty("name", mitk::StringProperty::New("Sphere Surface"));
  sphereNode->SetProperty("layer", mitk::IntProperty::New(3));
  sphereNode->SetProperty("material.ambientCoefficient", mitk::FloatProperty::New(1.0f));
  sphereNode->SetProperty("material.diffuseCoefficient", mitk::FloatProperty::New(0.0f));
  sphereNode->SetProperty("material.specularCoefficient", mitk::FloatProperty::New(0.0f));

  this->GetDataStorage()->Add(sphereNode);
}

// ----------- //
// ---- 5 ---- //
// ----------- //

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