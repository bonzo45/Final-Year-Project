#ifndef Uncertainty_Thresholder_h
#define Uncertainty_Thresholder_h

#include <mitkImage.h>

class UncertaintyThresholder {
	public:
    UncertaintyThresholder();
    ~UncertaintyThresholder();
    void setUncertainty(mitk::Image::Pointer image);
    void setIgnoreZeros(bool ignoreZeros);
    mitk::Image::Pointer thresholdUncertainty(double min, double max);
    void getTopXPercentThreshold(double percentage, double & min, double & max);

  private:
    mitk::Image::Pointer uncertainty;

    // Processing Parameters
    bool ignoreZeros;
    double min;
    double max;

    // Histogram (so we don't have to keep computing it)
    unsigned int * histogram;
    unsigned int totalPixels;

    unsigned int measurementComponents;
    unsigned int binsPerDimension;

    static const bool DEBUGGING = false;

    // ITK Methods
    template <typename TPixel, unsigned int VImageDimension>
    void ItkThresholdUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, double min, double max, mitk::Image::Pointer & result);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkComputePercentages(itk::Image<TPixel, VImageDimension>* itkImage, unsigned int * histogram, unsigned int & totalPixels);
};

#endif