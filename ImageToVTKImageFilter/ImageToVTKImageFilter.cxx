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

//  Definitely needed... probably
#include <vtkSmartVolumeMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkVolumeProperty.h>
#include <vtkMatrix4x4.h>
#include <vtkAxesActor.h>

int main(int argc, char *argv[]) {
    // Print Usage if no Image supplied.
    if (argc < 2) {
        std::cerr << "Usage: " << std::endl;
        std::cerr << argv[0] << " inputFile" << std::endl;
        return EXIT_FAILURE;
    }

    // ITK

    // Types
    typedef itk::Image<unsigned char, 3> VisualizingImageType;
    typedef itk::ImageFileReader<VisualizingImageType> ReaderType;

    // Set up File Reader
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(argv[1]);
    reader->Update();

    // Read file
    VisualizingImageType::Pointer image = reader->GetOutput();

    // VTK

    // Create Render Window & Renderer
    vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
    vtkSmartPointer<vtkRenderer> ren1 = vtkSmartPointer<vtkRenderer>::New();
    ren1->SetBackground(0.5f,0.5f,1.0f);
    renWin->AddRenderer(ren1);
    renWin->SetSize(1280,720);

    // Create the Interactor
    vtkSmartPointer<vtkRenderWindowInteractor> iren = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    iren->SetRenderWindow(renWin);
    renWin->Render();

    // ITK -> VTK
    typedef itk::ImageToVTKImageFilter<VisualizingImageType> itkVtkConverter;
    itkVtkConverter::Pointer conv = itkVtkConverter::New();
    conv->SetInput(image);

    // Create a volume mapper (to map the image we load to pixel values)
    vtkSmartPointer<vtkSmartVolumeMapper> volumeMapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();    
    // conv->Update();
    // volumeMapper->SetInputData(conv->GetOutput());
  
    return EXIT_SUCCESS;
}
