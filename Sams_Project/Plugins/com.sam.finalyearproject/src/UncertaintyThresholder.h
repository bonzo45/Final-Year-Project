#ifndef Uncertainty_Thresholder_h
#define Uncertainty_Thresholder_h

#include <mitkImage.h>

class UncertaintyThresholder {
	public:
    void setUncertainty(mitk::Image::Pointer image);

  private:
    mitk::Image::Pointer uncertainty;
};

#endif