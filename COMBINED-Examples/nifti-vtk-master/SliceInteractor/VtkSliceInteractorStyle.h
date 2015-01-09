#ifndef MY_VTK_INTERACTOR_STYLE_IMAGE_H
#define MY_VTK_INTERACTOR_STYLE_IMAGE_H

#include <vtkInteractorStyleImage.h>
#include <vtkObjectFactory.h>
#include <vtkImageViewer2.h>
#include <vtkTextMapper.h>
 
class VtkSliceInteractorStyle : public vtkInteractorStyleImage {

   public:
      static VtkSliceInteractorStyle* New();
      vtkTypeMacro(VtkSliceInteractorStyle, vtkInteractorStyleImage);

   protected:
      vtkImageViewer2* _ImageViewer;
      vtkTextMapper* _StatusMapper;
      int _Slice;
      int _MinSlice;
      int _MaxSlice;

   public:
   	void SetImageViewer(vtkImageViewer2* imageViewer);
   	void SetStatusMapper(vtkTextMapper* statusMapper);

   protected:
      void MoveSliceForward();
      void MoveSliceBackward();
      virtual void OnKeyDown();
      virtual void OnMouseWheelForward();
      virtual void OnMouseWheelBackward();
};

#endif