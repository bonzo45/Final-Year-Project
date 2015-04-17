#ifndef Uncertainty_Thresholder_h
#define Uncertainty_Thresholder_h

#include <mitkImage.h>

class UncertaintyThresholder {
	public:
    UncertaintyThresholder();
    void setUncertainty(mitk::Image::Pointer image);
    void setIgnoreZeros(bool ignoreZeros);
    mitk::Image::Pointer thresholdUncertainty(double min, double max);
    void getTopXPercentThreshold(int percentage, double & min, double & max);

  private:
    mitk::Image::Pointer uncertainty;

    // Processing Parameters
    bool ignoreZeros;
    double min;
    double max;

    // ITK Methods
    template <typename TPixel, unsigned int VImageDimension>
    void ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, double min, double max, mitk::Image::Pointer & result);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkTopXPercentThreshold(itk::Image<TPixel, VImageDimension>* itkImage, double percentage, double & lowerThreshold, double & upperThreshold);
};

#endif