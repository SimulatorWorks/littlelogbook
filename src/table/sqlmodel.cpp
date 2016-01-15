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

#include "table/sqlmodel.h"
#include "gui/constants.h"
#include "gui/airportinfo.h"

#include "gui/errorhandler.h"
#include "fs/lb/types.h"
#include "fs/fspaths.h"
#include "table/columnlist.h"
#include "table/formatter.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlutil.h"
#include "geo/calculations.h"
#include "logging/loggingdefs.h"

#include <algorithm>
#include <QApplication>
#include <QLineEdit>
#include <QSqlField>
#include <QPalette>

using atools::sql::SqlQuery;
using atools::sql::SqlUtil;
using atools::sql::SqlDatabase;
using atools::gui::ErrorHandler;

SqlModel::SqlModel(QWidget *parent, SqlDatabase *sqlDb, const ColumnList *columnList, bool hasAirportTable)
  : QSqlQueryModel(parent), db(sqlDb), columns(columnList), parentWidget(parent), hasAirports(hasAirportTable)
{
  tableName = "logbook";

  /* Alternating colors */
  rowBgColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
  rowAltBgColor = QApplication::palette().color(QPalette::Active, QPalette::AlternateBase);

  /* Slightly darker background for sort column */
  rowSortBgColor = rowBgColor.darker(106);
  rowSortAltBgColor = rowAltBgColor.darker(106);

  airportInfo = new AirportInfo(parent, db);

  buildQuery();
}

SqlModel::~SqlModel()
{
  delete airportInfo;
}

void SqlModel::filter(const QString& colName, const QVariant& value)
{
  bool colAlreadyFiltered = whereConditionMap.contains(colName);

  if(value.isNull() || (value.type() == QVariant::String && value.toString().isEmpty()))
  {
    // If we get a null value or an empty string and the
    // column is already filtered remove it
    if(colAlreadyFiltered)
      whereConditionMap.remove(colName);
  }
  else
  {
    QVariant newVariant;
    QString condition;

    if(value.type() == QVariant::String)
    {
      // Use like queries for string
      QString newVal = value.toString();

      if(newVal.startsWith(ll::constants::QUERY_NEGATE_CHAR))
      {
        if(newVal == ll::constants::QUERY_NEGATE_CHAR)
        {
          // A single "-" translates to not nulls
          condition = "is not null";
          newVal.clear();
        }
        else
        {
          condition = "not like";
          newVal.remove(0, 1);
        }
      }
      else
        condition = "like";

      // Replace "*" with "%" for SQL
      if(newVal.contains(ll::constants::QUERY_PLACEHOLDER_CHAR))
        newVal = newVal.toUpper().replace(ll::constants::QUERY_PLACEHOLDER_CHAR, "%");
      else if(!newVal.isEmpty())
        newVal = newVal.toUpper() + "%";

      newVariant = newVal;
    }
    else if(value.type() == QVariant::Int)
    {
      // Use equal for numbers
      newVariant = value;
      condition = "=";
    }
    const Column *col = columns->getColumn(colName);
    Q_ASSERT(col != nullptr);

    if(colAlreadyFiltered)
    {
      // Replace values in existing condition
      whereConditionMap[colName].oper = condition;
      whereConditionMap[colName].value = newVariant;
      whereConditionMap[colName].col = col;
    }
    else
      whereConditionMap.insert(colName, {colName, condition, newVariant, col});
  }
  buildQuery();
}

void SqlModel::filterOperator(const QString& op)
{
  whereOperator = op;
  buildQuery();
}

QVariant SqlModel::getFormattedFieldData(const QModelIndex& index) const
{
  return data(index);
}

void SqlModel::filterIncluding(QModelIndex index)
{
  filterBy(index, false);
  buildQuery();
}

void SqlModel::filterExcluding(QModelIndex index)
{
  filterBy(index, true);
  buildQuery();
}

void SqlModel::filterBy(QModelIndex index, bool exclude)
{
  QString whereCol = record().field(index.column()).name();
  QVariant whereValue = QSqlQueryModel::data(index);

  // If there is already a filter on the same column remove it
  if(whereConditionMap.contains(whereCol))
    whereConditionMap.remove(whereCol);

  QString whereOp;
  if(whereValue.isNull())
    whereOp = exclude ? "is not null" : "is null";
  else
    whereOp = exclude ? " not like " : " like ";

  const Column *col = columns->getColumn(whereCol);
  Q_ASSERT(col != nullptr);

  // Set the search text into the corresponding line edit
  QLineEdit *edit = columns->getColumn(whereCol)->getLineEditWidget();
  if(edit != nullptr)
    edit->setText((exclude ? ll::constants::QUERY_NEGATE_CHAR : "") + whereValue.toString());

  whereConditionMap.insert(whereCol, {whereCol, whereOp, whereValue, col});
}

void SqlModel::getGroupByColumn(QModelIndex index)
{
  groupByCol = record().fieldName(index.column());
  orderByCol = groupByCol;
  orderByOrder = "asc";
  clearWhereConditions();
  buildQuery();
  fillHeaderData();
}

void SqlModel::reset()
{
  orderByCol.clear();
  orderByOrder.clear();
  groupByCol.clear();
  clearWhereConditions();
  buildQuery();
  fillHeaderData();
}

void SqlModel::resetSearch()
{
  clearWhereConditions();
  buildQuery();
  // no need to rebuild header - view remains the same
}

void SqlModel::clearWhereConditions()
{
  // Keep simulator filter - system dependent
  auto iter = whereConditionMap.find("simulator_id");
  if(iter != whereConditionMap.end())
  {
    WhereCondition wc = *iter;
    whereConditionMap.clear();
    whereConditionMap.insert(wc.column, wc);
  }
  else
    whereConditionMap.clear();
}

void SqlModel::ungroup()
{
  clearWhereConditions();
  groupByCol.clear();

  // Restore last sort order
  orderByCol = lastOrderByCol;
  orderByOrder = sortOrderToSql(lastOrderByOrder);

  buildQuery();
  fillHeaderData();
}

void SqlModel::fillHeaderData()
{
  int cnt = record().count();
  for(int i = 0; i < cnt; ++i)
  {
    QString field = record().fieldName(i);
    const Column *cd = columns->getColumn(field);

    Q_ASSERT_X(cd != nullptr, "fillHeaderData", QString("field \"" + field + "\" is null").toLocal8Bit());

    if(!cd->isHiddenCol())
      setHeaderData(i, Qt::Horizontal, cd->getDisplayName());
  }
}

bool SqlModel::isGrouped() const
{
  return !groupByCol.isEmpty();
}

int SqlModel::getLastSortIndex() const
{
  return record().indexOf(lastOrderByCol);
}

Qt::SortOrder SqlModel::getLastSortOrder() const
{
  return lastOrderByOrder;
}

const Column *SqlModel::getColumnModel(const QString& colName) const
{
  return columns->getColumn(colName);
}

const Column *SqlModel::getColumnModel(int colIndex) const
{
  return columns->getColumn(record().fieldName(colIndex));
}

QString SqlModel::sortOrderToSql(Qt::SortOrder order)
{
  switch(order)
  {
    case Qt::AscendingOrder:
      return "asc";

    case Qt::DescendingOrder:
      return "desc";
  }
  return QString();
}

void SqlModel::sort(int column, Qt::SortOrder order)
{
  orderByColIndex = column;
  orderByCol = record().field(column).name();
  orderByOrder = sortOrderToSql(order);

  if(groupByCol.isEmpty())
  {
    // Remember this sort order for the next ungroup
    lastOrderByCol = orderByCol;
    lastOrderByOrder = order;
  }
  buildQuery();
}

QString SqlModel::buildColumnList()
{
  QVector<QString> colNames;
  for(const Column& col : columns->getColumns())
  {
    if(groupByCol.isEmpty())
    {
      // Not grouping - default view
      if(col.isDefaultCol() || col.isHiddenCol())
        colNames.append(col.getColumnName());
    }
    else if(col.getColumnName() == groupByCol || col.isGroupShow())
      // Add the group by column
      colNames.append(col.getColumnName());
    else
    {
      // Add all aggregate columns
      QString cname = col.getColumnName();
      if(col.isMin())
        colNames.append("min(" + cname + ") as " + cname + "_min");
      if(col.isMax())
        colNames.append("max(" + cname + ") as " + cname + "_max");
      if(col.isSum())
        colNames.append("sum(" + cname + ") as " + cname + "_sum");
    }
  }

  if(!groupByCol.isEmpty())
    // Always add total count when grouping
    colNames.append("count(*) as num_flights");

  // Concatenate to one string
  QString queryCols;
  bool first = true;
  for(QString cname : colNames)
  {
    if(!first)
      queryCols += ", ";
    queryCols += cname;

    first = false;
  }
  return queryCols;
}

QString SqlModel::buildWhereValue(const WhereCondition& cond)
{
  QString val;
  if(cond.value.type() == QVariant::String || cond.value.type() == QVariant::Char)
    val = " '" + cond.value.toString() + "'";
  else if(cond.value.type() == QVariant::Bool ||
          cond.value.type() == QVariant::Int ||
          cond.value.type() == QVariant::UInt ||
          cond.value.type() == QVariant::LongLong ||
          cond.value.type() == QVariant::ULongLong ||
          cond.value.type() == QVariant::Double)
    val = " " + cond.value.toString();
  return val;
}

QString SqlModel::buildWhere()
{
  QString queryWhere;
  QString queryWhereAnd;

  int numCond = 0, numAndCond = 0;
  for(const WhereCondition& cond : whereConditionMap)
  {
    if(!cond.col->isAlwaysAndCol())
    {
      if(numCond++ > 0)
        queryWhere += " " + whereOperator + " ";
      queryWhere += cond.column + " " + cond.oper + " ";
      if(!cond.value.isNull())
        queryWhere += buildWhereValue(cond);
    }
    else
    {
      if(numAndCond++ > 0)
        queryWhereAnd += " and ";
      queryWhereAnd += cond.column + " " + cond.oper + " ";
      if(!cond.value.isNull())
        queryWhereAnd += buildWhereValue(cond);
    }
  }
  if(numCond > 0)
    queryWhere = "(" + queryWhere + ")";

  if(numAndCond > 0)
  {
    if(numCond > 0)
      queryWhere += " and ";
    queryWhere += queryWhereAnd;
  }

  if(numCond > 0 || numAndCond > 0)
    queryWhere = " where " + queryWhere;

  return queryWhere;
}

void SqlModel::buildQuery()
{
  QString queryCols = buildColumnList();

  QString queryWhere = buildWhere();

  QString queryGroup;
  if(!groupByCol.isEmpty())
    queryGroup += "group by " + groupByCol;

  QString queryOrder;
  if(!orderByCol.isEmpty() && !orderByOrder.isEmpty())
  {
    const Column *col = columns->getColumn(orderByCol);
    Q_ASSERT(col != nullptr);

    if(!(col->getSortFuncColAsc().isEmpty() && col->getSortFuncColDesc().isEmpty()))
    {
      // Use sort functions to have null values at end of the list - will avoid indexes
      if(orderByOrder == "asc")
        queryOrder += "order by " + col->getSortFuncColAsc().arg(orderByCol) + " " + orderByOrder;
      else if(orderByOrder == "desc")
        queryOrder += "order by " + col->getSortFuncColDesc().arg(orderByCol) + " " + orderByOrder;
      else
        Q_ASSERT(orderByOrder != "asc" && orderByOrder != "desc");
    }
    else
      queryOrder += "order by " + orderByCol + " " + orderByOrder;
  }

  currentSqlQuery = "select " + queryCols + " from " + tableName +
                    " " + queryWhere + " " + queryGroup + " " + queryOrder;

  // Build a query to find the total row count of the result
  totalRowCount = 0;
  QString queryCount;
  if(isGrouped())
    queryCount = "select count(1) from "
                 "(select count(" + groupByCol + ") from " + tableName + " " + queryWhere + " " +
                 queryGroup + ")";
  else
    queryCount = "select count(1) from " + tableName + " " + queryWhere;

  try
  {
    SqlQuery countStmt(db);
    countStmt.exec(queryCount);
    if(countStmt.next())
      totalRowCount = countStmt.value(0).toInt();

    qDebug() << "Query" << currentSqlQuery;
    qDebug() << "Query Count" << queryCount;

    QSqlQueryModel::setQuery(currentSqlQuery, db->getQSqlDatabase());

    if(lastError().isValid())
      atools::gui::ErrorHandler(parentWidget).handleSqlError(lastError());
  }
  catch(std::exception& e)
  {
    atools::gui::ErrorHandler(parentWidget).handleException(e, "While executing query");
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parentWidget).handleUnknownException("While executing query");
  }
}

QString SqlModel::formatValue(const QString& colName, const QVariant& value) const
{
  using namespace atools::fs::lb::types;
  using namespace atools::fs;

  if(colName.startsWith("simulator_id"))
  {
  SimulatorType type = static_cast<SimulatorType>(value.toInt());
  switch(type)
  {
    case atools::fs::FSX:
      return tr("FSX");

    case atools::fs::FSX_SE:
      return tr("FSX SE");

    case atools::fs::P3D_V2:
      return tr("P3D V2");

    case atools::fs::P3D_V3:
      return tr("P3D V3");

    case atools::fs::ALL_SIMULATORS:
      return QString();

  }
  }
  else if(colName.startsWith("startdate"))
    return formatter::formatDate(value.toInt());
  else if(colName.startsWith("total_time"))
    return formatter::formatMinutesHours(value.toDouble());
  else if(colName.startsWith("night_time"))
    return formatter::formatMinutesHours(value.toDouble());
  else if(colName.startsWith("instrument_time"))
    return formatter::formatMinutesHours(value.toDouble());
  else if(colName.startsWith("distance"))
    return formatter::formatDoubleUnit(value.toDouble());
  else if(colName == "aircraft_type")
  {
    AircraftType type = static_cast<AircraftType>(value.toInt());
    switch(type)
    {
      case AIRCRAFT_UNKNOWN:
        return tr("Unknown");

      case AIRCRAFT_GLIDER:
        return tr("Glider");

      case AIRCRAFT_FIXED_WING:
        return tr("Fixed Wing");

      case AIRCRAFT_AMPHIBIOUS:
        return tr("Amphibious");

      case AIRCRAFT_ROTARY:
        return tr("Rotor");
    }
  }
  else if(colName == "aircraft_flags")
  {
    if((value.toInt() & AIRCRAFT_FLAG_MULTIMOTOR) == AIRCRAFT_FLAG_MULTIMOTOR)
      return tr("Multi-engine");
    else
      return QString();
  }
  else if(value.type() == QVariant::Int || value.type() == QVariant::UInt)
    return QLocale().toString(value.toInt());
  else if(value.type() == QVariant::LongLong || value.type() == QVariant::ULongLong)
    return QLocale().toString(value.toLongLong());
  else if(value.type() == QVariant::Double)
    return QLocale().toString(value.toDouble());

  return value.toString();
}

Qt::SortOrder SqlModel::getSortOrder() const
{
  return orderByOrder == "desc" ? Qt::DescendingOrder : Qt::AscendingOrder;
}

QVariant SqlModel::data(const QModelIndex& index, int role) const
{
  if(!index.isValid())
    return QVariant();

  if(role == Qt::DisplayRole)
  {
    QVariant var = QSqlQueryModel::data(index, role);
    QString col = record().field(index.column()).name();
    return formatValue(col, var);
  }
  else if(role == Qt::ToolTipRole)
  {
    QString col = record().field(index.column()).name();

    if((col == "airport_from_icao" || col == "airport_to_icao") && hasAirports)
      return airportInfo->createAirportHtml(QSqlQueryModel::data(index, Qt::DisplayRole).toString());
    else if(col.startsWith("startdate"))
      return formatter::formatDateLong(QSqlQueryModel::data(index).toInt());
    else if(col.startsWith("distance"))
    {
      double nm = QSqlQueryModel::data(index).toDouble();
      if(nm > 0.01)
        return formatter::formatDoubleUnit(atools::geo::nmToMeters(nm / 1000.), tr("kilometers"));
    }
  }
  else if(role == Qt::BackgroundRole)
  {
    if(index.column() == orderByColIndex)
      return (index.row() % 2) == 0 ? rowSortBgColor : rowSortAltBgColor;
    else
      return (index.row() % 2) == 0 ? rowBgColor : rowAltBgColor;
  }
  else if(role == Qt::TextAlignmentRole)
  {
    QString col = record().field(index.column()).name();

    if(col.startsWith("logbook_id"))
      return Qt::AlignRight;
    else if(col.startsWith("startdate"))
      return Qt::AlignRight;
    else if(col.startsWith("total_time"))
      return Qt::AlignRight;
    else if(col.startsWith("night_time"))
      return Qt::AlignRight;
    else if(col.startsWith("instrument_time"))
      return Qt::AlignRight;
    else if(col.startsWith("distance"))
      // TODO why is this still left aligned
      return Qt::AlignRight;
    else
      return QSqlQueryModel::data(index, role);
  }

  return QVariant();
}

void SqlModel::fetchMore(const QModelIndex& parent)
{
  QSqlQueryModel::fetchMore(parent);
  emit fetchedMore();
}

QVariantList SqlModel::getRawData(int row) const
{
  QVariantList values;
  for(int i = 0; i < columnCount(); ++i)
    values.append(QSqlQueryModel::data(createIndex(row, i)));
  return values;
}

QStringList SqlModel::getRawColumns() const
{
  QStringList cols;
  for(int i = 0; i < columnCount(); ++i)
    cols.append(record().field(i).name());
  return cols;
}

QVariantList SqlModel::getFormattedRowData(int row)
{
  QVariantList values;
  for(int i = 0; i < columnCount(); ++i)
  {
    QModelIndex idx = createIndex(row, i);
    values.append(formatValue(record().field(idx.column()).name(), QSqlQueryModel::data(idx)));
  }
  return values;
}
