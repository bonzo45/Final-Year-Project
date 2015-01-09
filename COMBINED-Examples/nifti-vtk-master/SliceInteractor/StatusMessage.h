#include <string>
#include <iostream>
#include <sstream>

// helper class to format slice status message
class StatusMessage {
public:
   static std::string Format(int slice, int maxSlice);
};