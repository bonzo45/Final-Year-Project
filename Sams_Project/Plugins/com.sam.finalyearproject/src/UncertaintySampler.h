#ifndef Uncertainty_Sampler_h
#define Uncertainty_Sampler_h

#include <mitkImage.h>

class UncertaintySampler {
	public:
    void setScan(mitk::Image::Pointer image);
    void setUncertainty(mitk::Image::Pointer image);

  private:
    mitk::Image::Pointer scan;
    mitk::Image::Pointer uncertainty;

};

#endif