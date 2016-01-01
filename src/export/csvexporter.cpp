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

#include "export/csvexporter.h"

#include "gui/constants.h"

#include "gui/errorhandler.h"
#include "table/sqlmodel.h"
#include "gui/dialog.h"
#include "sql/sqlexport.h"
#include "table/controller.h"

#include "logging/loggingdefs.h"

#include <QFile>
#include <QTextCodec>

using atools::gui::ErrorHandler;
using atools::gui::Dialog;
using atools::sql::SqlQuery;
using atools::sql::SqlExport;

CsvExporter::CsvExporter(QWidget *parent, Controller *controller) :
  Exporter(parent, controller)
{
}

CsvExporter::~CsvExporter()
{
}

QString CsvExporter::saveCsvFileDialog()
{
  return dialog->saveFileDialog(tr("Export CSV Document"),
                                tr("CSV Documents (*.csv);;All Files (*)"),
                                ll::constants::SETTINGS_EXPORT_FILE_DIALOG, "csv");
}

int CsvExporter::exportAll(bool open)
{
  int exported = 0;
  QString filename = saveCsvFileDialog();

  if(!filename.isEmpty())
  {
    qDebug() << "exportAllCsv" << filename;
    QFile file(filename);

    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      QTextStream stream(&file);
      qDebug() << "Used codec" << stream.codec()->name();

      // Run the current query to get all results - not only the visible
      atools::sql::SqlDatabase *db = controller->getSqlDatabase();
      SqlQuery query(db);
      query.exec(controller->getCurrentSqlQuery());

      SqlExport sqlExport;
      sqlExport.setSeparatorChar(';');
      QVariantList values;

      int row = 0;
      while(query.next())
      {
        QSqlRecord rec = query.record();
        if(row == 0)
          stream << sqlExport.getResultSetHeader(headerNames(rec.count()));

        // Write all columns
        values.clear();
        for(int col = 0; col < rec.count(); ++col)
          // Get data formatted as shown in the table
          values.append(controller->formatModelData(rec.fieldName(col), rec.value(col)));
        stream << sqlExport.getResultSetRow(values);
        row++;
        exported++;
      }

      stream.flush();
      file.close();

      if(open)
        openDocument(filename);
    }
    else
      errorHandler->handleIOError(file);
  }
  return exported;
}

int CsvExporter::exportSelected(bool open)
{
  int exported = 0;
  QString filename = saveCsvFileDialog();

  if(!filename.isEmpty())
  {
    qDebug() << "exportSelectedCsv" << filename;

    QFile file(filename);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      // Get the view selection
      const QItemSelection sel = controller->getSelection();
      QTextStream stream(&file);
      qDebug() << "Used codec" << stream.codec()->name();

      SqlExport sqlExport;
      sqlExport.setSeparatorChar(';');

      QVector<const Column *> columnList = controller->getCurrentColumns();

      stream << sqlExport.getResultSetHeader(headerNames(columnList.size()));

      for(QItemSelectionRange rng : sel)
        for(int row = rng.top(); row <= rng.bottom(); ++row)
        {
          // Export as formatted in the view
          stream << sqlExport.getResultSetRow(controller->getFormattedModelData(row));
          exported++;
        }

      stream.flush();
      file.close();

      if(open)
        openDocument(filename);
    }
    else
      errorHandler->handleIOError(file);
  }
  return exported;
}
