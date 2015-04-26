#include "SVDScanPlaneGenerator.h"

#include "Util.h"

#include <mitkVector.h>
#include <vnl/algo/vnl_svd.h>
#include <vcl_iostream.h>

void SVDScanPlaneGenerator::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);
}

vtkSmartPointer<vtkPlane> SVDScanPlaneGenerator::calculateBestScanPlane() {
  // TODO: Get worst x% of points.
  mitk::PointSet::Pointer pointSet = mitk::PointSet::New();
  mitk::Point3D point0;
  point0[0] = 0;
  point0[1] = 0;
  point0[2] = 0;
  pointSet->InsertPoint(0, point0);

  mitk::Point3D point1;
  point1[0] = 0;
  point1[1] = 0;
  point1[2] = 1;
  pointSet->InsertPoint(1, point1);

  mitk::Point3D point2;
  point2[0] = 1;
  point2[1] = 0;
  point2[2] = 0;
  pointSet->InsertPoint(2, point2);

  // Demean them.
  vnl_matrix<mitk::ScalarType> demeanedMatrix(pointSet->GetSize(), 3);

  mitk::Point3D centroid;
  calculateCentroid(pointSet, centroid);

  int pointTotal = pointSet->GetSize(0);
  for (int i = 0; i< pointTotal; i++) {
    mitk::Point3D point = pointSet->GetPoint(i);
    demeanedMatrix[i][0] = point[0] - centroid[0];
    demeanedMatrix[i][1] = point[1] - centroid[1];
    demeanedMatrix[i][2] = point[2] - centroid[2];
  }

  // Run SVD.
  vnl_svd<mitk::ScalarType> svd(demeanedMatrix, 0.0);
  vnl_vector<mitk::ScalarType> normal = svd.nullvector();
  if (normal[0] < 0) {
    normal = -normal;
  }

  // The plane consists of the centroid and normal.
  vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
  plane->SetOrigin(centroid[0], centroid[1], centroid[0]);
  plane->SetNormal(normal[0], normal[1], normal[2]);
  return plane;
}

void SVDScanPlaneGenerator::calculateCentroid(mitk::PointSet::Pointer pointSet, mitk::Point3D & centroid) {
  // If the point set is null, return (-1, -1, -1)
  if (pointSet.IsNull()) {
    centroid[0] = -1;
    centroid[1] = -1;
    centroid[2] = -1;
    return;
  }

  // If the point set isn't null, calculate the average point.
  int pointTotal = pointSet->GetSize(0);
  centroid[0] = centroid[1] = centroid[2] = 0.0;
  for (int i = 0; i < pointTotal; i++) {
    mitk::Point3D point = pointSet->GetPoint(i);
    centroid[0] += point[0];
    centroid[1] += point[1];
    centroid[2] += point[2];
  }

  centroid[0] /= pointTotal;
  centroid[1] /= pointTotal;
  centroid[2] /= pointTotal;
}