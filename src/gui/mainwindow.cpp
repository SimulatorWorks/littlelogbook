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

#include "mainwindow.h"
#include "pathdialog.h"

#include "table/colum.h"
#include "table/controller.h"
#include "fs/ap/airportloader.h"
#include "fs/lb/logbookloader.h"
#include "fs/lb/logbookentryfilter.h"
#include "fs/lb/types.h"
#include "fs/fspaths.h"
#include "globalstats.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "export/csvexporter.h"
#include "export/htmlexporter.h"
#include "gui/translator.h"
#include "helphandler.h"
#include "logging/logginghandler.h"
#include "settings/settings.h"
#include "table/sqlmodel.h"
#include "sql/sqlutil.h"
#include "ui_mainwindow.h"
#include "constants.h"
#include "logging/loggingdefs.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

using atools::sql::SqlDatabase;
using atools::settings::Settings;
using atools::gui::ErrorHandler;
using atools::gui::Dialog;
using atools::fs::FsPaths;
using atools::fs::SimulatorType;

MainWindow::MainWindow() :
  QMainWindow(nullptr), ui(new Ui::MainWindow)
{
  qDebug() << "MainWindow constructor";
  dialog = new atools::gui::Dialog(this);
  errorHandler = new atools::gui::ErrorHandler(this);

  ui->setupUi(this);

  openDatabase();

  // Can not be set in Qt Designer
  ui->tableView->horizontalHeader()->setSectionsMovable(true);
  ui->tableView->addAction(ui->actionTableCopy);

  // Create label for the statusbar
  selectionLabelText = tr("%1 of %2 entries selected, %3 visible.");
  selectionLabel = new QLabel();
  ui->statusBar->addPermanentWidget(selectionLabel);

  // Read configuration file
  readSettings();
  pathSettings.readSettings();

  controller = new Controller(this, &db, ui->tableView);
  csvExporter = new CsvExporter(this, controller);
  htmlExporter = new HtmlExporter(this, controller);
  globalStats = new GlobalStats(this, &db);
  helpHandler = new HelpHandler(this);

  updateDatabaseStatus();
  if(hasLogbook)
    postDatabaseLoad();

  connectAllSlots();

  updateActionStates();

  updateWidgetStatus();
  updateWidgetsOnSelection();
  updateGlobalStats();

  qDebug() << "MainWindow constructor returning";
}

MainWindow::~MainWindow()
{
  qDebug() << "MainWindow destructor";

  delete globalStats;
  delete csvExporter;
  delete htmlExporter;
  delete controller;
  delete ui;
  delete helpHandler;
  delete dialog;
  delete errorHandler;

  Settings::shutdown();
  atools::gui::Translator::unload();

  closeDatabase();

  qDebug() << "MainWindow destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();
}

void MainWindow::updateActionStates()
{
  qDebug() << "Updating action states";
  ui->actionShowToolbar->setChecked(!ui->mainToolBar->isHidden());
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
  ui->actionShowSearch->setChecked(!ui->fromAirportLineEdit->isHidden());
  ui->actionShowStatistics->setChecked(!ui->dockWidget->isHidden());
}

void MainWindow::assignControllerSlots()
{
  if(hasLogbook)
  {
    // Assign edit fields to controller / column descriptor to allow automatic
    // filtering
    controller->assignLineEdit("airport_from_icao", ui->fromAirportLineEdit);
    controller->assignLineEdit("airport_to_icao", ui->toAirportLineEdit);

    if(hasAirports)
      assignAirportLineEdits();

    controller->assignLineEdit("description", ui->descriptionLineEdit);
    controller->assignLineEdit("aircraft_reg", ui->aircraftRegLineEdit);
    controller->assignLineEdit("aircraft_descr", ui->aircraftDescrLineEdit);

    controller->assignComboBox("aircraft_type", ui->aircraftTypeComboBox);
    controller->assignComboBox("aircraft_flags", ui->aircraftInfoComboBox);
  }
}

void MainWindow::connectAllSlots()
{
  qDebug() << "Connecting slots";
  connect(ui->tableView, &QTableView::customContextMenuRequested, this, &MainWindow::tableContextMenu);
  connect(this, &MainWindow::windowShown, this, &MainWindow::startupChecks, Qt::QueuedConnection);

  connectControllerSlots();

  /* *INDENT-OFF* */
  connect(ui->fromAirportLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_from_icao", text); });
  connect(ui->toAirportLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_to_icao", text); });

  connect(ui->fromAirportNameLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_from_name", text); });
  connect(ui->fromCityLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_from_city", text); });
  connect(ui->fromStateLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_from_state", text); });
  connect(ui->fromCountryLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_from_country", text); });
  connect(ui->toAirportNameLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_to_name", text); });
  connect(ui->toCityLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_to_city", text); });
  connect(ui->toStateLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_to_state", text); });
  connect(ui->toCountryLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("airport_to_country", text); });

  connect(ui->descriptionLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("description", text); });
  connect(ui->aircraftRegLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("aircraft_reg", text); });
  connect(ui->aircraftDescrLineEdit, &QLineEdit::textEdited,
          [=](const QString& text) {controller->filterByLineEdit("aircraft_descr", text); });

  // Need to put this in a separate variable since there are two activated methods
  void (QComboBox::* activatedPtr)(int) = &QComboBox::activated;
  connect(ui->aircraftTypeComboBox, activatedPtr,
          [=](int index) {controller->filterByComboBox("aircraft_type", index); });
  connect(ui->aircraftInfoComboBox, activatedPtr,
          [=](int index) {controller->filterByComboBox("aircraft_flags",
                                                       index == 1 ? atools::fs::lb::types::AIRCRAFT_FLAG_MULTIMOTOR : 0); });
  connect(ui->conditionComboBox, activatedPtr,
          [=](int index) {index == 0 ? controller->filterOperatorAll(true) : controller->filterOperatorAny(true); });
  connect(ui->conditionComboBox, activatedPtr,
          [=](int index) {index == 1 ? controller->filterOperatorAny(true) : controller->filterOperatorAll(true); });
  /* *INDENT-ON* */

  assignControllerSlots();

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionTableCopy, &QAction::triggered, this, &MainWindow::tableCopyCipboard);

  // File menu
  connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
  connect(ui->actionOpenLogbook, &QAction::triggered, this, &MainWindow::pathDialog);

  // Export menu
  connect(ui->actionExportAllCsv, &QAction::triggered, this, &MainWindow::exportAllCsv);
  connect(ui->actionExportSelectedCsv, &QAction::triggered, this, &MainWindow::exportSelectedCsv);
  connect(ui->actionExportAllHtml, &QAction::triggered, this, &MainWindow::exportAllHtml);
  connect(ui->actionExportSelectedHtml, &QAction::triggered, this, &MainWindow::exportSelectedHtml);

  // View menu
  connect(ui->actionShowAll, &QAction::triggered, this, &MainWindow::loadAllRowsIntoView);
  connect(ui->actionResetSearch, &QAction::triggered, this, &MainWindow::resetSearch);
  connect(ui->actionResetView, &QAction::triggered, this, &MainWindow::resetView);
  connect(ui->actionUngroup, &QAction::triggered, this, &MainWindow::ungroup);
  connect(ui->actionShowToolbar, &QAction::toggled, ui->mainToolBar, &QToolBar::setVisible);
  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);
  connect(ui->actionShowStatistics, &QAction::toggled, ui->dockWidget, &QDockWidget::setVisible);
  connect(ui->actionShowSearch, &QAction::toggled, this, &MainWindow::showSearchBar);

  // Extras menu
  connect(ui->actionResetMessages, &QAction::triggered, this, &MainWindow::resetMessages);
  connect(ui->actionFilterLogbookEntries, &QAction::toggled, this, &MainWindow::filterLogbookEntries);

  // Help menu
  connect(ui->actionAbout, &QAction::triggered, helpHandler, &HelpHandler::about);
  connect(ui->actionAboutQt, &QAction::triggered, helpHandler, &HelpHandler::aboutQt);
  connect(ui->actionHelp, &QAction::triggered, helpHandler, &HelpHandler::help);

  // Connect widget status to actions
  connect(ui->mainToolBar, &QToolBar::visibilityChanged, ui->actionShowToolbar, &QAction::setChecked);
  connect(ui->dockWidget, &QDockWidget::visibilityChanged, ui->actionShowStatistics, &QAction::setChecked);
}

void MainWindow::pathDialog()
{
  PathDialog d(this, &pathSettings);
  d.exec();
  SimulatorType type = atools::fs::FSX;

  if(d.hasLogbookFileChanged(type) || d.hasRunwaysFileChanged(type))
  {
    preDatabaseLoad();

    if(d.hasRunwaysFileChanged(type))
    {
      checkRunwaysFile(type, false);
      checkLogbookFile(type, false);
    }
    else if(d.hasLogbookFileChanged(type))
      checkLogbookFile(type, false);

    postDatabaseLoad();
  }
}

void MainWindow::startupChecks()
{
  SimulatorType type = atools::fs::FSX;

  bool notifyReload = true;
  if(!pathSettings.isLogbookFileValid(type))
  {
    notifyReload = false;

    PathDialog d(this, &pathSettings);
    d.exec();
  }

  if(pathSettings.hasLogbookFileChanged(type) || pathSettings.hasRunwaysFileChanged(type))
  {
    preDatabaseLoad();
    checkRunwaysFile(type, notifyReload);
    updateDatabaseStatus();
    checkLogbookFile(type, notifyReload);
    postDatabaseLoad();
  }
}

void MainWindow::exportAllCsv()
{
  int exported = csvExporter->exportAll(ui->actionOpenAfterExport->isChecked());
  ui->statusBar->showMessage(QString(tr("Exported %1 logbook entries to CSV document.")).arg(exported));
}

void MainWindow::exportSelectedCsv()
{
  int exported = csvExporter->exportSelected(ui->actionOpenAfterExport->isChecked());
  ui->statusBar->showMessage(QString(tr("Exported %1 logbook entries to CSV document.")).arg(exported));
}

void MainWindow::exportAllHtml()
{
  int exported = htmlExporter->exportAll(ui->actionOpenAfterExport->isChecked());
  ui->statusBar->showMessage(QString(tr("Exported %1 logbook entries to HTML document.")).arg(exported));
}

void MainWindow::exportSelectedHtml()
{
  int exported = htmlExporter->exportSelected(ui->actionOpenAfterExport->isChecked());
  ui->statusBar->showMessage(QString(tr("Exported %1 logbook entries to HTML document.")).arg(exported));
}

void MainWindow::updateGlobalStats()
{
  ui->globalStatsTextEdit->setHtml(globalStats->createGlobalStatsReport(hasLogbook, hasAirports));
}

void MainWindow::resetView()
{
  int result = dialog->showQuestionMsgBox(ll::constants::SETTINGS_SHOW_RESET_VIEW,
                                          tr("Reset sort order, column order and column sizes to default?"),
                                          tr("Do not &show this dialog again."),
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    qDebug() << "resetView";

    controller->resetView();
    ui->statusBar->showMessage(tr("View reset to default."));
    ui->actionUngroup->setEnabled(false);
    ui->conditionComboBox->setEnabled(true);
  }
}

void MainWindow::resetSearch()
{
  qDebug() << "resetSearch";
  controller->resetSearch();
  ui->statusBar->showMessage(tr("Search filters cleared."));
}

void MainWindow::loadAllRowsIntoView()
{
  qDebug() << "loadAll";
  controller->loadAllRows();
  ui->statusBar->showMessage(tr("All logbook entries read."));
}

void MainWindow::ungroup()
{
  qDebug() << "ungroup";
  controller->ungroup();
  ui->statusBar->showMessage(tr("Grouping released."));
  ui->actionUngroup->setEnabled(false);
  ui->conditionComboBox->setEnabled(true);
}

void MainWindow::resetMessages()
{
  qDebug() << "resetMessages";
  Settings& s = Settings::instance();

  // Show all message dialogs again
  s->setValue(ll::constants::SETTINGS_SHOW_QUIT, true);
  s->setValue(ll::constants::SETTINGS_SHOW_RELOAD, true);
  s->setValue(ll::constants::SETTINGS_SHOW_NO_RUNWAYS, true);
  s->setValue(ll::constants::SETTINGS_SHOW_RELOAD_RUNWAYS, true);
  s->setValue(ll::constants::SETTINGS_SHOW_RESET_VIEW, true);
  s->setValue(ll::constants::SETTINGS_SHOW_FILTER_RELOAD, true);
  s.syncSettings();

  ui->statusBar->showMessage(tr("All message dialogs reset."));
}

void MainWindow::connectControllerSlots()
{
  /* *INDENT-OFF* */
  controller->connectModelReset([=]() { this->updateWidgetsOnSelection(); });
  controller->connectFetchedMore([=]() { this->updateWidgetsOnSelection(); });
  /* *INDENT-ON* */

  if(ui->tableView->selectionModel() != nullptr)
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::tableSelectionChanged);
}

void MainWindow::assignAirportLineEdits()
{
  controller->assignLineEdit("airport_from_name", ui->fromAirportNameLineEdit);
  controller->assignLineEdit("airport_from_city", ui->fromCityLineEdit);
  controller->assignLineEdit("airport_from_state", ui->fromStateLineEdit);
  controller->assignLineEdit("airport_from_country", ui->fromCountryLineEdit);

  controller->assignLineEdit("airport_to_name", ui->toAirportNameLineEdit);
  controller->assignLineEdit("airport_to_city", ui->toCityLineEdit);
  controller->assignLineEdit("airport_to_state", ui->toStateLineEdit);
  controller->assignLineEdit("airport_to_country", ui->toCountryLineEdit);
}

void MainWindow::cleanAirportLineEdits()
{
  controller->assignLineEdit("airport_from_name", nullptr);
  controller->assignLineEdit("airport_from_city", nullptr);
  controller->assignLineEdit("airport_from_state", nullptr);
  controller->assignLineEdit("airport_from_country", nullptr);

  controller->assignLineEdit("airport_to_name", nullptr);
  controller->assignLineEdit("airport_to_city", nullptr);
  controller->assignLineEdit("airport_to_state", nullptr);
  controller->assignLineEdit("airport_to_country", nullptr);
}

void MainWindow::openDatabase()
{
  try
  {
    using atools::sql::SqlDatabase;

    // Get a file in the organization specific directory with an application
    // specific name (i.e. Linux: ~/.config/ABarthel/little_logbook.sqlite)
    databaseFile = atools::settings::Settings::getConfigFilename(".sqlite");

    qDebug() << "Opening database" << databaseFile;
    db = SqlDatabase::addDatabase(ll::constants::DATABASE_TYPE);
    db.setDatabaseName(databaseFile);
    db.open();
  }
  catch(std::exception& e)
  {
    errorHandler->handleException(e, "While opening database");
  }
  catch(...)
  {
    errorHandler->handleUnknownException("While opening database");
  }
}

void MainWindow::closeDatabase()
{
  try
  {
    using atools::sql::SqlDatabase;

    qDebug() << "Closing database" << databaseFile;
    db.close();
  }
  catch(std::exception& e)
  {
    errorHandler->handleException(e, "While closing database");
  }
  catch(...)
  {
    errorHandler->handleUnknownException("While closing database");
  }
}

void MainWindow::checkRunwaysFile(SimulatorType type, bool notifyChange)
{
  if(!pathSettings.isRunwaysFileValid(type))
  {
    qDebug() << "Found runways file" << pathSettings.getRunwaysFile(type);

    dialog->showInfoMsgBox(ll::constants::SETTINGS_SHOW_NO_RUNWAYS,
                           QString(tr("Runways file<br/><i>%1</i><br/>not found.<br/>"
                                      "Get Peter Dowson's Make Runways utility and run it in your FSX folder "
                                      "to show airport, city, state and country names.<br/>"
                                      "<a href=\"http://www.schiratti.com/dowson.html\">"
                                        "Download the Make Runways utility here.</a><br/>"
                                        "Note that this is optional.")).
                           arg(QDir::toNativeSeparators(pathSettings.getRunwaysFile(type))),
                           tr("Do not &show this dialog again."));
  }
  else
  {
    if(pathSettings.hasRunwaysFileChanged(type))
    {
      // File was changed
      qDebug() << "Found runways file changed" << pathSettings.getRunwaysFile(type);

      if(notifyChange)
        dialog->showInfoMsgBox(ll::constants::SETTINGS_SHOW_RELOAD_RUNWAYS,
                               QString(tr("Runways file<br/><i>%1</i><br/> is new or has changed.<br/>"
                                          "Will reload now.")).
                               arg(QDir::toNativeSeparators(pathSettings.getRunwaysFile(type))),
                               tr("Do not &show this dialog again."));

      // If true is returned the logbook will be reloaded too
      loadAirports(type);
    }
    else
      qDebug() << "checkRunwaysFile: nothing to do";
  }
}

void MainWindow::checkLogbookFile(SimulatorType type, bool notifyChange)
{
  // Windows 7 for FSX boxed is
  // c:\Users\alex\Documents\Flight Simulator X Files\Logbook.BIN
  if(!pathSettings.isLogbookFileValid(type))
  {
    // File not found or not set yet - let the user select a new one
    qDebug() << "Need new logbook file. logbookFilename:" << pathSettings.getLogbookFile(type);

    dialog->showInfoMsgBox(ll::constants::SETTINGS_SHOW_NO_RUNWAYS,
                           QString(tr("Logbook file<br/><i>%1</i><br/>not found.")).
                           arg(QDir::toNativeSeparators(pathSettings.getLogbookFile(type))),
                           tr("Do not &show this dialog again."));
  }
  else
  {
    if(pathSettings.hasLogbookFileChanged(type))
    {
      if(notifyChange)
        dialog->showInfoMsgBox(ll::constants::SETTINGS_SHOW_RELOAD,
                               QString(tr("Logbook file<br/><i>%1</i><br/> is new or has changed.<br/>"
                                          "Will reload now.")).
                               arg(QDir::toNativeSeparators(pathSettings.getLogbookFile(type))),
                               tr("Do not &show this dialog again."));

      loadLogbookDatabase(type);
    }
    else
      qDebug() << "checkLogbookFile: nothing to do";
  }
}

void MainWindow::updateWidgetStatus()
{
  showHideAirportLineEdits(ui->actionShowSearch->isChecked());

  // Disable/enable all line edit widgets in the first line of the search bar
  for(int i = 0; i < ui->gridLayoutSearch->count(); ++i)
  {
    QWidget *w = ui->gridLayoutSearch->itemAt(i)->widget();
    if(w != nullptr)
      w->setEnabled(hasLogbook);
  }

  // Disable/enable all line edit widgets and combo boxes in the second
  // line of the search bar
  for(int i = 0; i < ui->gridLayoutSearch2->count(); ++i)
  {
    QWidget *w = ui->gridLayoutSearch2->itemAt(i)->widget();
    if(w != nullptr)
      w->setEnabled(hasLogbook);
  }

  // Only enable these if there are logbook entries
  ui->actionShowAll->setEnabled(hasLogbook);
  ui->actionResetSearch->setEnabled(hasLogbook);
  ui->actionResetView->setEnabled(hasLogbook);
  ui->actionReloadLogbook->setEnabled(hasLogbook);
  ui->actionExportAllCsv->setEnabled(hasLogbook);
  ui->actionExportAllHtml->setEnabled(hasLogbook);
  ui->conditionComboBox->setEnabled(hasLogbook);

  // Update export menu if there is a selection in the table view
  QItemSelectionModel *sm = ui->tableView->selectionModel();
  ui->actionExportSelectedCsv->setEnabled(hasLogbook && sm != nullptr && sm->hasSelection());
  ui->actionExportSelectedHtml->setEnabled(hasLogbook && sm != nullptr && sm->hasSelection());

  ui->actionUngroup->setEnabled(hasLogbook && controller->isGrouped());
}

void MainWindow::tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(selected);
  Q_UNUSED(deselected);
  updateWidgetsOnSelection();
}

void MainWindow::updateWidgetsOnSelection()
{
  // Update export menu if there is a selection in the table view
  QItemSelectionModel *sm = ui->tableView->selectionModel();
  ui->actionExportSelectedCsv->setEnabled(hasLogbook && sm != nullptr && sm->hasSelection());
  ui->actionExportSelectedHtml->setEnabled(hasLogbook && sm != nullptr && sm->hasSelection());

  // Update show all action - disable if everything is already shown
  ui->actionShowAll->setEnabled(controller->getTotalRowCount() != controller->getVisibleRowCount());

  // Update status bar
  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  selectionLabel->setText(selectionLabelText.arg(selectedRows).
                          arg(controller->getTotalRowCount()).
                          arg(controller->getVisibleRowCount()));
}

void MainWindow::showSearchBar(bool visible)
{
  // Show or hide first line of line edits
  qDebug() << "showSearchBar";
  for(int i = 0; i < ui->gridLayoutSearch->count(); ++i)
  {
    QWidget *w = ui->gridLayoutSearch->itemAt(i)->widget();
    if(w != nullptr)
      w->setVisible(visible);
  }

  // Show or hide second line of line edits and combo boxes
  for(int i = 0; i < ui->gridLayoutSearch2->count(); ++i)
  {
    QWidget *w = ui->gridLayoutSearch2->itemAt(i)->widget();
    if(w != nullptr)
      w->setVisible(visible);
  }

  // Hide additional edits if runways.xml is not available
  showHideAirportLineEdits(visible);
}

void MainWindow::showHideAirportLineEdits(bool visible)
{
  bool show = visible && hasAirports;
  ui->fromAirportNameLineEdit->setVisible(show);
  ui->fromCityLineEdit->setVisible(show);
  ui->fromStateLineEdit->setVisible(show);
  ui->fromCountryLineEdit->setVisible(show);
  ui->toAirportNameLineEdit->setVisible(show);
  ui->toCityLineEdit->setVisible(show);
  ui->toStateLineEdit->setVisible(show);
  ui->toCountryLineEdit->setVisible(show);
}

void MainWindow::filterLogbookEntries()
{
  SimulatorType type = atools::fs::FSX;
  if(hasLogbook)
  {
    dialog->showInfoMsgBox(ll::constants::SETTINGS_SHOW_FILTER_RELOAD,
                           tr("Logbooks will be reloaded."),
                           tr("Do not &show this dialog again."));

    pathSettings.invalidateLogbookFile(type);
    preDatabaseLoad();
    checkLogbookFile(type, false);
    postDatabaseLoad();
  }
}

void MainWindow::preDatabaseLoad()
{
  controller->resetSearch();
  controller->clearModel();
}

void MainWindow::updateDatabaseStatus()
{
  hasLogbook = atools::sql::SqlUtil(&db).hasTableAndRows("logbook");
  // Check if runways.xml was loaded
  hasAirports = atools::sql::SqlUtil(&db).hasTableAndRows("airport");
}

void MainWindow::postDatabaseLoad()
{
  // Check if there are any logbook entries at all to disable most GUI elements
  updateDatabaseStatus();

  controller->setHasLogbook(hasLogbook);
  controller->setHasAirports(hasAirports);

  controller->prepareModel();
  connectControllerSlots();

  updateWidgetsOnSelection();
  updateWidgetStatus();
  updateGlobalStats();
}

void MainWindow::readSettings()
{
  qDebug() << "readSettings";

  Settings& s = Settings::instance();

  if(s->contains(ll::constants::SETTINGS_MAINWINDOW_SIZE))
    resize(s->value(ll::constants::SETTINGS_MAINWINDOW_SIZE, sizeHint()).toSize());

  if(s->contains(ll::constants::SETTINGS_MAINWINDOW_STATE))
    restoreState(s->value(ll::constants::SETTINGS_MAINWINDOW_STATE).toByteArray());

  ui->statusBar->setHidden(!s->value(ll::constants::SETTINGS_SHOW_STATUSBAR, true).toBool());

  showSearchBar(s->value(ll::constants::SETTINGS_SHOW_SEARCHOOL, true).toBool());

  ui->actionOpenAfterExport->setChecked(s->value(ll::constants::SETTINGS_EXPORT_OPEN, true).toBool());

  ui->actionFilterLogbookEntries->setChecked(s->value(ll::constants::SETTINGS_FILTER_ENTRIES, false).toBool());
}

void MainWindow::writeSettings()
{
  qDebug() << "writeSettings";

  Settings& s = Settings::instance();
  s->setValue(ll::constants::SETTINGS_MAINWINDOW_SIZE, size());
  s->setValue(ll::constants::SETTINGS_MAINWINDOW_STATE, saveState());
  s->setValue(ll::constants::SETTINGS_SHOW_STATUSBAR, !ui->statusBar->isHidden());

  s->setValue(ll::constants::SETTINGS_SHOW_SEARCHOOL, !ui->fromAirportLineEdit->isHidden());
  s->setValue(ll::constants::SETTINGS_EXPORT_OPEN, ui->actionOpenAfterExport->isChecked());

  s->setValue(ll::constants::SETTINGS_FILTER_ENTRIES, ui->actionFilterLogbookEntries->isChecked());
  s.syncSettings();
}

bool MainWindow::loadAirports(SimulatorType type)
{
  bool success = true;
  qDebug() << "Starting airport import...";

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  atools::fs::ap::AirportLoader apLoader(&db);

  try
  {
    apLoader.loadAirports(pathSettings.getRunwaysFile(type));
  }
  catch(std::exception& e)
  {
    success = false;
    errorHandler->handleException(e, "While loading logbook");
  }
  catch(...)
  {
    success = false;
    errorHandler->handleUnknownException("While loading logbook");
  }

  QGuiApplication::restoreOverrideCursor();

  if(success)
  {
    qDebug() << "Airport import done";
    ui->statusBar->showMessage(QString(tr("Loaded %1 airports.")).arg(apLoader.getNumLoaded()));
    pathSettings.setRunwaysFileLoaded(type);
    pathSettings.invalidateLogbookFile(type);
  }
  else
  {
    qDebug() << "Airport import failed";
    ui->statusBar->showMessage(QString(tr("Airport import failed.")));
  }
  return success;
}

bool MainWindow::loadLogbookDatabase(SimulatorType type)
{
  using namespace atools::fs::lb;

  bool success = true;
  qDebug() << "Starting logbook import...";

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  /* All entries pass default filter */
  LogbookEntryFilter filter;

  if(ui->actionFilterLogbookEntries->isChecked())
  {
  Settings& s = Settings::instance();

  if(s.getAndStoreValue(ll::constants::SETTINGS_FILTER_INVALID_DATE, true).toBool())
    filter.invalidDate();

  if(s.getAndStoreValue(ll::constants::SETTINGS_FILTER_START_AND_DEST_EMPTY, true).toBool())
    filter.startAndDestEmpty();

  if(s.getAndStoreValue(ll::constants::SETTINGS_FILTER_START_OR_DEST_EMPTY, false).toBool())
    filter.startOrDestEmpty();

  if(s.getAndStoreValue(ll::constants::SETTINGS_FILTER_START_DEST_SAME, true).toBool())
    filter.startAndDestSame();

  int minFlightTimeMins = s.getAndStoreValue(ll::constants::SETTINGS_FILTER_MIN_FLIGH_TIME, 5).toInt();
  if(minFlightTimeMins)
    filter.flightTimeLowerThan(minFlightTimeMins);

  s.syncSettings();
  }

  LogbookLoader importer(&db);

  try
  {
    importer.loadLogbook(pathSettings.getLogbookFile(type), filter, false /* append */);
  }
  catch(std::exception& e)
  {
    success = false;
    errorHandler->handleException(e, "While loading logbook");
  }
  catch(...)
  {
    success = false;
    errorHandler->handleUnknownException("While loading logbook");
  }

  QGuiApplication::restoreOverrideCursor();

  if(success)
  {
    qDebug() << "logbook import done";
    ui->statusBar->showMessage(QString(tr("Loaded %1 logbook entries.")).arg(importer.getNumLoaded()));
    pathSettings.setLogbookFileLoaded(type);
  }
  else
  {
    qDebug() << "Logbook import failed";
    ui->statusBar->showMessage(QString(tr("Logbook import failed.")));
  }
  return success;
}

void MainWindow::tableCopyCipboard()
{
  qDebug() << "tableCopyCipboard";
  QString rows;
  int exported = csvExporter->exportSelectedToString(&rows);
  QApplication::clipboard()->setText(rows);
  ui->statusBar->showMessage(QString(tr("Copied %1 logbook entries to clipboard.")).arg(exported));
}

void MainWindow::tableContextMenu(const QPoint& pos)
{
  // Do not show menu if no logbook is loaded
  if(hasLogbook)
  {
    QString header, fieldData;
    bool columnCanFilter = false, columnCanGroup = false;

    QModelIndex index = controller->getModelIndexAt(pos);
    if(index.isValid())
    {
      const Column *columnDescriptor = controller->getColumn(index.column());
      Q_ASSERT(columnDescriptor != nullptr);
      columnCanFilter = columnDescriptor->isFilter();
      columnCanGroup = columnDescriptor->isGroup();

      if(columnCanGroup)
      {
        header = controller->getHeaderNameAt(index);
        Q_ASSERT(!header.isNull());
        // strip LF and other from header name
        header.replace("-\n", "").replace("\n", " ");
      }

      if(columnCanFilter)
        // Disabled menu items don't need any content
        fieldData = controller->getFieldDataAt(index);
    }
    else
      qDebug() << "Invalid index at" << pos;

    // Build the menu
    QMenu menu;

    menu.addAction(ui->actionTableCopy);
    ui->actionTableCopy->setEnabled(index.isValid());

    menu.addAction(ui->actionTableSelectAll);
    ui->actionTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);

    menu.addSeparator();
    menu.addAction(ui->actionResetView);
    menu.addAction(ui->actionResetSearch);
    menu.addAction(ui->actionShowAll);

    QString actionFilterIncludingText, actionFilterExcludingText, actionGroupByColText;
    actionFilterIncludingText = ui->actionFilterIncluding->text();
    actionFilterExcludingText = ui->actionFilterExcluding->text();
    actionGroupByColText = ui->actionGroupByCol->text();

    // Add data to menu item text
    ui->actionFilterIncluding->setText(ui->actionFilterIncluding->text().arg(fieldData));
    ui->actionFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

    ui->actionFilterExcluding->setText(ui->actionFilterExcluding->text().arg(fieldData));
    ui->actionFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

    // Add table header name
    ui->actionGroupByCol->setText(ui->actionGroupByCol->text().arg(header));
    ui->actionGroupByCol->setEnabled(index.isValid() && columnCanGroup && !controller->isGrouped());

    ui->actionUngroup->setEnabled(index.isValid());

    menu.addSeparator();
    menu.addAction(ui->actionFilterIncluding);
    menu.addAction(ui->actionFilterExcluding);
    menu.addSeparator();

    if(controller->isGrouped())
      menu.addAction(ui->actionUngroup);
    else
      menu.addAction(ui->actionGroupByCol);

    QAction *a = menu.exec(QCursor::pos());
    if(a != nullptr)
    {
      // A menu item was selected
      if(a == ui->actionFilterIncluding)
        controller->filterIncluding(index);
      else if(a == ui->actionFilterExcluding)
        controller->filterExcluding(index);
      else if(a == ui->actionGroupByCol)
      {
        controller->groupByColumn(index);
        ui->actionUngroup->setEnabled(true);
        ui->conditionComboBox->setEnabled(false);
      }
      else if(a == ui->actionUngroup)
      {
        controller->ungroup();
        ui->actionUngroup->setEnabled(false);
        ui->conditionComboBox->setEnabled(true);
      }
      else if(a == ui->actionTableSelectAll)
        controller->selectAll();
      // else if(a == ui->actionTableCopy) this is alread covered by the connected action
    }

    // Restore old menu texts
    ui->actionFilterIncluding->setText(actionFilterIncludingText);
    ui->actionFilterIncluding->setEnabled(true);

    ui->actionFilterExcluding->setText(actionFilterExcludingText);
    ui->actionFilterExcluding->setEnabled(true);

    ui->actionGroupByCol->setText(actionGroupByColText);
    ui->actionGroupByCol->setEnabled(true);
  }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Catch all close events like Ctrl-Q or Menu/Exit or clicking on the
  // close button on the window frame
  qDebug() << "closeEvent";
  int result = dialog->showQuestionMsgBox(ll::constants::SETTINGS_SHOW_QUIT,
                                          tr("Really Quit?"),
                                          tr("Do not &show this dialog again."),
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::Yes);

  if(result != QMessageBox::Yes)
    event->ignore();
  else
  {
    disconnect(ui->dockWidget, &QDockWidget::visibilityChanged,
               ui->actionShowStatistics, &QAction::setChecked);

    disconnect(ui->mainToolBar, &QToolBar::visibilityChanged,
               ui->actionShowToolbar, &QAction::setChecked);
    writeSettings();
    pathSettings.writeSettings();
  }
}

void MainWindow::showEvent(QShowEvent *event)
{
  if(firstStart)
  {
    emit windowShown();
    firstStart = false;
  }

  event->ignore();
}
