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

// PNG
#include <vtkPNGReader.h>

// Nifti. Need VTK 6.2.
#include <vtkNIFTIImageReader.h>

// --
// Colours
// --
const float BACKGROUND_R = 0.0f;
const float BACKGROUND_G = 0.0f;
const float BACKGROUND_B = 0.0f;

// --
// Files
// --
const char* nifti1Demo = "/home/sam/cs4/final-year-project/Data/Nifti/avg152T1_LR_nifti.nii.gz";
const char* nifti2Fetal = "/home/sam/cs4/final-year-project/Data/Fetal/3T_GPUtest.nii.gz";
const char* nifti2Demo = "/home/sam/cs4/final-year-project/Data/Nifti2/MNI152_T1_1mm_nifti2.nii.gz";

// --
// Typedefs
// --
typedef itk::Image<unsigned char, 3> VisualizingImageType;
typedef itk::ImageFileReader<VisualizingImageType> ReaderType;
typedef itk::ImageToVTKImageFilter<VisualizingImageType> itkVtkConverter;

vtkSmartPointer<vtkImageViewer2> axialViewer;
vtkSmartPointer<vtkImageViewer2> sagittalViewer;
vtkSmartPointer<vtkImageViewer2> coronalViewer;

/**
  * Loads a nifti file using ITK and converts it to VTK format suitable for 3D rendering.
  * fileName - the .nii or .nii.gz file.
  */
itkVtkConverter::Pointer loadNifti(std::string fileName) {
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

/**
  * Creates a volume mapper from a nifti file imported with loadNifti(), so it can be rendered in 3D.
  * niftiInput - output of loadNifti
  */
vtkSmartPointer<vtkVolume> createNiftiVolume(itkVtkConverter::Pointer niftiInput) {
    // --------------------
    // VTK: (3D) Create a VolumeMapper that uses input image.
    // --------------------
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputData(niftiInput->GetOutput());

    // --
    // VTK: Setting VolumeProperty (Transparency and Colours)
    // --
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

/**
  * Creates a renderer from a nifti file imported with loadNifti().
  * niftiItkVtkConverter - output of loadNifti
  */
vtkSmartPointer<vtkRenderer> createNifti3DRenderer(itkVtkConverter::Pointer niftiItkVtkConverter) {
    // --
    // Create a Volume (for 3D visualisation of the Nifti)
    // --
    cout << "RenderWindowUI: Creating Volume..." << endl;
    vtkSmartPointer<vtkVolume> niftiVolume = createNiftiVolume(niftiItkVtkConverter);

    // --
    // Create a Renderer
    //--
    cout << "RenderWindowUI: Creating 3D Renderer..." << endl;
    vtkSmartPointer<vtkRenderer> threeDRenderer = vtkSmartPointer<vtkRenderer>::New();
    threeDRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);

    // --
    // Add Volume & Axes
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
    // Reset Camera
    // --
    cout << "RenderWindowUI: Resetting Camera..." << endl;
    threeDRenderer->ResetCamera();

    return threeDRenderer;
}

/**
  * Properties of Text displaying slice number.
  */
vtkSmartPointer<vtkTextProperty> getSliceTextProperty() {
    vtkSmartPointer<vtkTextProperty> sliceTextProp = vtkSmartPointer<vtkTextProperty>::New();
    sliceTextProp->SetFontFamilyToCourier();
    sliceTextProp->SetFontSize(20);
    sliceTextProp->SetVerticalJustificationToBottom();
    sliceTextProp->SetJustificationToLeft();

    return sliceTextProp;
}

/**
  * Creates a TextMapper for displaying the slice number.
  * minSlice - the first slice in the file
  * maxSlice - the last slice in the file
  * textProperty - the properties for the text (call getSliceTextProperty())
  */
vtkSmartPointer<vtkTextMapper> getSliceTextMapper(int minSlice, int maxSlice, vtkSmartPointer<vtkTextProperty> textProperty) {
    vtkSmartPointer<vtkTextMapper> sliceTextMapper = vtkSmartPointer<vtkTextMapper>::New();
    std::string msg = StatusMessage::Format(minSlice, maxSlice);
    sliceTextMapper->SetInput(msg.c_str());
    sliceTextMapper->SetTextProperty(textProperty);

    return sliceTextMapper;
}

/**
  * Creates an Actor for displaying the slice number.
  * textMapper - the text mapper (call getSliceTextMapper())
  */
vtkSmartPointer<vtkActor2D> getSliceTextActor(vtkSmartPointer<vtkTextMapper> textMapper) {
    vtkSmartPointer<vtkActor2D> sliceTextActor = vtkSmartPointer<vtkActor2D>::New();
    sliceTextActor->SetMapper(textMapper);
    sliceTextActor->SetPosition(15, 10);

    return sliceTextActor;
}

/**
  * Creates a vtkImageViewer2 which views a PNG file.
  * fileName - path to the .png file
  */
vtkSmartPointer<vtkImageViewer2> createPNGViewer(const char* fileName) {
    // Reader
    vtkSmartPointer<vtkPNGReader> reader = vtkSmartPointer<vtkPNGReader>::New();
    reader->SetFileName(fileName);

    // Viewer
    vtkSmartPointer<vtkImageViewer2> pngViewer = vtkSmartPointer<vtkImageViewer2>::New();
    pngViewer->SetInputConnection(reader->GetOutputPort());

    return pngViewer;
}

/**
  * Creates a vtkImageViewer2 which provides a slice view of some Nifti data.
  * fileName - path to the .nii or .nii.gz file
  * renderWindow - vtkRenderWindow to render the result
  *
  */
vtkSmartPointer<vtkImageViewer2> createNiftiSliceViewer(const char* fileName, vtkSmartPointer<vtkRenderWindow> renderWindow) {
    vtkSmartPointer<vtkNIFTIImageReader> niftiReader = vtkSmartPointer<vtkNIFTIImageReader>::New();
    niftiReader->SetFileName(fileName);

    // IMAGEVIEWER
    vtkSmartPointer<vtkImageViewer2> newViewer = vtkSmartPointer<vtkImageViewer2>::New();
    newViewer->SetInputConnection(niftiReader->GetOutputPort());
    newViewer->SetRenderWindow(renderWindow);
    
    // STYLE
    vtkSmartPointer<VtkSliceInteractorStyle> style = vtkSmartPointer<VtkSliceInteractorStyle>::New();
    style->SetImageViewer(newViewer);

    vtkSmartPointer<vtkTextProperty> sliceTextProp = getSliceTextProperty();
    vtkSmartPointer<vtkTextMapper> sliceTextMapper = getSliceTextMapper(newViewer->GetSliceMin(), newViewer->GetSliceMax(), sliceTextProp);
    vtkSmartPointer<vtkActor2D> sliceTextActor = getSliceTextActor(sliceTextMapper);
    style->SetStatusMapper(sliceTextMapper);
    newViewer->GetRenderWindow()->GetInteractor()->SetInteractorStyle(style);
    newViewer->GetRenderer()->AddActor2D(sliceTextActor);

    return newViewer;
}

/**
  * Constructor. Called when Window is created.
  */
RenderWindowUI::RenderWindowUI() {
    cout << "RenderWindowUI: Setting up UI..." << endl;
    this->setupUi(this);
 
    // --
    // 3D
    // --
    cout << "Adding 3D View..." << endl;
    itkVtkConverter::Pointer niftiItkVtkConverter = loadNifti(nifti1Demo);
    vtkSmartPointer<vtkRenderer> threeDRenderer = createNifti3DRenderer(niftiItkVtkConverter);
    vtkSmartPointer<vtkRenderWindow> renWin = this->threeDWidget->GetRenderWindow();    
    renWin->AddRenderer(threeDRenderer);
    renWin->Render();

    // --
    // Axial
    // --
    cout << "Adding Axial View..." << endl;
    axialViewer = createNiftiSliceViewer(nifti1Demo, this->axialWidget->GetRenderWindow());
    axialViewer->Render();

    // --
    // Sagittal
    // --
    cout << "Adding Sagittal View..." << endl;
    sagittalViewer = createNiftiSliceViewer(nifti1Demo, this->sagittalWidget->GetRenderWindow());
    sagittalViewer->Render();

    // --
    // Coronal
    // --
    cout << "Adding Coronal View..." << endl;
    coronalViewer = createNiftiSliceViewer(nifti1Demo, this->coronalWidget->GetRenderWindow());
    coronalViewer->Render();

    // Set up action signals and slots
    connect(this->actionExit, SIGNAL(triggered()), this, SLOT(slotExit()));
};

/**
  * Flicks through each slice.
  */
void RenderWindowUI::something() {
    for (int i = coronalViewer->GetSliceMin(); i < coronalViewer->GetSliceMax(); i++) {
        coronalViewer->SetSlice(i);
        coronalViewer->Render();
    }
}

/**
  * What to do when the window is closed?
  */
void RenderWindowUI::slotExit() {
  qApp->exit();
}