#include <QApplication>
#include "RenderWindowUI.h"
 
int main(int argc, char** argv)
{
  QApplication app(argc, argv);
 
  RenderWindowUI renderWindowUI;
  renderWindowUI.show();
 
  return app.exec();
}