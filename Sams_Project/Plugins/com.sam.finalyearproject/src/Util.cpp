#include "Util.h"
#include <sstream>
#include <iomanip>

std::string Util::StringFromDouble(double value) {
  std::ostringstream ss;
  ss << std::setprecision(2) << std::fixed << value;
  return(ss.str());
}