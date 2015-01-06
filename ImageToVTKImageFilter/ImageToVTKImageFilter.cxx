#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageToVTKImageFilter.h>

#include "vtkVersion.h"
#include "vtkImageViewer.h"
#include "vtkImageMapper3D.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSmartPointer.h"
#include "vtkImageActor.h"
#include "vtkInteractorStyleImage.h"
#include "vtkRenderer.h"
#include "itkRGBPixel.h"

int main(int argc, char *argv[]) {
    // Print Usage if no Image supplied.
    if (argc < 2) {
        std::cerr << "Usage: " << std::endl;
        std::cerr << argv[0] << " inputImageFile" << std::endl;
        return EXIT_FAILURE;
    }

    // Define Types
    typedef itk::Image<itk::RGBPixel<unsigned char>, 2> ImageType;
    typedef itk::ImageFileReader<ImageType>             ReaderType;
    typedef itk::ImageToVTKImageFilter<ImageType>       ConnectorType;

    // Create a 'Reader' for the Image. (ITK)
    // Create a 'Connector'. (ITK -> VTK)
    ReaderType::Pointer reader = ReaderType::New();
    ConnectorType::Pointer connector = ConnectorType::New();
    reader->SetFileName(argv[1]);
    connector->SetInput(reader->GetOutput());
  
    // Create an 'actor' to represent the image.
    // Actors are things that can be rendered.
    vtkSmartPointer<vtkImageActor> actor = vtkSmartPointer<vtkImageActor>::New();
    connector->Update();
    actor->GetMapper()->SetInputData(connector->GetOutput());
    
    // Create a renderer and add that actor.
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->AddActor(actor);
    renderer->ResetCamera();

    // Create a render window.
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);

    // Create an interactor.
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    vtkSmartPointer<vtkInteractorStyleImage> style = vtkSmartPointer<vtkInteractorStyleImage>::New();
    renderWindowInteractor->SetInteractorStyle(style);
    renderWindowInteractor->SetRenderWindow(renderWindow);
    renderWindowInteractor->Initialize();
    renderWindowInteractor->Start();
  
    return EXIT_SUCCESS;
}
