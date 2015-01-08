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
#include <vtkMatrix4x4.h>
#include <vtkAxesActor.h>

int main(int argc, char *argv[]) {
    // ITK: Read the Image.
    typedef itk::Image<unsigned char, 3> VisualizingImageType;
    typedef itk::ImageFileReader<VisualizingImageType> ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(argv[1]);
    reader->Update();
    VisualizingImageType::Pointer image=reader->GetOutput();

    // VTK: Create RenderWindow and Renderer.
    vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
    vtkSmartPointer<vtkRenderer> ren1 = vtkSmartPointer<vtkRenderer>::New();
    ren1->SetBackground(0.5f,0.5f,1.0f);
    renWin->AddRenderer(ren1);
    renWin->SetSize(1280,1024);
    vtkSmartPointer<vtkRenderWindowInteractor> iren = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    iren->SetRenderWindow(renWin);
    renWin->Render(); // make sure we have an OpenGL context.

    // ItkVtkGlue: ITK to VTK image conversion.
    typedef itk::ImageToVTKImageFilter<VisualizingImageType> itkVtkConverter;
    itkVtkConverter::Pointer conv = itkVtkConverter::New();
    conv->SetInput(image);
    conv->Update();

    // VTK: Create a VolumeMapper that uses input image.
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

    // VTK: Adding Axis, for a reference position.
    vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(250,250,250);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.01);
    ren1->AddActor(axes);

    // VTK: Go!
    ren1->AddVolume(volume);
    ren1->ResetCamera();
    renWin->Render();
    iren->Start();

    return EXIT_SUCCESS;
}
