/*****************************************************************************
* Copyright 2015 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLELOGBOOK_COLUMNLIST_H
#define LITTLELOGBOOK_COLUMNLIST_H

#include "table/colum.h"

#include <QHash>
#include <QObject>
#include <QVector>
#include <QStringList>

class QLineEdit;
class QComboBox;

/*
 * A list of column descriptor that define behaviour and display in the table
 * view.
 */
class ColumnList :
  public QObject
{
  Q_OBJECT

public:
  /*
   * @param hasAirports true if the airports table (from runways.xml file)
   * exists.
   */
  ColumnList(bool hasAirports);
  virtual ~ColumnList();

  /* Get column descriptor for the given query column name or alias */
  const Column *getColumn(const QString& field) const;

  /* Get all column descriptors */
  const QVector<Column>& getColumns() const
  {
    return columns;
  }

  /* Assign a QLineEdit widget to the column descriptor with the given name */
  void assignLineEdit(const QString& field, QLineEdit *edit);

  /* Assign a QComboBox widget to the column descriptor with the given name */
  void assignComboBox(const QString& field, QComboBox *combo);

  /* Clear all LineEdit widgets and ComboBox widgets */
  void clearWidgets();

  /* Enable or disable widgets except the ones with the give column names */
  void enableWidgets(bool enabled = true, const QStringList& exceptColNames = QStringList());

private:
  QVector<Column> columns;
  QHash<QString, Column *> nameColumnMap;
};

#endif // LITTLELOGBOOK_COLUMNLIST_H
