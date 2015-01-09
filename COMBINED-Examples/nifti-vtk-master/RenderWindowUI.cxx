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

// Slices
#include <vtkImageViewer2.h>
#include <vtkTextProperty.h>
#include <vtkTextMapper.h>
#include <vtkActor2D.h>
#include <VtkSliceInteractorStyle.h>
#include <StatusMessage.h>

const float BACKGROUND_R = 0.0f;
const float BACKGROUND_G = 0.0f;
const float BACKGROUND_B = 0.0f;

const int RESOLUTION_X = 1280;
const int RESOLUTION_Y = 720;

typedef itk::Image<unsigned char, 3> VisualizingImageType;
typedef itk::ImageFileReader<VisualizingImageType> ReaderType;
typedef itk::ImageToVTKImageFilter<VisualizingImageType> itkVtkConverter;

vtkSmartPointer<vtkRenderWindow> splitscreen(itkVtkConverter::Pointer inputData) {
    // --------------------
    // VTK: (3D) Create a VolumeMapper that uses input image.
    // --------------------
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputData(inputData->GetOutput());

    // VTK: Setting VolumeProperty (transparency and color mapping).
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();

    vtkSmartPointer<vtkPiecewiseFunction> compositeOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
    compositeOpacity->AddPoint(0.0, 0.0);
    compositeOpacity->AddPoint(255.0, 1.0);
    volumeProperty->SetScalarOpacity(compositeOpacity);
    volumeProperty->SetScalarOpacityUnitDistance(1.0);

    vtkSmartPointer<vtkColorTransferFunction> color = vtkSmartPointer<vtkColorTransferFunction>::New();
    color->AddRGBPoint(0.0, 0.0,0.0,0.0);
    color->AddRGBPoint(255.0, 1.0,1.0,1.0);
    volumeProperty->SetColor(color);

    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);

    // --------------------
    // VTK: (2D) Create a ImageResliceMapper that uses input image.
    // --------------------
    // vtkSmartPointer<vtkImageResliceMapper> imageResliceMapper = vtkSmartPointer<vtkImageResliceMapper>::New();
    // imageResliceMapper->SetInputData(inputData->GetOutput());

    // vtkSmartPointer<vtkImageSlice> imageSlice = vtkSmartPointer<vtkImageSlice>::New();
    // imageSlice->SetMapper(imageResliceMapper);
    // imageSlice->GetProperty()->SetInterpolationTypeToNearest();

    // --------------------
    // VTK: Create Renderers
    // --------------------
    double axialViewport[4] = {0.0, 0.5, 0.5, 1.0};
    double threeDViewport[4] = {0.5, 0.5, 1.0, 1.0};
    double sagittalViewport[4] = {0.0, 0.0, 0.5, 0.5};
    double coronalViewport[4] = {0.5, 0.0, 1.0, 0.5};

    // Axial
    vtkSmartPointer<vtkRenderer> axialRenderer = vtkSmartPointer<vtkRenderer>::New();
    axialRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    axialRenderer->SetViewport(axialViewport);

    // 3D
    vtkSmartPointer<vtkRenderer> threeDRenderer = vtkSmartPointer<vtkRenderer>::New();
    threeDRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    threeDRenderer->SetViewport(threeDViewport);

    // Sagittal
    vtkSmartPointer<vtkRenderer> sagittalRenderer = vtkSmartPointer<vtkRenderer>::New();
    sagittalRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    sagittalRenderer->SetViewport(sagittalViewport);

    // Coronal
    vtkSmartPointer<vtkRenderer> coronalRenderer = vtkSmartPointer<vtkRenderer>::New();
    coronalRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    coronalRenderer->SetViewport(coronalViewport);

    // --------------------
    // VTK: Add Volume to Renderers.
    // --------------------
    axialRenderer->AddVolume(volume);
    threeDRenderer->AddVolume(volume);
    sagittalRenderer->AddVolume(volume);
    coronalRenderer->AddVolume(volume);

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
 
    // --------------------
    // ITK: Read the Image.
    // --------------------
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName("/home/sam/cs4/final-year-project/Data/Nifti/avg152T1_LR_nifti.nii.gz");
    reader->Update();
    VisualizingImageType::Pointer image = reader->GetOutput();

    // --------------------
    // ItkVtkGlue: ITK to VTK image conversion.
    // --------------------
    itkVtkConverter::Pointer itkVtkConverter = itkVtkConverter::New();
    itkVtkConverter->SetInput(image);
    itkVtkConverter->Update();

    this->qvtkWidget->SetRenderWindow(splitscreen(itkVtkConverter));

    // Set up action signals and slots
    connect(this->actionExit, SIGNAL(triggered()), this, SLOT(slotExit()));
};

void RenderWindowUI::slotExit() {
  qApp->exit();
}

/*

#include <itkImageFileReader.h>
#include "itkImageToVTKImageFilter.h"

#include <vtkVersion.h>
#include <vtkSmartPointer.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkVolumeProperty.h>
#include <vtkAxesActor.h>

#include <vtkImageSlice.h>
#include <vtkImageProperty.h>
#include <vtkImageResliceMapper.h>
#include <vtkInteractorStyleImage.h>

#include <vtkImageViewer2.h>
#include <vtkTextMapper.h>
#include <vtkTextProperty.h>
#include <vtkActor2D.h>

#include <VtkSliceInteractorStyle.h>
#include <StatusMessage.h>

#include <QApplication>
#include <QVTKWidget.h>

*/