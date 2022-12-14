/*
    Copyright (C) 2005-2007 Remon Sijrier

    This file is part of Traverso

    Traverso is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.

*/

#include "Utils.h"
#include "Mixer.h"

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QPixmapCache>
#include <QRegularExpression>
#include <QLocale>
#include <QChar>
#include <QTranslator>
#include <QDir>
#include <cmath>

TimeRef msms_to_timeref(QString str)
{
	TimeRef out;
    QStringList lst = str.simplified().split(QRegularExpression("[;,.:]"), Qt::SkipEmptyParts);

	if (lst.size() >= 1) out += TimeRef(lst.at(0).toInt() * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 2) out += TimeRef(lst.at(1).toInt() * UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 3) out += TimeRef(lst.at(2).toInt() * UNIVERSAL_SAMPLE_RATE / 1000);

	return out;
}

TimeRef cd_to_timeref(QString str)
{
	TimeRef out;
    QStringList lst = str.simplified().split(QRegularExpression("[;,.:]"), Qt::SkipEmptyParts);

	if (lst.size() >= 1) out += TimeRef(lst.at(0).toInt() * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 2) out += TimeRef(lst.at(1).toInt() * UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 3) out += TimeRef(lst.at(2).toInt() * UNIVERSAL_SAMPLE_RATE / 75);

	return out;
}

TimeRef cd_to_timeref_including_hours(QString str)
{
	TimeRef out;
    QStringList lst = str.simplified().split( QRegularExpression("[;,.:]"), Qt::SkipEmptyParts);

	if (lst.size() >= 1) out += TimeRef(lst.at(0).toInt() * ONE_HOUR_UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 2) out += TimeRef(lst.at(1).toInt() * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 3) out += TimeRef(lst.at(2).toInt() * UNIVERSAL_SAMPLE_RATE);
	if (lst.size() >= 4) out += TimeRef(lst.at(3).toInt() * UNIVERSAL_SAMPLE_RATE / 75);

	return out;
}

QString coefficient_to_dbstring ( float coeff, int decimals)
{
	float db = coefficient_to_dB ( coeff );

	QString gainIndB;

	if (std::fabs(db) < (1/::pow(10, decimals))) {
		db = 0.0f;
	}

	if ( db < -99 )
		gainIndB = "- INF";
	else if ( db < 0 )
		gainIndB = "- " + QByteArray::number ( ( -1 * db ), 'f', decimals ) + " dB";
	else if ( db > 0 )
		gainIndB = "+ " + QByteArray::number ( db, 'f', decimals ) + " dB";
	else {
		gainIndB = "  " + QByteArray::number ( db, 'f', decimals ) + " dB";
	}

	return gainIndB;
}

qint64 create_id( )
{
	int r = rand();
	QDateTime time = QDateTime::currentDateTime();
    uint timeValue = time.toSecsSinceEpoch();
	qint64 id = timeValue;
	id *= 1000000000;
	id += r;

	return id;
}

QDateTime extract_date_time(qint64 id)
{
	QDateTime time;
    //QT6_FIXME
//	time.setTime_t(id / 1000000000);
	return time;
}

QPixmap find_pixmap ( const QString & pixname )
{
	QPixmap pixmap;

    if ( ! QPixmapCache::find( pixname, &pixmap ) )
	{
		pixmap = QPixmap ( pixname );
		QPixmapCache::insert ( pixname, pixmap );
	}

	return pixmap;
}

QString timeref_to_hms(const TimeRef& ref)
{
	qint64 remainder;
	int hours, mins, secs;

	qint64 universalframe = ref.universal_frame();

	hours = (int) (universalframe / ONE_HOUR_UNIVERSAL_SAMPLE_RATE);
	remainder = qint64(universalframe - (hours * ONE_HOUR_UNIVERSAL_SAMPLE_RATE));
	mins = (int) (remainder / ( ONE_MINUTE_UNIVERSAL_SAMPLE_RATE ));
	remainder -= mins * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE;
	secs = (int) (remainder / UNIVERSAL_SAMPLE_RATE);
    QString spos("%1:%2%3");
    return spos.arg(hours, 2, 10, QLatin1Char('0')).arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));

}

QString timeref_to_ms(const TimeRef& ref)
{
	qint64 remainder;
	int mins, secs;

	qint64 universalframe = ref.universal_frame();

	mins = (int) (universalframe / ( ONE_MINUTE_UNIVERSAL_SAMPLE_RATE ));
	remainder = (long unsigned int) (universalframe - (mins * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE));
	secs = (int) (remainder / UNIVERSAL_SAMPLE_RATE);
    QString spos("%1:%2");
    return spos.arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));
}

// TimeRef to MM:SS.99 (hundredths)
QString timeref_to_ms_2 (const TimeRef& ref)
{
	qint64 remainder;
	int mins, secs, frames;

	qint64 universalframe = ref.universal_frame();

	mins = universalframe / ( ONE_MINUTE_UNIVERSAL_SAMPLE_RATE );
	remainder = universalframe - ( mins * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE );
	secs = remainder / UNIVERSAL_SAMPLE_RATE;
	remainder -= secs * UNIVERSAL_SAMPLE_RATE;
	frames = remainder * 100 / UNIVERSAL_SAMPLE_RATE;
    QString spos("%1:%2%3%4");
    return spos.arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')).arg(QLocale::system().decimalPoint()).arg(frames, 2, 10, QLatin1Char('0'));
}

// TimeRef to MM:SS.999 (ms)
QString timeref_to_ms_3(const TimeRef& ref)
{
	qint64 remainder;
	int mins, secs, frames;

	qint64 universalframe = ref.universal_frame();

	mins = universalframe / ( ONE_MINUTE_UNIVERSAL_SAMPLE_RATE );
	remainder = universalframe - ( mins * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE );
	secs = remainder / UNIVERSAL_SAMPLE_RATE;
	remainder -= secs * UNIVERSAL_SAMPLE_RATE;
	frames = remainder * 1000 / UNIVERSAL_SAMPLE_RATE;
    QString spos("%1:%2%3%4");
    return spos.arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')).arg(QLocale::system().decimalPoint()).arg(frames, 3, 10, QLatin1Char('0'));
}

// Frame to MM:SS:75 (75ths of a second, for CD burning)
QString timeref_to_cd (const TimeRef& ref)
{
	qint64 remainder;
	int mins, secs, frames;

	qint64 universalframe = ref.universal_frame();

	mins = universalframe / ( ONE_MINUTE_UNIVERSAL_SAMPLE_RATE );
	remainder = universalframe - ( mins * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE );
	secs = remainder / UNIVERSAL_SAMPLE_RATE;
	remainder -= secs * UNIVERSAL_SAMPLE_RATE;
	frames = remainder * 75 / UNIVERSAL_SAMPLE_RATE;
    QString spos("%1:%2%3");
    return spos.arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')).arg(frames, 2, 10, QLatin1Char('0'));
}

// Frame to HH:MM:SS,75 (75ths of a second, for CD burning)
QString timeref_to_cd_including_hours (const TimeRef& ref)
{
	qint64 remainder;
	int hours, mins, secs, frames;

	qint64 universalframe = ref.universal_frame();

	hours = int(universalframe / ONE_HOUR_UNIVERSAL_SAMPLE_RATE);
	remainder = qint64(universalframe - (hours * ONE_HOUR_UNIVERSAL_SAMPLE_RATE));
	mins = (int) (remainder / ( ONE_MINUTE_UNIVERSAL_SAMPLE_RATE ));
	remainder -= mins * ONE_MINUTE_UNIVERSAL_SAMPLE_RATE;
	secs = (int) (remainder / UNIVERSAL_SAMPLE_RATE);
	remainder -= secs * UNIVERSAL_SAMPLE_RATE;
	frames = remainder * 75 / UNIVERSAL_SAMPLE_RATE;

    QString spos("%1:%2%3%4");
    return spos.arg(hours, 2, 10, QLatin1Char('0')).arg(mins, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0')).arg(frames, 2, 10, QLatin1Char('0'));
}

QString timeref_to_text(const TimeRef & ref, qint64 scalefactor)
{
	if (scalefactor >= 512*640) {
		return timeref_to_ms_2(ref);
	} else {
		return timeref_to_ms_3(ref);
	}
}


QStringList find_qm_files()
{
	QDir dir(":/translations");
	QStringList fileNames = dir.entryList(QStringList("*.qm"), QDir::Files, QDir::Name);
	QMutableStringListIterator i(fileNames);
	while (i.hasNext()) {
		i.next();
		i.setValue(dir.filePath(i.value()));
	}
	return fileNames;
}

QString language_name_from_qm_file(const QString& lang)
{
	QTranslator translator;
	translator.load(lang);
	return translator.translate("LanguageName", "English", "The name of this Language, e.g. German would be Deutch");
}

bool t_MetaobjectInheritsClass(const QMetaObject *mo, const QString& className)
{
	while (mo) {
		if (mo->className() == className) {
			return true;
		}
		mo = mo->superClass();
	}
	return false;
}

bool t_KeyStringToKeyValue(int &variable, const QString &text)
{
	if (text == "NUMERICAL")
	{
		return true;
	}

	variable = 0;
	QString s;
	int x  = 0;
    if ((text != "") && (text.length() > 0) ) {
		s="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		x = s.indexOf(text);
		if (x>=0) {
			variable = Qt::Key_A + x;
		} else {
			s="|ESC     |TAB     |BACKTAB |BKSPACE |RETURN  |ENTER   |INSERT  |DELETE  "
			"|PAUSE   |PRINT   |SYSREQ  |CLEAR   ";
			x = s.indexOf("|" + text);
			if (x>=0)
				variable = Qt::Key_Escape + (x/9);
			else {
				s="|HOME    |END     |LEFTARROW  |UPARROW  |RIGHTARROW  "
				"|DOWNARROW  |PRIOR   |NEXT    ";
				x = s.indexOf("|" + text);
				if (x>=0)
					variable = Qt::Key_Home + (x/9);
				else {
					s="|SHIFT   |CTRL    |META    |ALT     |CAPS    "
					"|NUMLOCK |SCROLL  ";
					x = s.indexOf("|" + text);
					if (x>=0)
						variable = Qt::Key_Shift + (x/9);
					else {
						s="F1 F2 F3 F4 F5 F6 F7 F8 F9 F10F11F12";
						x=s.indexOf(text);
						if (x>=0) {
							variable = Qt::Key_F1 + (x/3);
						} else if (text=="SPACE") {
							variable = Qt::Key_Space;
						} else if (text == "MOUSEBUTTONLEFT") {
							variable = Qt::LeftButton;
						} else if (text == "MOUSEBUTTONRIGHT") {
							variable = Qt::RightButton;
						} else if (text == "MOUSEBUTTONMIDDLE") {
                            variable = Qt::MiddleButton;
						} else if (text == "MOUSEBUTTONX1") {
							variable = Qt::XButton1;
						} else if (text == "MOUSEBUTTONX2") {
							variable = Qt::XButton2;
						} else if (text == "MOUSESCROLLHORIZONTALLEFT") {
							variable = MouseScrollHorizontalLeft;
						} else if (text =="MOUSESCROLLHORIZONTALRIGHT") {
							variable = MouseScrollHorizontalRight;
						} else if (text == "MOUSESCROLLVERTICALUP") {
							variable = MouseScrollVerticalUp;
						} else if( text == "MOUSESCROLLVERTICALDOWN") {
							variable = MouseScrollVerticalDown;
						} else if( text == "/") {
							variable = Qt::Key_Slash;
						} else if ( text == "\\") {
							variable = Qt::Key_Backslash;
						} else if ( text == "[") {
							variable = Qt::Key_BracketLeft;
						} else if ( text == "]") {
							variable = Qt::Key_BracketRight;
						} else if ( text == "PAGEUP") {
							variable = Qt::Key_PageUp;
						} else if ( text == "PAGEDOWN") {
							variable = Qt::Key_PageDown;
						} else if (text == "MINUS") {
							variable = Qt::Key_Minus;
						} else if (text == "PLUS") {
							variable = Qt::Key_Plus;
						} else if (text == ";") {
							variable = Qt::Key_Semicolon;
						} else if (text == "'") {
							variable = Qt::Key_Apostrophe;
						} else if (text == ",") {
							variable = Qt::Key_Comma;
						} else if (text == ".") {
							variable = Qt::Key_Period;
						} else {
							printf("KeyStringToValue: No value found for key %s\n", QS_C(text));
							return false;
						}
					}
				}
			}
		}



	}

	// Code found, return true
	return true;
}
