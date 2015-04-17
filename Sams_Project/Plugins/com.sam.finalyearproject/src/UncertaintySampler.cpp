#include "UncertaintySampler.h"
#include "Util.h"

#include <cmath> // abs

#include <mitkImagePixelReadAccessor.h>

void UncertaintySampler::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
  this->uncertaintyHeight = uncertainty->GetDimension(0);
  this->uncertaintyWidth = uncertainty->GetDimension(1);
  this->uncertaintyDepth = uncertainty->GetDimension(2);
}

bool UncertaintySampler::isWithinUncertainty(vtkVector<float, 3> position) {
  return (0 <= position[0] && position[0] <= uncertaintyHeight - 1 &&
         0 <= position[1] && position[1] <= uncertaintyWidth - 1 &&
         0 <= position[2] && position[2] <= uncertaintyDepth - 1);
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
  double accumulator = 0.0;
  unsigned int sampleCount = 0;
  while (0 <= tortoise[0] && tortoise[0] <= uncertaintyHeight - 1 &&
         0 <= tortoise[1] && tortoise[1] <= uncertaintyWidth - 1 &&
         0 <= tortoise[2] && tortoise[2] <= uncertaintyDepth - 1) {
    double sample = interpolateUncertaintyAtPosition(tortoise);

    // Include sample if it's not background.
    if (sample != 0.0) {
      accumulator += sample;
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

  double result = (accumulator / sampleCount);
  if (DEBUGGING) {
    cout << "Result is " << result << " (" << accumulator << "/" << sampleCount << ")" << endl;
  }
  return result;
}

double UncertaintySampler::interpolateUncertaintyAtPosition(vtkVector<float, 3> position) {
  // Use an image accessor to read values from the uncertainty.
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
          neighbour[0] = round(position[0] + i);
          neighbour[1] = round(position[1] + j);
          neighbour[2] = round(position[2] + k);

          // If the neighbour doesn't exist (we're over the edge), skip it.
          if (neighbour[0] < 0.0f || uncertaintyHeight <= neighbour[0] ||
              neighbour[1] < 0.0f || uncertaintyWidth <= neighbour[1] ||
              neighbour[2] < 0.0f || uncertaintyDepth <= neighbour[2]) {
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