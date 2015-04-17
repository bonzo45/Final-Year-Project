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

vtkVector<float, 3> Util::vectorScale(vtkVector<float, 3> a, float b) {
  vtkVector<float, 3> c = vtkVector<float, 3>();
  c[0] = a[0] * b;
  c[1] = a[1] * b;
  c[2] = a[2] * b;
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