#ifndef Uncertainty_Texture_h
#define Uncertainty_Texture_h

#include <mitkImage.h>
#include <itkImage.h>

typedef itk::Image<unsigned char, 2>  TextureImageType;


class UncertaintyTexture {
	public:
    void setUncertainty(mitk::Image::Pointer image);
    void setDimensions(unsigned int width, unsigned int height);
    void setScalingLinear(bool scalingLinear);
    void clearSampling();
    void setSamplingAverage();
    void setSamplingMinimum();
    void setSamplingMaximum();
    mitk::Image::Pointer generateUncertaintyTexture();

  private:
    mitk::Image::Pointer uncertainty;
    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;

    unsigned int textureWidth, textureHeight;

    bool scalingLinear;

    bool samplingAverage, samplingMinimum, samplingMaximum;
};

#endif