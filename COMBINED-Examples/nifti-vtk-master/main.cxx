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

#include <myVtkInteractorStyleImage.h>

const float BACKGROUND_R = 0.5f;
const float BACKGROUND_G = 0.5f;
const float BACKGROUND_B = 1.0f;

// helper class to format slice status message
class StatusMessage {
public:
   static std::string Format(int slice, int maxSlice) {
      std::stringstream tmp;
      tmp << "Slice Number  " << slice + 1 << "/" << maxSlice + 1;
      return tmp.str();
   }
};

int main(int argc, char *argv[]) {
    // --------------------
    // ITK: Read the Image.
    // --------------------
    typedef itk::Image<unsigned char, 3> VisualizingImageType;
    typedef itk::ImageFileReader<VisualizingImageType> ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(argv[1]);
    reader->Update();
    VisualizingImageType::Pointer image=reader->GetOutput();

    // --------------------
    // ItkVtkGlue: ITK to VTK image conversion.
    // --------------------
    typedef itk::ImageToVTKImageFilter<VisualizingImageType> itkVtkConverter;
    itkVtkConverter::Pointer conv = itkVtkConverter::New();
    conv->SetInput(image);
    conv->Update();

    // --------------------
    // VTK: (3D) Create a VolumeMapper that uses input image.
    // --------------------
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputData(conv->GetOutput());

    // VTK: Setting VolumeProperty (transparency and color mapping).
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();

    vtkSmartPointer<vtkPiecewiseFunction> compositeOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
    compositeOpacity->AddPoint(0.0, 0.0);
    compositeOpacity->AddPoint(255.0, 1.0);
    volumeProperty->SetScalarOpacity(compositeOpacity);
    volumeProperty->SetScalarOpacityUnitDistance(1.0);

    vtkSmartPointer<vtkColorTransferFunction> color = vtkSmartPointer<vtkColorTransferFunction>::New();
    color->AddRGBPoint(0.0,    0.0,0.0,0.0);
    color->AddRGBPoint(255.0, 1.0,1.0,1.0);
    volumeProperty->SetColor(color);

    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);

    // --------------------
    // VTK: (2D) Create a ImageResliceMapper that uses input image.
    // --------------------
    vtkSmartPointer<vtkImageResliceMapper> imageResliceMapper = vtkSmartPointer<vtkImageResliceMapper>::New();
    imageResliceMapper->SetInputData(conv->GetOutput());

    vtkSmartPointer<vtkImageSlice> imageSlice = vtkSmartPointer<vtkImageSlice>::New();
    imageSlice->SetMapper(imageResliceMapper);
    imageSlice->GetProperty()->SetInterpolationTypeToNearest();

    // --------------------
    // VTK: (2D) Create a ImageViewer that uses input image.
    // --------------------
    vtkSmartPointer<vtkImageViewer2> imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
    imageViewer->SetInputData(conv->GetOutput()); 

    // slice status message
    vtkSmartPointer<vtkTextProperty> sliceTextProp = vtkSmartPointer<vtkTextProperty>::New();
    sliceTextProp->SetFontFamilyToCourier();
    sliceTextProp->SetFontSize(20);
    sliceTextProp->SetVerticalJustificationToBottom();
    sliceTextProp->SetJustificationToLeft();

    vtkSmartPointer<vtkTextMapper> sliceTextMapper = vtkSmartPointer<vtkTextMapper>::New();
    std::string msg = StatusMessage::Format(imageViewer->GetSliceMin(), imageViewer->GetSliceMax());
    sliceTextMapper->SetInput(msg.c_str());
    sliceTextMapper->SetTextProperty(sliceTextProp);

    vtkSmartPointer<vtkActor2D> sliceTextActor = vtkSmartPointer<vtkActor2D>::New();
    sliceTextActor->SetMapper(sliceTextMapper);
    sliceTextActor->SetPosition(15, 10);    // create an interactor with our own style (inherit from vtkInteractorStyleImage)
   // in order to catch mousewheel and key events
   vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
      vtkSmartPointer<vtkRenderWindowInteractor>::New();
 
   vtkSmartPointer<myVtkInteractorStyleImage> myInteractorStyle =
      vtkSmartPointer<myVtkInteractorStyleImage>::New();
 
   // make imageviewer2 and sliceTextMapper visible to our interactorstyle
   // to enable slice status message updates when scrolling through the slices
   myInteractorStyle->SetImageViewer(imageViewer);
   myInteractorStyle->SetStatusMapper(sliceTextMapper);
 
   imageViewer->SetupInteractor(renderWindowInteractor);
   // make the interactor use our own interactorstyle
   // cause SetupInteractor() is defining it's own default interatorstyle 
   // this must be called after SetupInteractor()
   renderWindowInteractor->SetInteractorStyle(myInteractorStyle);
   // add slice status message and usage hint message to the renderer
   imageViewer->GetRenderer()->AddActor2D(sliceTextActor);
 
   // initialize rendering and interaction
   //imageViewer->GetRenderWindow()->SetSize(400, 300);
   //imageViewer->GetRenderer()->SetBackground(0.2, 0.3, 0.4);
   imageViewer->Render();
   imageViewer->GetRenderer()->ResetCamera();
   imageViewer->Render();
   renderWindowInteractor->Start();  

    // --------------------
    // VTK: Create RenderWindow and Renderer(s).
    // --------------------
    vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();

    double axialViewport[4] = {0.0, 0.5, 0.5, 1.0};
    double threeDViewport[4] = {0.5, 0.5, 1.0, 1.0};
    double sagittalViewport[4] = {0.0, 0.0, 0.5, 0.5};
    double coronalViewport[4] = {0.5, 0.0, 1.0, 0.5};

    // Axial
    vtkSmartPointer<vtkRenderer> axialRenderer = vtkSmartPointer<vtkRenderer>::New();
    axialRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    renWin->AddRenderer(axialRenderer);
    axialRenderer->SetViewport(axialViewport);

    // 3D
    vtkSmartPointer<vtkRenderer> threeDRenderer = vtkSmartPointer<vtkRenderer>::New();
    threeDRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    renWin->AddRenderer(threeDRenderer);
    threeDRenderer->SetViewport(threeDViewport);

    // Sagittal
    vtkSmartPointer<vtkRenderer> sagittalRenderer = vtkSmartPointer<vtkRenderer>::New();
    sagittalRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    renWin->AddRenderer(sagittalRenderer);
    sagittalRenderer->SetViewport(sagittalViewport);

    // Coronal
    vtkSmartPointer<vtkRenderer> coronalRenderer = vtkSmartPointer<vtkRenderer>::New();
    coronalRenderer->SetBackground(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    renWin->AddRenderer(coronalRenderer);
    coronalRenderer->SetViewport(coronalViewport);
    
    // Window Interactor
    vtkSmartPointer<vtkRenderWindowInteractor> windowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    vtkSmartPointer<vtkInteractorStyleImage> style = vtkSmartPointer<vtkInteractorStyleImage>::New();
    windowInteractor->SetInteractorStyle(style);
    windowInteractor->SetRenderWindow(renWin);
    
    renWin->SetSize(1280,720);
    renWin->Render(); // (call render to ensure we have  an OpenGL context)

    // --------------------
    // VTK: Add Axis to 3D View.
    // --------------------
    vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(250,250,250);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.01);
    threeDRenderer->AddActor(axes);

    // --------------------
    // VTK: Go!
    // --------------------
    //axialRenderer->AddViewProp(imageSlice);
    threeDRenderer->AddVolume(volume);
    sagittalRenderer->AddVolume(volume);
    coronalRenderer->AddVolume(volume);

    axialRenderer->ResetCamera();
    threeDRenderer->ResetCamera();
    sagittalRenderer->ResetCamera();
    coronalRenderer->ResetCamera();

    renWin->Render();
    windowInteractor->Start();

    return EXIT_SUCCESS;
}
