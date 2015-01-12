#include "RenderWindowUI.h"

#include <itkImageFileReader.h>
#include <itkImageToVTKImageFilter.h>

// Splitscreen
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolume.h>
#include <vtkRenderer.h>
#include <vtkAxesActor.h>

// Simple Slice
#include <vtkImageSlice.h>
#include <vtkImageResliceMapper.h>
#include <vtkImageProperty.h>

// Slices
#include <vtkImageViewer2.h>
#include <vtkTextProperty.h>
#include <vtkTextMapper.h>
#include <vtkActor2D.h>
#include <VtkSliceInteractorStyle.h>
#include <StatusMessage.h>

// CUBE and SPHERE
#include <vtkPolyDataMapper.h>
#include <vtkSphereSource.h>
#include <vtkCubeSource.h>

const float BACKGROUND_R = 0.0f;
const float BACKGROUND_G = 0.0f;
const float BACKGROUND_B = 0.0f;

const int RESOLUTION_X = 1280;
const int RESOLUTION_Y = 720;

typedef itk::Image<unsigned char, 3> VisualizingImageType;
typedef itk::ImageFileReader<VisualizingImageType> ReaderType;
typedef itk::ImageToVTKImageFilter<VisualizingImageType> itkVtkConverter;

itkVtkConverter::Pointer readNifti(std::string fileName) {
   // --------------------
    // ITK: Read the Image.
    // --------------------
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(fileName);
    reader->Update();
    VisualizingImageType::Pointer image = reader->GetOutput();

    // --------------------
    // ItkVtkGlue: ITK to VTK image conversion.
    // --------------------
    itkVtkConverter::Pointer itkVtkConverter = itkVtkConverter::New();
    itkVtkConverter->SetInput(image);
    itkVtkConverter->Update();

    return itkVtkConverter; 
}

vtkSmartPointer<vtkVolume> createNiftiVolume(itkVtkConverter::Pointer niftiInput) {
    // --------------------
    // VTK: (3D) Create a VolumeMapper that uses input image.
    // --------------------
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputData(niftiInput->GetOutput());

    // VTK: Setting VolumeProperty (transparency and color mapping).
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();

    vtkSmartPointer<vtkPiecewiseFunction> compositeOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
    compositeOpacity->AddPoint(0.0, 0.0);
    compositeOpacity->AddPoint(255.0, 1.0);
    volumeProperty->SetScalarOpacity(compositeOpacity);
    volumeProperty->SetScalarOpacityUnitDistance(1.0);

    vtkSmartPointer<vtkColorTransferFunction> color = vtkSmartPointer<vtkColorTransferFunction>::New();
    color->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
    color->AddRGBPoint(255.0, 1.0, 1.0, 1.0);
    volumeProperty->SetColor(color);

    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);

    return volume;
}

vtkSmartPointer<vtkImageResliceMapper> createNiftiSimpleSliceMapper(itkVtkConverter::Pointer niftiInput) {
    // --------------------
    // VTK: (2D) Create a ImageResliceMapper that uses input image.
    // --------------------
    vtkSmartPointer<vtkImageResliceMapper> imageResliceMapper = vtkSmartPointer<vtkImageResliceMapper>::New();
    imageResliceMapper->SetInputData(niftiInput->GetOutput());

    vtkSmartPointer<vtkImageSlice> imageSlice = vtkSmartPointer<vtkImageSlice>::New();
    imageSlice->SetMapper(imageResliceMapper);
    imageSlice->GetProperty()->SetInterpolationTypeToNearest();

    return imageResliceMapper;
}

vtkSmartPointer<vtkRenderer> newDefaultRenderer() {
    vtkSmartPointer<vtkRenderer> defaultRenderer = vtkSmartPointer<vtkRenderer>::New();
    defaultRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);

    return defaultRenderer;
}

vtkSmartPointer<vtkRenderWindow> splitscreen(itkVtkConverter::Pointer niftiItkVtkConverter) {
    // // --------------------
    // // VTK: (3D) Create a VolumeMapper that uses input image.
    // // --------------------
    // vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    // volumeMapper->SetInputData(niftiItkVtkConverter->GetOutput());

    // // VTK: Setting VolumeProperty (transparency and color mapping).
    // vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();

    // vtkSmartPointer<vtkPiecewiseFunction> compositeOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
    // compositeOpacity->AddPoint(0.0, 0.0);
    // compositeOpacity->AddPoint(255.0, 1.0);
    // volumeProperty->SetScalarOpacity(compositeOpacity);
    // volumeProperty->SetScalarOpacityUnitDistance(1.0);

    // vtkSmartPointer<vtkColorTransferFunction> color = vtkSmartPointer<vtkColorTransferFunction>::New();
    // color->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
    // color->AddRGBPoint(255.0, 1.0, 1.0, 1.0);
    // volumeProperty->SetColor(color);

    // vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    // volume->SetMapper(volumeMapper);
    // volume->SetProperty(volumeProperty);

    // --
    // Create a Volume (for 3D visualisation of the Nifti)
    // --
    vtkSmartPointer<vtkVolume> niftiVolume = createNiftiVolume(niftiItkVtkConverter);

    // --
    // Create a renderer for each view.
    //--
    vtkSmartPointer<vtkRenderer> axialRenderer = newDefaultRenderer();
    vtkSmartPointer<vtkRenderer> threeDRenderer = newDefaultRenderer();
    vtkSmartPointer<vtkRenderer> sagittalRenderer = newDefaultRenderer();
    vtkSmartPointer<vtkRenderer> coronalRenderer = newDefaultRenderer();

    // --------------------
    // VTK: Add Volume to Renderers.
    // --------------------
    axialRenderer->AddVolume(niftiVolume);
    threeDRenderer->AddVolume(niftiVolume);
    sagittalRenderer->AddVolume(niftiVolume);
    coronalRenderer->AddVolume(niftiVolume);

    // --------------------
    // VTK: Add Axis to 3D View.
    // --------------------
    vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(250,250,250);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.01);
    threeDRenderer->AddActor(axes);

    // --------------------
    // VTK: Setup View
    // --------------------
    axialRenderer->ResetCamera();
    threeDRenderer->ResetCamera();
    sagittalRenderer->ResetCamera();
    coronalRenderer->ResetCamera();

    // --------------------
    // VTK: Add to Window
    // --------------------
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(axialRenderer);
    renderWindow->AddRenderer(threeDRenderer);
    renderWindow->AddRenderer(sagittalRenderer);
    renderWindow->AddRenderer(coronalRenderer);

    // Window Interactor
    // vtkSmartPointer<vtkRenderWindowInteractor> windowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    // windowInteractor->SetRenderWindow(renderWindow);
    // windowInteractor->Start();

    return renderWindow;
}

// void slice(itkVtkConverter::Pointer inputData, vtkRenderWindow* renderWindow) {
//     // --------------------
//     // VTK: (2D) Display the data in slice view.
//     // --------------------
//     vtkSmartPointer<vtkImageViewer2> imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
//     imageViewer->SetInputData(inputData->GetOutput()); 

//     // Setup Slice Indicator
//     vtkSmartPointer<vtkTextProperty> sliceTextProp = vtkSmartPointer<vtkTextProperty>::New();
//     sliceTextProp->SetFontFamilyToCourier();
//     sliceTextProp->SetFontSize(20);
//     sliceTextProp->SetVerticalJustificationToBottom();
//     sliceTextProp->SetJustificationToLeft();

//     vtkSmartPointer<vtkTextMapper> sliceTextMapper = vtkSmartPointer<vtkTextMapper>::New();
//     std::string msg = StatusMessage::Format(imageViewer->GetSliceMin(), imageViewer->GetSliceMax());
//     sliceTextMapper->SetInput(msg.c_str());
//     sliceTextMapper->SetTextProperty(sliceTextProp);

//     vtkSmartPointer<vtkActor2D> sliceTextActor = vtkSmartPointer<vtkActor2D>::New();
//     sliceTextActor->SetMapper(sliceTextMapper);
//     sliceTextActor->SetPosition(15, 10);

//     // Custom Interactor
//     vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
//     vtkSmartPointer<VtkSliceInteractorStyle> myInteractorStyle = vtkSmartPointer<VtkSliceInteractorStyle>::New();
//     myInteractorStyle->SetImageViewer(imageViewer);
//     myInteractorStyle->SetStatusMapper(sliceTextMapper);

//     imageViewer->SetupInteractor(renderWindowInteractor);
//     renderWindowInteractor->SetInteractorStyle(myInteractorStyle);
//     imageViewer->GetRenderer()->AddActor2D(sliceTextActor);

//     // Go!
//     imageViewer->GetRenderWindow()->SetSize(RESOLUTION_X, RESOLUTION_Y);
//     imageViewer->GetRenderer()->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
//     imageViewer->Render();
//     imageViewer->GetRenderer()->ResetCamera();
//     imageViewer->Render();
//     renderWindowInteractor->Start();
// }

// Constructor
RenderWindowUI::RenderWindowUI() {
    this->setupUi(this);
 
    // --
    // Read the Nifti File
    // --
    itkVtkConverter::Pointer niftiItkVtkConverter = readNifti("/home/sam/cs4/final-year-project/Data/Nifti/avg152T1_LR_nifti.nii.gz");
    
    // --
    // Create a Volume (for 3D visualisation of the Nifti)
    // --
    vtkSmartPointer<vtkVolume> niftiVolume = createNiftiVolume(niftiItkVtkConverter);

    // --
    // Create a renderer for each view.
    //--
    vtkSmartPointer<vtkRenderer> axialRenderer = newDefaultRenderer();
    vtkSmartPointer<vtkRenderer> threeDRenderer = newDefaultRenderer();
    vtkSmartPointer<vtkRenderer> sagittalRenderer = newDefaultRenderer();
    vtkSmartPointer<vtkRenderer> coronalRenderer = newDefaultRenderer();

    // --
    // Add volumes/slices/props
    // --
    axialRenderer->AddVolume(niftiVolume);
    // threeDRenderer->AddVolume(niftiVolume);
    // sagittalRenderer->AddVolume(niftiVolume);
    // coronalRenderer->AddVolume(niftiVolume);

    // // --------------------------- SPHERE AND CUBE -----------------
    // // Sphere
    // vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    // sphereSource->Update();
    // vtkSmartPointer<vtkPolyDataMapper> sphereMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    // sphereMapper->SetInputConnection(sphereSource->GetOutputPort());
    // vtkSmartPointer<vtkActor> sphereActor = vtkSmartPointer<vtkActor>::New();
    // sphereActor->SetMapper(sphereMapper);

    // // Cube
    // vtkSmartPointer<vtkCubeSource> cubeSource = vtkSmartPointer<vtkCubeSource>::New();
    // cubeSource->Update();
    // vtkSmartPointer<vtkPolyDataMapper> cubeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    // cubeMapper->SetInputConnection(cubeSource->GetOutputPort());
    // vtkSmartPointer<vtkActor> cubeActor = vtkSmartPointer<vtkActor>::New();
    // cubeActor->SetMapper(cubeMapper);

    // // VTK Renderer
    // axialRenderer->AddActor(sphereActor);
    // threeDRenderer->AddActor(cubeActor);
    // sagittalRenderer->AddActor(cubeActor);
    // coronalRenderer->AddActor(sphereActor);
    // // -------------------------------------------------------------

    // ---------------------------- USE QT -------------------------
    // --
    // Reset cameras
    // --
    axialRenderer->ResetCamera();
    threeDRenderer->ResetCamera();
    sagittalRenderer->ResetCamera();
    coronalRenderer->ResetCamera();

    // // --
    // // Add renderers to widgets
    // // --
    // this->axialWidget->GetRenderWindow()->AddRenderer(axialRenderer);
    // this->threeDWidget->GetRenderWindow()->AddRenderer(threeDRenderer);
    // this->sagittalWidget->GetRenderWindow()->AddRenderer(sagittalRenderer);
    // this->coronalWidget->GetRenderWindow()->AddRenderer(coronalRenderer);

    // --
    // OR set render windows of widgets
    // --
    vtkSmartPointer<vtkRenderWindow> renWin = this->axialWidget->GetRenderWindow(); // vtkSmartPointer<vtkRenderWindow>::New();
    //this->axialWidget->SetRenderWindow(renWin);
    
    vtkSmartPointer<vtkRenderWindowInteractor> iren = this->axialWidget->GetInteractor(); // vtkSmartPointer<vtkRenderWindowInteractor>::New();
    // renWin->SetInteractor(iren);
    // iren->SetRenderWindow(renWin);

    renWin->AddRenderer(axialRenderer);
    //this->axialWidget->show();
    renWin->Render();
    iren->Start();
    // this->threeDWidget->SetRenderWindow();
    // this->sagittalWidget->SetRenderWindow();
    // this->coronalWidget->SetRenderWindow();
    // -------------------------------------------------------------

    // --------------------------- OR NOT --------------------------
    // vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
    // renWin->AddRenderer(axialRenderer);
    // renWin->SetSize(300, 300);
    // vtkSmartPointer<vtkRenderWindowInteractor> iren = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    // iren->SetRenderWindow(renWin);
    // renWin->Render();
    // iren->Start();
    // -------------------------------------------------------------

    // Set up action signals and slots
    connect(this->actionExit, SIGNAL(triggered()), this, SLOT(slotExit()));
};

void RenderWindowUI::slotExit() {
  qApp->exit();
}