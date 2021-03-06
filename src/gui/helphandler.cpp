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

#include "gui/helphandler.h"

#include "gui/constants.h"

#include "logging/logginghandler.h"
#include "settings/settings.h"
#include "atools.h"
#include "logging/loggingdefs.h"

#include <QMessageBox>
#include <QApplication>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QSettings>

HelpHandler::HelpHandler(QWidget *parent)
  : parentWidget(parent)
{

}

HelpHandler::~HelpHandler()
{

}

void HelpHandler::about()
{
  QStringList logs = atools::logging::LoggingHandler::getLogFiles();
  QString logStr;

  for(const QString& log : logs)
  {
    QUrl url(QUrl::fromLocalFile(log));

    logStr += QString("<a href=\"%1\">%2<a><br/>").arg(url.toString()).arg(log);
  }

  QMessageBox::about(parentWidget,
                     tr("About %1").arg(QApplication::applicationName()),
                     tr("<p><b>%1</b></p>"
                          "<p>is a logbook viewer and search tool for FSX.</p>"
                            "<p>This software is licensed under "
                              "<a href=\"http://www.gnu.org/licenses/gpl-3.0\">GPL3</a> or any later version.</p>"
                                "<p>The source code for this application is available at "
                                  "<a href=\"https://github.com/albar965\">Github</a>.</p>"
                                    "<p><b>Copyright 2015-2016 Alexander Barthel (albar965@mailbox.org).</b></p>"
                                      "<p><hr/>Version %2 (revision %3)</p>"
                                        "<p>atools Version %4 (revision %5)</p>"
                                          "<p><hr/>%6:</p><p><i>%7</i></p>").
                     arg(QApplication::applicationName()).
                     arg(QApplication::applicationVersion()).
                     arg(GIT_REVISION).
                     arg(atools::version()).
                     arg(atools::gitRevision()).
                     arg(logs.size() > 1 ? tr("Logfiles") : tr("Logfile")).
                     arg(logStr));
}

void HelpHandler::aboutQt()
{
  QMessageBox::aboutQt(parentWidget, tr("About Qt"));
}

void HelpHandler::help()
{
  QString overrideLang =
    atools::settings::Settings::instance()->value(ll::constants::SETTINGS_LANGUAGE, QString()).toString();
  QString lang;

  if(overrideLang.isEmpty())
    lang = QLocale::system().bcp47Name().section('-', 0, 0);
  else
    lang = overrideLang;

  QString appPath = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
  QString helpPrefix(appPath + QDir::separator() + "help" + QDir::separator());
  QString helpSuffix(QString(QDir::separator()) + "index.html");

  QString helpFile(helpPrefix + lang + helpSuffix), defaultHelpFile(
    helpPrefix +
    ll::constants::DEFAULT_HELP_LANG +
    helpSuffix);

  QUrl url;
  if(QFileInfo::exists(helpFile))
    url = QUrl::fromLocalFile(helpFile);
  else if(QFileInfo::exists(defaultHelpFile))
    url = QUrl::fromLocalFile(defaultHelpFile);
  else
    QMessageBox::warning(parentWidget, QApplication::applicationName(), QString(
                           tr("Help file <i>%1</i> not found")).arg(QDir::toNativeSeparators(defaultHelpFile)));

  qDebug() << "Help file" << url;

  if(!url.isEmpty())
    if(!QDesktopServices::openUrl(url))
      QMessageBox::warning(parentWidget, QApplication::applicationName(), QString(
                             tr("Error opening help URL <i>%1</i>")).arg(url.toDisplayString()));
}
