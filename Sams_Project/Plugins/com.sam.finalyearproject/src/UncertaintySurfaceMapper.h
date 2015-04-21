#ifndef Uncertainty_Surface_Mapper_h
#define Uncertainty_Surface_Mapper_h

#include <mitkImage.h>
#include <mitkSurface.h>

class UncertaintySurfaceMapper {
  public:
    UncertaintySurfaceMapper();
    void setUncertainty(mitk::Image::Pointer uncertainty);
    void setSurface(mitk::Surface::Pointer surface);
    void setSamplingFull();
    void setSamplingHalf();
    void setScalingNone();
    void setScalingLinear();
    void setScalingHistogram();
    void setBlackAndWhite();
    void setColour();
    void setSamplingAverage();
    void setSamplingMinimum();
    void setSamplingMaximum();
    void map();

  private:
    mitk::Image::Pointer uncertainty;
    mitk::Surface::Pointer surface;

    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;

    bool samplingFull;
    bool samplingHalf;

    bool scalingNone;
    bool scalingLinear;
    bool scalingHistogram;
    void clearScaling();

    bool blackAndWhite;
    bool colour;

    bool samplingAverage;
    bool samplingMinimum;
    bool samplingMaximum;
    void clearSampling();

    static const bool DEBUGGING = false;
};

#endif