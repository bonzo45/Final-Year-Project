#ifndef Uncertainty_Surface_Mapper_h
#define Uncertainty_Surface_Mapper_h

#include <mitkImage.h>
#include <mitkSurface.h>

class UncertaintySurfaceMapper {
  public:
    enum SAMPLING_DISTANCE {HALF, FULL};
    enum SCALING {NONE, LINEAR, HISTOGRAM};
    enum COLOUR {BLACK_AND_WHITE, BLACK_AND_RED};
    enum SAMPLING_ACCUMULATOR {AVERAGE, MINIMUM, MAXIMUM};

    UncertaintySurfaceMapper();
    void setUncertainty(mitk::Image::Pointer uncertainty);
    void setSurface(mitk::Surface::Pointer surface);
    void setSamplingDistance(SAMPLING_DISTANCE samplingDistance);
    void setScaling(SCALING scaling);
    void setColour(COLOUR colour);
    void setSamplingAccumulator(SAMPLING_ACCUMULATOR samplingAccumulator);
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
    SCALING scaling;
    COLOUR colour;
    SAMPLING_ACCUMULATOR samplingAccumulator;

    bool invertNormals;

    double legendMinValue, legendMaxValue;

    static const bool DEBUGGING = true;
};

#endif