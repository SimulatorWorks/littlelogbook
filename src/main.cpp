/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "gui/mainwindow.h"
#include "gui/constants.h"

#include "settings/settings.h"
#include "logging/logginghandler.h"
#include "logging/loggingutil.h"
#include "gui/translator.h"
#include "exception.h"
#include "gui/errorhandler.h"
#include "logging/loggingdefs.h"

#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QSharedMemory>

int main(int argc, char *argv[])
{
  // Initialize the resources from atools static library
  Q_INIT_RESOURCE(atools);

  int retval = 0;
  QApplication app(argc, argv);
  QApplication::setWindowIcon(QIcon(":/littlelogbook/resources/icons/logbook.svg"));
  QCoreApplication::setApplicationName("Little Logbook");
  QCoreApplication::setOrganizationName("ABarthel");
  QCoreApplication::setOrganizationDomain("abarthel.org");
  QCoreApplication::setApplicationVersion("1.5.0");

  try
  {
    using atools::logging::LoggingHandler;
    using atools::logging::LoggingUtil;
    using atools::settings::Settings;
    using atools::gui::Translator;

    // Initialize logging and force logfiles into the system or user temp directory
    LoggingHandler::initializeForTemp(atools::settings::Settings::getOverloadedPath(
                                        ":/littlelogbook/resources/config/logging.cfg"));

    // Print some information which can be useful for debugging
    LoggingUtil::logSystemInformation();
    LoggingUtil::logStandardPaths();
    Settings::logSettingsInformation();

    Settings& s = Settings::instance();
    // Load local and Qt system translations from various places
    Translator::load(s->value(ll::constants::SETTINGS_LANGUAGE, QString()).toString());

#if defined(Q_OS_WIN32)
    // Detect other running application instance - this is unsafe on Unix since shm can remain after crashes
    QSharedMemory shared("7ac8013e-b1c4-4d7f-83ed-1fc201cf004c"); // generated GUID
    if(!shared.create(512, QSharedMemory::ReadWrite))
    {
      QMessageBox::critical(nullptr, QObject::tr("%1 - Error").arg(QApplication::applicationName()),
                            QObject::tr("%1 is already running.").arg(QApplication::applicationName()));
      return 1;
    }
#endif

    // Write version to configuration file
    QString oldVersion = s->value(ll::constants::SETTINGS_VERSION).toString();
    qInfo() << "Found version" << oldVersion << "in configuration file";
    s.getAndStoreValue(ll::constants::SETTINGS_VERSION, QCoreApplication::applicationVersion());
    s.syncSettings();

    MainWindow mainWindow;
    mainWindow.show();

    qDebug() << "Before app.exec()";
    retval = app.exec();

    qDebug() << "app.exec() done, retval is" << retval;
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(nullptr).handleException(e);
    retval = 1;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(nullptr).handleUnknownException();
    retval = 1;
  }
  return retval;
}
