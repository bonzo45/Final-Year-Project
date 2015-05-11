/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date$
Version:   $Revision$ 
 
Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include <berryStarter.h>
#include <Poco/Util/MapConfiguration.h>

#include <QtGui/QApplication>
#include <iostream>
#include <QtGui/QMessageBox>

class SamApplication : public QApplication {
  public:
    SamApplication(int & a, char **& b) : QApplication(a, b) {
      // Important: Dynamically load the libCTKWidgetsPlugins.so library.
      addLibraryPath(QCoreApplication::applicationDirPath() + "/../../MITK-superbuild/CTK-build/CTK-build/bin");
    };

    bool notify(QObject * receiver, QEvent * event) {
      QString msg;
      try {
        return QApplication::notify(receiver, event);
      } catch (Poco::Exception& e) {
        msg = QString::fromStdString(e.displayText());
      } catch (std::exception& e) {
        msg = e.what();
      } catch (...) {
        msg = "Unknown exception";
      }

      QString text("Sorry... something has gone wrong. :-( \n\n");
      text += msg;
      QMessageBox::critical(0, "Error", text);
      return false;
    }
};

int main(int argc, char** argv)
{
  // Create a QApplication instance first
  SamApplication myApp(argc, argv);
  myApp.setApplicationName("Sams_App");
  myApp.setOrganizationName("DKFZ, Medical and Biological Informatics");

  // These paths replace the .ini file and are tailored for installation
  // packages created with CPack. If a .ini file is presented, it will
  // overwrite the settings in MapConfiguration
  Poco::Path basePath(argv[0]);
  basePath.setFileName("");
  
  Poco::Path provFile(basePath);
  provFile.setFileName("Sams_App.provisioning");

  Poco::Util::MapConfiguration* sbConfig(new Poco::Util::MapConfiguration());
  sbConfig->setString(berry::Platform::ARG_PROVISIONING, provFile.toString());
  sbConfig->setString(berry::Platform::ARG_APPLICATION, "org.mitk.qt.extapplication");
  return berry::Starter::Run(argc, argv, sbConfig);
  return 0;
}
