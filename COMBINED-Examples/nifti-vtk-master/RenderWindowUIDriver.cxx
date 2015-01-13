#include <QApplication>
#include "RenderWindowUI.h"
 
int main(int argc, char** argv)
{
	cout << "Main: Creating Application..." << endl;
	QApplication app(argc, argv);

	cout << "Main: Creating UI..." << endl;
	RenderWindowUI renderWindowUI;

	cout << "Main: Displaying UI..." << endl;
	renderWindowUI.show();

	cout << "Main: Executing App..." << endl;
	return app.exec();
}