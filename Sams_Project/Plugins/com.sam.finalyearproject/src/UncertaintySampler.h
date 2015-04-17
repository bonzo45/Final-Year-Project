#ifndef Uncertainty_Sampler_h
#define Uncertainty_Sampler_h

#include <mitkImage.h>
#include <vtkVector.h>

class UncertaintySampler {
	public:
    void setUncertainty(mitk::Image::Pointer image);
    double sampleUncertainty(vtkVector<float, 3> startPosition, vtkVector<float, 3> direction, int percentage = 100);

  private:
    mitk::Image::Pointer uncertainty;
    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;

    const bool DEBUGGING = false;

    double interpolateUncertaintyAtPosition(vtkVector<float, 3> position);
    bool isWithinUncertainty(vtkVector<float, 3> position);
};

#endif