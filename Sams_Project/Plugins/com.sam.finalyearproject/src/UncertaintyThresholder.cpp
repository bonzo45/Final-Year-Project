#include "UncertaintyThresholder.h"

void UncertaintyThresholder::setUncertainty(mitk::Image::Pointer uncertainty) {
  this->uncertainty = uncertainty;
}