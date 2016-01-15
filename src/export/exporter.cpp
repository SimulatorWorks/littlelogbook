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

#include "export/exporter.h"
#include "table/controller.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "logging/loggingdefs.h"

#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QSqlField>
#include <QSqlRecord>

using atools::gui::Dialog;
using atools::gui::ErrorHandler;

Exporter::Exporter(QWidget *parent, Controller *controllerObj)
  : parentWidget(parent), controller(controllerObj)
{
  dialog = new Dialog(parent);
  errorHandler = new ErrorHandler(parent);
}

Exporter::~Exporter()
{
  delete dialog;
  delete errorHandler;
}

void Exporter::createVisualColumnIndex(int cnt, QVector<int>& visualToIndex)
{
  visualToIndex.clear();
  visualToIndex.fill(-1, cnt);

  for(int i = 0; i < cnt; ++i)
  {
    int vindex = controller->getColumnVisualIndex(i);

    Q_ASSERT(vindex >= 0);

    if(controller->isColumnVisibleInView(i))
      visualToIndex[vindex] = i;
  }
}

QStringList Exporter::headerNames(int cnt, const QVector<int>& visualToIndex)
{
  QStringList columnNames;

  for(int i = 0; i < cnt; ++i)
    if(visualToIndex[i] != -1)
      columnNames.append(controller->getColumn(visualToIndex[i])->getDisplayName().
                         replace("-\n", "").replace("\n", " "));
  return columnNames;
}

QStringList Exporter::headerNames(int cnt)
{
  QStringList columnNames;

  for(int i = 0; i < cnt; ++i)
    columnNames.append(controller->getColumn(i)->getDisplayName().
                       replace("-\n", "").replace("\n", " "));
  return columnNames;
}

void Exporter::openDocument(const QString& file)
{
  QUrl url(QUrl::fromLocalFile(file));

  qDebug() << "HtmlExporter opening" << url;
  if(!QDesktopServices::openUrl(url))
  {
    qWarning() << "openUrl failed for" << url;
    QMessageBox::warning(parentWidget, QApplication::applicationName(),
                         QString(tr("Cannot open file <i>%1</i>")).arg(file),
                         QMessageBox::Close, QMessageBox::NoButton);
  }
}

void Exporter::fillRecord(const QVariantList& values, const QStringList& cols, QSqlRecord& rec)
{
  Q_ASSERT(values.size() == cols.size());

  if(rec.isEmpty())
    for(int i = 0; i < values.size(); i++)
      rec.append(QSqlField(cols.at(i), values.at(i).type()));

  for(int i = 0; i < values.size(); i++)
    rec.setValue(i, values.at(i));
}
