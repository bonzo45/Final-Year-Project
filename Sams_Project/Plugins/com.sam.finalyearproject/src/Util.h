#ifndef Sams_View_Util_h
#define Sams_View_Util_h

#include <string>

#include <vtkSmartPointer.h>
#include <vtkPlane.h>
#include <vtkVector.h>

#include <mitkImage.h>
#include <mitkDataNode.h>
#include <mitkBaseProperty.h>


class Util {
	public:
    // ---- Conversions ---- //
    static std::string StringFromDouble(double value);
    static unsigned char IntensityToRed(unsigned char intensity);
    static unsigned char IntensityToGreen(unsigned char intensity);
    static unsigned char IntensityToBlue(unsigned char intensity);
    // ---- Vectors ---- //
    static vtkVector<float, 3> vectorAdd(vtkVector<float, 3> a, vtkVector<float, 3> b);
    static vtkVector<float, 3> vectorSubtract(vtkVector<float, 3> a, vtkVector<float, 3> b);
    static vtkVector<double, 3> vectorSubtract(vtkVector<double, 3> a, vtkVector<double, 3> b);
    static vtkVector<float, 3> vectorScale(vtkVector<float, 3> a, float b);
    static vtkVector<float, 3> vectorCross(vtkVector<float, 3> a, vtkVector<float, 3> b);
    // ---- MITK ---- //
    static mitk::Image::Pointer MitkImageFromNode(mitk::DataNode::Pointer node);
    static std::string StringFromStringProperty(mitk::BaseProperty * property);
    static bool BoolFromBoolProperty(mitk::BaseProperty * property);
    // ---- Planes ---- //
    static double distanceFromPointToPlane(unsigned int x, unsigned int y, unsigned int z, vtkSmartPointer<vtkPlane> plane);
    static vtkSmartPointer<vtkPlane> planeFromPoints(vtkVector<float, 3> point1, vtkVector<float, 3> point2, vtkVector<float, 3> point3);
};


#endif