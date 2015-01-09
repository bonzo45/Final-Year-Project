#include <StatusMessage.h>

std::string StatusMessage::Format(int slice, int maxSlice) {
  std::stringstream tmp;
  tmp << "Slice Number  " << slice + 1 << "/" << maxSlice + 1;
  return tmp.str();
}