#include "UncertaintySampler.h"

void UncertaintySampler::setScan(mitk::Image::Pointer scan) {
  this->scan = scan;
}

void UncertaintySampler::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
}