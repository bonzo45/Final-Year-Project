#include "Util.h"
#include <sstream>
#include <iomanip>

#include <mitkProperties.h>

// --------------------- //
// ---- Conversions ---- //
// --------------------- //

std::string Util::StringFromDouble(double value) {
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << value;
  return(ss.str());
}

unsigned char Util::IntensityToRed(unsigned char intensity) {
  return 255 - intensity;
}

unsigned char Util::IntensityToGreen(unsigned char /*intensity*/) {
  return 0;
}

unsigned char Util::IntensityToBlue(unsigned char /*intensity*/) {
  return 0;
}

// ----------------- //
// ---- Vectors ---- //
// ----------------- //

vtkVector<float, 3> Util::vectorAdd(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] + b[0];
  c[1] = a[1] + b[1];
  c[2] = a[2] + b[2];
  return c;
}

vtkVector<float, 3> Util::vectorSubtract(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  c[2] = a[2] - b[2];
  return c;
}

vtkVector<double, 3> Util::vectorSubtract(vtkVector<double, 3> a, vtkVector<double, 3> b) {
  vtkVector<double, 3> c = vtkVector<double, 3>();
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  c[2] = a[2] - b[2];
  return c;
}

vtkVector<float, 3> Util::vectorScale(vtkVector<float, 3> a, float b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] * b;
  c[1] = a[1] * b;
  c[2] = a[2] * b;
  return c;  
}

vtkVector<double, 3> Util::vectorScale(vtkVector<double, 3> a, double b) {
  vtkVector<double, 3> c = vtkVector<double, 3>();
  c[0] = a[0] * b;
  c[1] = a[1] * b;
  c[2] = a[2] * b;
  return c;
}

vtkVector<float, 3> Util::vectorCross(vtkVector<float, 3> a, vtkVector<float, 3> b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[1] * b[2] - a[2] * b[1];
  c[1] = a[2] * b[0] - a[0] * b[2];
  c[2] = a[0] * b[1] - a[1] * b[0];
  return c;  
}

// -------------- //
// ---- MITK ---- //
// -------------- //

mitk::Image::Pointer Util::MitkImageFromNode(mitk::DataNode::Pointer node) {
  return dynamic_cast<mitk::Image*>(node->GetData());
}

std::string Util::StringFromStringProperty(mitk::BaseProperty * property) {
  mitk::StringProperty::Pointer stringProperty = dynamic_cast<mitk::StringProperty*>(property);
  if (stringProperty) {
    return stringProperty->GetValueAsString();
  }
  else {
    return "INVALID PROPERTY";
  }
}

bool Util::BoolFromBoolProperty(mitk::BaseProperty * property) {
  mitk::BoolProperty * boolProperty = dynamic_cast<mitk::BoolProperty *>(property);
  if (boolProperty) {
    return boolProperty->GetValue();
  }
  else {
    return false;
  }
}

// ---------------- //
// ---- Planes ---- //
// ---------------- //

double Util::distanceFromPointToPlane(unsigned int x, unsigned int y, unsigned int z, vtkSmartPointer<vtkPlane> plane) {
  vtkVector<double, 3> pointVector = vtkVector<double, 3>();
  pointVector[0] = x;
  pointVector[1] = y;
  pointVector[2] = z;

  double * origin = plane->GetOrigin();
  vtkVector<double, 3> originVector = vtkVector<double, 3>();
  originVector[0] = origin[0];
  originVector[1] = origin[1];
  originVector[2] = origin[2];

  double * normal = plane->GetNormal();
  vtkVector<double, 3> normalVector = vtkVector<double, 3>();
  normalVector[0] = normal[0];
  normalVector[1] = normal[1];
  normalVector[2] = normal[2];

  // (Point - Center) dot Normal
  return std::abs(Util::vectorSubtract(pointVector, originVector).Dot(normalVector));
}

vtkSmartPointer<vtkPlane> Util::planeFromPoints(vtkVector<float, 3> point1, vtkVector<float, 3> point2, vtkVector<float, 3> point3) {
  vtkSmartPointer<vtkPlane> nextPlane = vtkSmartPointer<vtkPlane>::New();
  nextPlane->SetOrigin(point1[0], point1[1], point1[0]);
  
  vtkVector<float, 3> cross = Util::vectorCross(Util::vectorSubtract(point2, point1), Util::vectorSubtract(point3, point1));
  cross.Normalize();
  nextPlane->SetNormal(cross[0], cross[1], cross[2]);

  return nextPlane;
}