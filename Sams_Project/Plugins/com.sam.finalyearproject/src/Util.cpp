#include <string>
#include <sstream>
#include <iomanip>

std::string StringFromDouble(double value) {
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << value;
  return(ss.str());
}