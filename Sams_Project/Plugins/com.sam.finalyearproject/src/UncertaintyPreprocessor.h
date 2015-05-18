#ifndef Uncertainty_Processor_h
#define Uncertainty_Processor_h

#include <mitkImage.h>

class UncertaintyPreprocessor {
	public:
    void setScan(mitk::Image::Pointer image);
    void setUncertainty(mitk::Image::Pointer image);
    void setNormalizationParams(double min, double max);
    void setErodeParams(int erodeThickness, double erodeThreshold, int dilateThickness);
    mitk::Image::Pointer preprocessUncertainty(bool invert, bool erode, bool align);

  private:
    mitk::Image::Pointer scan;
    mitk::Image::Pointer uncertainty;

    // Preprocessing parameters
    double normalizationMin;
    double normalizationMax;
    
    int erodeErodeThickness;
    double erodeThreshold;
    int erodeDilateThickness;

    // ITK Methods
    template <typename TPixel, unsigned int VImageDimension>
    void ItkNormalizeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkInvertUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result);
    template <typename TPixel, unsigned int VImageDimension>
    void ItkErodeUncertainty(itk::Image<TPixel, VImageDimension>* itkImage, mitk::Image::Pointer & result);
};

#endif