#ifndef Sams_View_Util_h
#define Sams_View_Util_h

#include <string>

#include <vtkVector.h>

#include <mitkImage.h>
#include <mitkDataNode.h>
#include <mitkBaseProperty.h>


class Util {
	public:
    // ---- Conversions ---- //
    static std::string StringFromDouble(double value);
    // ---- Vectors ---- //
    static vtkVector<float, 3> vectorAdd(vtkVector<float, 3> a, vtkVector<float, 3> b);
    static vtkVector<float, 3> vectorSubtract(vtkVector<float, 3> a, vtkVector<float, 3> b);
    static vtkVector<float, 3> vectorScale(vtkVector<float, 3> a, float b);
    // ---- MITK ---- //
    static mitk::Image::Pointer MitkImageFromNode(mitk::DataNode::Pointer node);
    static std::string StringFromStringProperty(mitk::BaseProperty * property);
    static bool BoolFromBoolProperty(mitk::BaseProperty * property);
};


#endif