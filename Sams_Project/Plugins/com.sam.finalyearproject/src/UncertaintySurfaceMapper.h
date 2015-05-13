#ifndef Uncertainty_Surface_Mapper_h
#define Uncertainty_Surface_Mapper_h

#include <mitkImage.h>
#include <mitkSurface.h>

class UncertaintySurfaceMapper {
  public:
    enum SAMPLING_DISTANCE {HALF, FULL};
    enum Scaling {NONE, LINEAR, HISTOGRAM};
    enum Colour {BLACK_AND_WHITE, COLOUR};
    enum SamplingAccumulator {AVERAGE, MINIMUM, MAXIMUM};

    UncertaintySurfaceMapper();
    void setUncertainty(mitk::Image::Pointer uncertainty);
    void setSurface(mitk::Surface::Pointer surface);
    void setSamplingDistance(SAMPLING_DISTANCE distance);
    void setSamplingHalf();
    void setScalingNone();
    void setScalingLinear();
    void setScalingHistogram();
    void setBlackAndWhite();
    void setColour();
    void setSamplingAverage();
    void setSamplingMinimum();
    void setSamplingMaximum();
    void setInvertNormals(bool invertNormals);
    void map();

    double getLegendMinValue();
    double getLegendMaxValue();
    void getLegendMinColour(char * colour);
    void getLegendMaxColour(char * colour);

  private:
    mitk::Image::Pointer uncertainty;
    mitk::Surface::Pointer surface;

    unsigned int uncertaintyHeight, uncertaintyWidth, uncertaintyDepth;

    SAMPLING_DISTANCE samplingDistance;

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

    bool invertNormals;

    double legendMinValue, legendMaxValue;

    static const bool DEBUGGING = false;
};

#endif