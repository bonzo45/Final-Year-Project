#ifndef Uncertainty_Sampler_h
#define Uncertainty_Sampler_h

#include <mitkImage.h>
#include <vtkVector.h>

class UncertaintySampler {
	public:
    UncertaintySampler();
    void setUncertainty(mitk::Image::Pointer image);
    void setAverage();
    void setMin();
    void setMax();
    double sampleUncertainty(vtkVector<float, 3> startPosition, vtkVector<float, 3> direction, int percentage = 100);

  private:
    mitk::Image::Pointer uncertainty;
    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;
    double initialAccumulator;
    double (*accumulate)(double, double);
    double (*collapse)(double, double);
    static const bool DEBUGGING = true;

    double interpolateUncertaintyAtPosition(vtkVector<float, 3> position);
    bool isWithinUncertainty(vtkVector<float, 3> position);
    unsigned int continuousToDiscrete(double continuous, unsigned int max);
};

#endif