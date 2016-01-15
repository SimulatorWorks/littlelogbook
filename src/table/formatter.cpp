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

#include "table/formatter.h"

#include <QDateTime>
#include <QLocale>
#include <QObject>
#include <algorithm>

namespace formatter {

QString formatMinutesHours(double time)
{
  int hours = (int)time;
  int minutes = (int)((time - hours) * 60);
  return QString(QObject::tr("%1:%2")).arg(QLocale().toString(hours)).
         arg(minutes, 2, 10, QChar('0'));
}

QString formatMinutesHoursLong(double time)
{
  int hours = (int)time;
  int minutes = (int)((time - hours) * 60);
  return QString(QObject::tr("%1 h %2 m")).arg(QLocale().toString(hours)).
         arg(minutes, 2, 10, QChar('0'));
}

QString formatMinutesHoursDays(double time)
{
  int days = (int)time / 24;
  int hours = (int)time - (days * 24);
  int minutes = (int)((time - std::floor(time)) * 60.);
  return QString(QObject::tr("%1:%2:%3")).
         arg(QLocale().toString(days)).
         arg(hours, 2, 10, QChar('0')).
         arg(minutes, 2, 10, QChar('0'));
}

QString formatMinutesHoursDaysLong(double time)
{
  int days = (int)time / 24;
  int hours = (int)time - (days * 24);
  int minutes = (int)((time - std::floor(time)) * 60.);
  QString retval;
  if(days > 0)
    retval += QString(QObject::tr("%1 d")).arg(QLocale().toString(days));

  if(hours > 0)
  {
    if(!retval.isEmpty())
      retval += QString(QObject::tr(" %1 h")).arg(hours, 2, 10, QChar('0'));
    else
      retval += QString(QObject::tr("%1 h")).arg(QLocale().toString(hours));
  }

  if(!retval.isEmpty())
    retval += QString(QObject::tr(" %1 m")).arg(minutes, 2, 10, QChar('0'));
  else
    retval += QString(QObject::tr("%1 m")).arg(QLocale().toString(minutes));

  return retval;
}

QString formatDoubleUnit(double value, const QString& unit, int precision)
{
  if(unit.isEmpty())
    return QString(QObject::tr("%L1")).arg(QLocale().toString(value, 'f', precision));
  else
    return QString(QObject::tr("%L1 %2")).arg(QLocale().toString(value, 'f', precision)).arg(unit);
}

QString formatDate(int timeT)
{
  QDateTime dateTime;
  dateTime.setTimeSpec(Qt::UTC);
  dateTime.setTime_t((uint)timeT);
  if(timeT > 0 && dateTime.isValid() && !dateTime.isNull())
    return dateTime.toString(Qt::DefaultLocaleShortDate);
  else
    return QObject::tr("Invalid date");
}

QString formatDateLong(int timeT)
{
  QDateTime dateTime;
  dateTime.setTimeSpec(Qt::UTC);
  dateTime.setTime_t((uint)timeT);
  if(timeT > 0 && dateTime.isValid() && !dateTime.isNull())
    // Workaround to remove the UTC label since FSX stores local time without timezone spec
    return dateTime.toString(Qt::DefaultLocaleLongDate).replace("UTC", "");
  else
    return QObject::tr("Invalid date");
}

} // namespace formatter
