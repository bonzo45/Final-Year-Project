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

// PNG
#include <vtkPNGReader.h>

// Nifti? Need VTK 6.2.
#include <vtkNIFTIImageReader.h>

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

// vtkSmartPointer<vtkImageViewer2> createNiftiImageViewer(itkVtkConverter::Pointer niftiInput) {
//     // --------------------
//     // VTK: (2D) Display the data in slice view.
//     // --------------------
//     vtkSmartPointer<vtkImageViewer2> imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
//     imageViewer->SetInputData(niftiInput->GetOutput()); 

//     // Setup Text to indicate slice number.
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
//     // myInteractorStyle->SetStatusMapper(sliceTextMapper);

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

//     return imageViewer;
// }

vtkSmartPointer<vtkRenderer> newDefaultRenderer() {
    vtkSmartPointer<vtkRenderer> defaultRenderer = vtkSmartPointer<vtkRenderer>::New();
    defaultRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);

    return defaultRenderer;
}

vtkSmartPointer<vtkTextProperty> getSliceTextProperty() {
    vtkSmartPointer<vtkTextProperty> sliceTextProp = vtkSmartPointer<vtkTextProperty>::New();
    sliceTextProp->SetFontFamilyToCourier();
    sliceTextProp->SetFontSize(20);
    sliceTextProp->SetVerticalJustificationToBottom();
    sliceTextProp->SetJustificationToLeft();

    return sliceTextProp;
}

vtkSmartPointer<vtkTextMapper> getSliceTextMapper(int minSlice, int maxSlice, vtkSmartPointer<vtkTextProperty> textProperty) {
    vtkSmartPointer<vtkTextMapper> sliceTextMapper = vtkSmartPointer<vtkTextMapper>::New();
    std::string msg = StatusMessage::Format(minSlice, maxSlice);
    sliceTextMapper->SetInput(msg.c_str());
    sliceTextMapper->SetTextProperty(textProperty);

    return sliceTextMapper;
}

vtkSmartPointer<vtkActor2D> getSliceTextActor(vtkSmartPointer<vtkTextMapper> textMapper) {
    vtkSmartPointer<vtkActor2D> sliceTextActor = vtkSmartPointer<vtkActor2D>::New();
    sliceTextActor->SetMapper(textMapper);
    sliceTextActor->SetPosition(15, 10);

    return sliceTextActor;
}

vtkSmartPointer<vtkImageViewer2> niftiViewer;

// Constructor
RenderWindowUI::RenderWindowUI() {
    // --
    // Files
    // --
    const char* nifti1Demo = "/home/sam/cs4/final-year-project/Data/Nifti/avg152T1_LR_nifti.nii.gz";
    const char* nifti2Fetal = "/home/sam/cs4/final-year-project/Data/Fetal/3T_GPUtest.nii.gz";
    const char* nifti2Demo = "/home/sam/cs4/final-year-project/Data/Nifti2/MNI152_T1_1mm_nifti2.nii.gz";

    cout << "RenderWindowUI: Setting up UI..." << endl;
    this->setupUi(this);
 
    // --
    // Read the Nifti File
    // --
    cout << "RenderWindowUI: Reading " << nifti1Demo << endl;
    itkVtkConverter::Pointer niftiItkVtkConverter = readNifti(nifti1Demo);
    
    // --
    // Create a Volume (for 3D visualisation of the Nifti)
    // --
    cout << "RenderWindowUI: Creating Volume..." << endl;
    vtkSmartPointer<vtkVolume> niftiVolume = createNiftiVolume(niftiItkVtkConverter);

    // --
    // Create the slice viewer (for 2D visualisation of the Nifti)
    // --
    //vtkSmartPointer<vtkImageViewer2> niftiImageViewer = createNiftiImageViewer(niftiItkVtkConverter);

    // --
    // Create a renderer for each view.
    //--
    cout << "RenderWindowUI: Creating 3D Renderer..." << endl;
    vtkSmartPointer<vtkRenderer> threeDRenderer = newDefaultRenderer();

    // --
    // Add volumes/slices/props
    // --
    cout << "RenderWindowUI: Adding Volume..." << endl;
    threeDRenderer->AddVolume(niftiVolume);

    cout << "RenderWindowUI: Adding Axes..." << endl;
    vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(250,250,250);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.01);
    threeDRenderer->AddActor(axes);

    // --
    // Reset cameras
    // --
    cout << "RenderWindowUI: Resetting Camera..." << endl;
    threeDRenderer->ResetCamera();

    // --
    // Add Renderers to the Window.
    // --
    cout << "Adding Renderer to RenderWindow..." << endl;
    vtkSmartPointer<vtkRenderWindow> renWin = this->threeDWidget->GetRenderWindow();    
    //vtkSmartPointer<vtkRenderWindowInteractor> iren = this->threeDWidget->GetInteractor();
    renWin->AddRenderer(threeDRenderer);
    renWin->Render();
    // If you do this it does: QVTKInteractor cannot control the event loop.
    // iren->Start();

    // --
    // BETA
    // --
    // Create Image Viewer
    cout << "Adding PNG to RenderWindow..." << endl;
    vtkSmartPointer<vtkPNGReader> reader = vtkSmartPointer<vtkPNGReader>::New();
    reader->SetFileName("/home/sam/Desktop/test.png");

    // Visualize
    vtkSmartPointer<vtkImageViewer2> pngViewer = vtkSmartPointer<vtkImageViewer2>::New();
    pngViewer->SetInputConnection(reader->GetOutputPort());
    pngViewer->SetRenderWindow(this->axialWidget->GetRenderWindow());
    pngViewer->Render();

    // --
    // BETA2
    // --
    cout << "Adding Slices to RenderWindow..." << endl;
    // Read the image
    itkVtkConverter::Pointer niftiItkVtkConverter2 = readNifti(nifti1Demo);
    vtkSmartPointer<vtkImageViewer2> sliceViewer = vtkSmartPointer<vtkImageViewer2>::New();
    sliceViewer->SetInputData(niftiItkVtkConverter2->GetOutput());
    sliceViewer->SetRenderWindow(this->sagittalWidget->GetRenderWindow());
    sliceViewer->Render();

    // --
    // BETA3
    // --
    cout << "Adding Nifti to RenderWindow..." << endl;
    vtkSmartPointer<vtkNIFTIImageReader> niftiReader = vtkSmartPointer<vtkNIFTIImageReader>::New();
    niftiReader->SetFileName(nifti1Demo);
    // niftiReader->SetFileName(nifti2Fetal);
    // niftiReader->SetFileName(nifti2Demo);

    // Visualize
    niftiViewer = vtkSmartPointer<vtkImageViewer2>::New();
    niftiViewer->SetInputConnection(niftiReader->GetOutputPort());
    niftiViewer->SetRenderWindow(this->coronalWidget->GetRenderWindow());
    
    ////
    // vtkSmartPointer<vtkInteractorStyleImage> style = vtkSmartPointer<vtkInteractorStyleImage>::New();
    vtkSmartPointer<VtkSliceInteractorStyle> style = vtkSmartPointer<VtkSliceInteractorStyle>::New();
    style->SetImageViewer(niftiViewer);

    // // Setup Text to indicate slice number.
    // vtkSmartPointer<vtkTextProperty> sliceTextProp = getSliceTextProperty();
    // vtkSmartPointer<vtkTextMapper> sliceTextMapper = getSliceTextMapper(niftiViewer->GetSliceMin(), niftiViewer->GetSliceMax(), sliceTextProp);
    // vtkSmartPointer<vtkActor2D> sliceTextActor = getSliceTextActor(sliceTextMapper);

    // vtkSmartPointer<VtkSliceInteractorStyle> style = vtkSmartPointer<VtkSliceInteractorStyle>::New();
    // style->SetImageViewer(niftiViewer);
    // style->SetStatusMapper(sliceTextMapper);

    niftiViewer->GetRenderWindow()->GetInteractor()->SetInteractorStyle(style);
    // niftiViewer->SetupInteractor(this->coronalWidget->GetRenderWindow()->GetInteractor());
    // niftiViewer->GetRenderer()->AddActor2D(sliceTextActor);
    
    ////
    niftiViewer->SetSlice(10);
    niftiViewer->Render();

    // Set up action signals and slots
    connect(this->actionExit, SIGNAL(triggered()), this, SLOT(slotExit()));
};

void RenderWindowUI::something() {
    for (int i = 1; i < 90; i++) {
        niftiViewer->SetSlice(i);
        niftiViewer->Render();
    }
}

void RenderWindowUI::slotExit() {
  qApp->exit();
}