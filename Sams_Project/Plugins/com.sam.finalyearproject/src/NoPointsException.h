#ifndef No_Points_Exception_h
#define No_Points_Exception_h

class NoPointsException : public std::runtime_error {
  public:
    NoPointsException() : std::runtime_error("No points are available for SVD.") {

    }
};

#endif