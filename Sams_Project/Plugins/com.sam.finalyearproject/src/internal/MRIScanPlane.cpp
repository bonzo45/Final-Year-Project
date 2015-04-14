class MRIScanPlane {
    unsigned int sliceWidth;
    unsigned int sliceHeight;
    unsigned int sliceCount;

    double spacing;
    double sliceThickness;

    vtkVector<float, 3> plane;

  public:
    void set_values (int,int);
    int area (void);
};