#include "UncertaintySampler.h"
#include "Util.h"

#include <cmath> // abs
#include <cfloat> // DBL_MAX

#include <mitkImagePixelReadAccessor.h>
#include <mitkImageAccessByItk.h>

// Loading bar
#include <mitkProgressBar.h>

UncertaintySampler::UncertaintySampler() {
  setAverage();
}

/**
  * Set the uncertainty to sample.
  */
void UncertaintySampler::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);
}

double add(double a, double b) {
  return a + b;
}

double min(double a, double b) {
  return std::min(a, b);
}

double max(double a, double b) {
  return std::max(a, b);
}

double divide(double a, double b) {
  return a / b;
}

double passThrough(double a, double /*b*/) {
  return a;
}

/**
  * The sampled value will be the average of all the sample points.
  */
void UncertaintySampler::setAverage() {
  this->initialAccumulator = 0.0;
  this->accumulate = &add;
  this->collapse = &divide;
}

/**
  * The sampled value will be the smallest of all the sampled points.
  */
void UncertaintySampler::setMin() {
  this->initialAccumulator = DBL_MAX;
  this->accumulate = &min;
  this->collapse = &passThrough;
}

/**
  * The sampled value will be the largest of all the sampled points.
  */
void UncertaintySampler::setMax() {
  this->initialAccumulator = 0;
  this->accumulate = &max;
  this->collapse = &passThrough;
}

/**
  * Returns the average uncertainty along a vector in the uncertainty.
  * startPosition - the vector to begin tracing from
  * direction - the vector of the direction to trace in
  * percentage - how far to send the ray through the volume (0-100) (default 100)
  */
double UncertaintySampler::sampleUncertainty(vtkVector<float, 3> startPosition, vtkVector<float, 3> direction, int percentage) {
  // Starting at 'startPosition' move in 'direction' in unit steps, taking samples.  
  // Similar to tortoise & hare algorithm. The tortoise moves slowly, collecting the samples we use
  // and the hare travels faster to see where the end of the uncertainty is.
  // (this allows us to stop a certain percentage of the way)
  vtkVector<float, 3> tortoise = vtkVector<float, 3>(startPosition);
  vtkVector<float, 3> hare = vtkVector<float, 3>(startPosition);
  // Hare travels faster.
  vtkVector<float, 3> hareDirection = Util::vectorScale(direction, 100.0f / percentage);

  if (DEBUGGING) {
    cout << "Tortoise: (" << tortoise[0] << ", " << tortoise[1] << ", " << tortoise[2] << ")" << endl <<
            "Direction: (" << direction[0] << ", " << direction[1] << ", " << direction[2] << ")" << endl <<
            "Hare: (" << hare[0] << ", " << hare[1] << ", " << hare[2] << ")" << endl <<
            "Hare Direction: (" << hareDirection[0] << ", " << hareDirection[1] << ", " << hareDirection[2] << ")" << endl;  
  }

  // Move the tortoise and hare to the start of the uncertainty (i.e. not background)
  while (isWithinUncertainty(tortoise)) {
    double sample = interpolateUncertaintyAtPosition(tortoise);

    if (DEBUGGING) {
      cout << " - Finding Uncertainty: (" << tortoise[0] << ", " << tortoise[1] << ", " << tortoise[2] << ")" <<
              " is " << sample << endl;
    }

    if (sample == 0.0) {
      tortoise = Util::vectorAdd(tortoise, direction);
    }
    else {
      break;
    }
  }
  hare = vtkVector<float, 3>(tortoise);

  // Move the tortoise and hare at different speeds. The tortoise gathers samples, but
  // stops when the hare reaches the edge of the uncertainty.
  double accumulator = initialAccumulator;
  unsigned int sampleCount = 0;
  while (isWithinUncertainty(tortoise)) {
    double sample = interpolateUncertaintyAtPosition(tortoise);

    // Include sample if it's not background.
    if (sample != 0.0) {
      accumulator = accumulate(accumulator, sample);
      sampleCount++;
    }

    if (DEBUGGING) {
      cout << " - Sample at: (" << tortoise[0] << ", " << tortoise[1] << ", " << tortoise[2] << ")" <<
              " is " << sample << endl;
    }

    // Move along.
    tortoise = Util::vectorAdd(tortoise, direction);
    hare = Util::vectorAdd(hare, hareDirection);

    if (DEBUGGING) {
      cout << " - Hare moves to: (" << hare[0] << ", " << hare[1] << ", " << hare[2] << ")" << endl;
    }

    // If the hare goes over the edge, stop.
    if (percentage != 100 && (!isWithinUncertainty(hare) || interpolateUncertaintyAtPosition(hare) == 0.0)) {
      if (DEBUGGING) {
        cout << "- Hare over the edge." << endl;
      }
      break;
    }
  }

  double result = collapse(accumulator, sampleCount);
  if (DEBUGGING) {
    cout << "Result is " << result << " (" << accumulator << "/" << sampleCount << ")" << endl;
  }
  return result;
}

/**
  * Given a continuous position in the volume this interpolates the value.
  * NOTE: ITK has functionality to do this (see ITK VERSION below) but it turned out to be
  *   slower than the manual version I had written before I had realised this. I think it must
  *   sample more neighbours than the MANUAL VERSION.
  */
double UncertaintySampler::interpolateUncertaintyAtPosition(vtkVector<float, 3> position) {
  // // ITK VERSION
  // double result;
  // AccessByItk_2(this->uncertainty, Util::ItkInterpolateValue, position, result);
  // return result;

  // MANUAL VERSION
  try  {
    // See if the uncertainty data is available to be read.
    mitk::ImagePixelReadAccessor<double, 3> readAccess(this->uncertainty);

    double interpolationTotalAccumulator = 0.0;
    double interpolationDistanceAccumulator = 0.0;

    // We're going to interpolate this point by looking at the 8 nearest neighbours. 
    int xSampleRange = (round(position[0]) < position[0])? 1 : -1;
    int ySampleRange = (round(position[1]) < position[1])? 1 : -1;
    int zSampleRange = (round(position[2]) < position[2])? 1 : -1;

    // Loop through the 8 samples.
    for (int i = std::min(xSampleRange, 0); i <= std::max(xSampleRange, 0); i++) {
      for (int j = std::min(ySampleRange, 0); j <= std::max(ySampleRange, 0); j++) { 
        for (int k = std::min(zSampleRange, 0); k <= std::max(zSampleRange, 0); k++) {
          // Get the position of the neighbour.
          vtkVector<float, 3> neighbour = vtkVector<float, 3>();
          neighbour[0] = continuousToDiscrete(position[0] + i, uncertaintyHeight);
          neighbour[1] = continuousToDiscrete(position[1] + j, uncertaintyWidth);
          neighbour[2] = continuousToDiscrete(position[2] + k, uncertaintyDepth);

          // If the neighbour doesn't exist (we're over the edge), skip it.
          if (!isWithinUncertainty(neighbour)) {
            continue;
          }

          // Read the uncertainty of the neighbour.
          itk::Index<3> index;
          index[0] = neighbour[0];
          index[1] = neighbour[1];
          index[2] = neighbour[2];
          double neighbourUncertainty = readAccess.GetPixelByIndex(index);

          // If the uncertainty of the neighbour is 0, skip it.
          if (std::abs(neighbourUncertainty) < 0.0001) {
            continue;
          }

          // Get the distance to this neighbour
          vtkVector<float, 3> difference = Util::vectorSubtract(position, neighbour);
          double distanceToSample = difference.Norm();

          // If the distance turns out to be zero, we have a perfect match. Ignore all other samples.
          if (std::abs(distanceToSample) < 0.0001) {
            interpolationTotalAccumulator = neighbourUncertainty;
            interpolationDistanceAccumulator = 1;
            goto BREAK_ALL_LOOPS;
          }

          // Accumulate
          interpolationTotalAccumulator += neighbourUncertainty / distanceToSample;
          interpolationDistanceAccumulator += 1.0 / distanceToSample;
        }
      }
    }
    BREAK_ALL_LOOPS:

    // Interpolate the values. If there were no valid samples, set it to zero.
    return (interpolationTotalAccumulator == 0.0) ? 0 : interpolationTotalAccumulator / interpolationDistanceAccumulator;
  }

  catch (mitk::Exception & e) {
    cerr << "Hmmm... it appears we can't get read access to the uncertainty image. Maybe it's gone? Maybe it's type isn't double? (I've assumed it is)" << e << endl;
    return -1;
  }
}

/**
  * Returns true if a continuous position is within the range of the uncertainty.
  *  i.e. with a 3 pixel image the valid range is -0.5 to 2.5
  */
bool UncertaintySampler::isWithinUncertainty(vtkVector<float, 3> position) {
  return (
        -0.5 <= position[0] && position[0] <= (uncertaintyHeight - 0.5) &&
        -0.5 <= position[1] && position[1] <= (uncertaintyWidth - 0.5) &&
        -0.5 <= position[2] && position[2] <= (uncertaintyDepth - 0.5)
  );
}

/**
  * Rounds x to the nearest pixel index. Edge cases (-0.5 and max - 0.5) are mapped to edge pixels.
  *   e.g.  x = -0.5, max = 5, returns 0.
  *         x = 2.7, max = 5, returns 3.
  *         x = 4.5, max = 5, returns 4.
  */
unsigned int UncertaintySampler::continuousToDiscrete(double x, unsigned int max) {
  if (x == -0.5) {
    return 0;
  }

  else if (x == (max - 0.5)) {
    return max - 1;
  }

  else {
    return round(x);
  }
}