#include "ScanPlaneCalculator.h"
#include "Util.h"

#include <cmath>

#include <itkMultiplyImageFilter.h>
#include <itkImportImageFilter.h>
#include <itkStatisticsImageFilter.h>

#include <mitkImageCast.h>

// Loading bar
#include <mitkProgressBar.h>

ScanPlaneCalculator::ScanPlaneCalculator() {
  setGoodnessThreshold(0.5);
  setMaximumIterations(10);
  setPlaneThickness(1.0);
}

void ScanPlaneCalculator::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);

  mitk::CastToItkImage(uncertainty, this->uncertaintyItk);

  typedef itk::StatisticsImageFilter<itk::Image<double, 3> > StatisticsImageFilterType;
  StatisticsImageFilterType::Pointer statisticsImageFilter = StatisticsImageFilterType::New();
  statisticsImageFilter->SetInput(this->uncertaintyItk);
  statisticsImageFilter->Update();
  totalUncertainty = statisticsImageFilter->GetSum();
  if (DEBUGGING) {
    cout << "Total Uncertainty: " << totalUncertainty << endl;
  }
}

void ScanPlaneCalculator::setGoodnessThreshold(double goodness) {
  this->goodnessThreshold = goodness;
}

void ScanPlaneCalculator::setMaximumIterations(unsigned int iterations) {
  this->maxIterations = iterations;
}

void ScanPlaneCalculator::setPlaneThickness(double thickness) {
  this->planeThickness = thickness;
}

vtkSmartPointer<vtkPlane> ScanPlaneCalculator::calculateBestScanPlane() {
  unsigned int iterations = 0;
  double bestGoodnessSoFar = 0.0;

  vtkSmartPointer<vtkPlane> bestPlaneSoFar;
  if (DEBUGGING) {
      cout << "Finding Best Scan Plane" << endl;
    }
  // Keep generating planes until we give up (too many iterations) or we find one that is good enough.
  while (iterations < maxIterations && bestGoodnessSoFar < goodnessThreshold) {
    if (DEBUGGING) {
      cout << "Iteration " << iterations << "." << endl;
    }
    // Generate new plane.
    vtkSmartPointer<vtkPlane> nextPlane = generatePotentialPlane();

    if (DEBUGGING) {
      double * origin = nextPlane->GetOrigin();
      cout << " - origin: (" << origin[0] << ", " << origin[1] << ", " << origin[2] << ")" << endl;
      double * normal = nextPlane->GetNormal();
      cout << " - normal: (" << normal[0] << ", " << normal[1] << ", " << normal[2] << ")" << endl;
    }

    // See how good it is.
    double nextGoodness = evaluateScanPlaneGoodness(nextPlane);
    if (bestGoodnessSoFar < nextGoodness) {
      bestGoodnessSoFar = nextGoodness;
      bestPlaneSoFar = nextPlane;
    }

    if (DEBUGGING) {
      cout << " - goodness: " << nextGoodness << endl;
    }

    iterations++;
  }

  if (DEBUGGING) {
    cout << "Best One" << endl;
    double * origin = bestPlaneSoFar->GetOrigin();
    cout << " - origin: (" << origin[0] << ", " << origin[1] << ", " << origin[2] << ")" << endl;
    double * normal = bestPlaneSoFar->GetNormal();
    cout << " - normal: (" << normal[0] << ", " << normal[1] << ", " << normal[2] << ")" << endl;
    cout << " - goodness: " << bestGoodnessSoFar << endl;
  }

  return bestPlaneSoFar;
}

double ScanPlaneCalculator::distanceFromPointToPlane(unsigned int x, unsigned int y, unsigned int z, vtkSmartPointer<vtkPlane> plane) {
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

vtkSmartPointer<vtkPlane> ScanPlaneCalculator::planeFromPoints(vtkVector<float, 3> point1, vtkVector<float, 3> point2, vtkVector<float, 3> point3) {
  vtkSmartPointer<vtkPlane> nextPlane = vtkSmartPointer<vtkPlane>::New();
  nextPlane->SetOrigin(point1[0], point1[1], point2[0]);
  
  vtkVector<float, 3> cross = Util::vectorCross(Util::vectorSubtract(point2, point1), Util::vectorSubtract(point3, point1));
  cross.Normalize();
  nextPlane->SetNormal(cross[0], cross[1], cross[2]);

  return nextPlane;
}

vtkSmartPointer<vtkPlane> ScanPlaneCalculator::generatePotentialPlane() {
  // TODO: Generate slightly better random points.
  vtkVector<float, 3> point1 = vtkVector<float, 3>();
  point1[0] = rand() % uncertaintyHeight;
  point1[1] = rand() % uncertaintyWidth;
  point1[2] = rand() % uncertaintyDepth;

  vtkVector<float, 3> point2 = vtkVector<float, 3>();
  point2[0] = rand() % uncertaintyHeight;
  point2[1] = rand() % uncertaintyWidth;
  point2[2] = rand() % uncertaintyDepth;

  vtkVector<float, 3> point3 = vtkVector<float, 3>();
  point3[0] = rand() % uncertaintyHeight;
  point3[1] = rand() % uncertaintyWidth;
  point3[2] = rand() % uncertaintyDepth;

  // Create a plane from those points.
  return planeFromPoints(point1, point2, point3);
}

double ScanPlaneCalculator::evaluateScanPlaneGoodness(vtkSmartPointer<vtkPlane> plane) {
  // Create a volume (uncertainty mask) the same size as the uncertainty with each value set to zero.
  double * uncertaintyMask = new double[uncertaintyHeight * uncertaintyWidth * uncertaintyDepth];
  memset(uncertaintyMask, 0, sizeof(double) * uncertaintyHeight * uncertaintyWidth * uncertaintyDepth);
  double * it = uncertaintyMask;

  // For each point in the uncertainty mask set it's value to be proportional to the distance from the plane.
  for(unsigned int z = 0; z < uncertaintyDepth; z++) {
    for(unsigned int y = 0; y < uncertaintyWidth; y++) {
      for(unsigned int x = 0; x < uncertaintyHeight; x++) {
        double distance = distanceFromPointToPlane(x, y, z, plane);
        if (distance <= 1.0) {
          *it = distance;
        }
        it++;
      }
    }
  }

  // Bring the mask into ITK-land.
  typedef itk::ImportImageFilter<double, 3> ImportFilterType;
  ImportFilterType::Pointer importFilter = ImportFilterType::New(); 
  
  ImportFilterType::SizeType  size; 
  size[0] = uncertaintyHeight;
  size[1] = uncertaintyWidth;
  size[2] = uncertaintyDepth;
  
  ImportFilterType::IndexType start;
  start[0] = 0;
  start[1] = 0;
  start[2] = 0;
  
  ImportFilterType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);
  importFilter->SetRegion(region);
 
  double origin[3];
  origin[0] = 0.0;
  origin[1] = 0.0;
  origin[2] = 0.0;
  importFilter->SetOrigin(origin);
 
  double spacing[3];
  spacing[0] = 1.0;
  spacing[1] = 1.0;
  spacing[2] = 1.0;
  importFilter->SetSpacing(spacing);

  const bool importImageFilterWillOwnTheBuffer = true;
  importFilter->SetImportPointer(uncertaintyMask, uncertaintyHeight * uncertaintyWidth * uncertaintyDepth, importImageFilterWillOwnTheBuffer);
  importFilter->Update();

  // Do pointwise product between the uncertainty mask and the uncertainty.
  typedef itk::MultiplyImageFilter<itk::Image<double, 3> > MultiplyImageFilterType;
 
  MultiplyImageFilterType::Pointer multiplyFilter = MultiplyImageFilterType::New();
  multiplyFilter->SetInput1(uncertaintyItk);
  multiplyFilter->SetInput2(importFilter->GetOutput());
  multiplyFilter->Update();

  // The total amount of uncertainty covered by this plane is the sum of all values in this product.
  typedef itk::StatisticsImageFilter<itk::Image<double, 3> > StatisticsImageFilterType;
  StatisticsImageFilterType::Pointer statisticsImageFilter = StatisticsImageFilterType::New();
  statisticsImageFilter->SetInput(multiplyFilter->GetOutput());
  statisticsImageFilter->Update();

  return statisticsImageFilter->GetSum() / totalUncertainty;
}