#include "SamsPointSet.h"

void SamsPointSet::SetSamsView(Sams_View * samsView) {
  this->samsView = samsView;
}

void SamsPointSet::OnPointSetChange() {
  std::cout << "Point set changed." << std::endl;
  samsView->PointSetChanged(this);
}