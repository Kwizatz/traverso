/*
Copyright (C) 2006 Remon Sijrier 

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

$Id: Config.h,v 1.5 2007/01/11 12:03:25 r_sijrier Exp $
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QHash>

class Config : public QObject
{
	Q_OBJECT
public:
	int get_int_property(const QString& property, int defaultValue=0) const;
	
	int get_hardware_int_property(const QString& property, int defaultValue=0) const;
	int get_ie_int_property(const QString& property, int defaultValue=0) const;
	int get_project_int_property(const QString& property, int defaultValue=0) const;

	float get_float_property(const QString& type, const QString& property, float defaultValue=0.0) const;
	
	QString get_project_string_property(const QString& property, const QString& defaultValue="") const;
	QString get_hardware_string_property(const QString& property, const QString& defaultValue="") const;
	
	void set_property(const QString& type, const QString& property, float newValue);

	void set_project_property(const QString& property, int newValue);
	void set_project_property(const QString& property, const QString& newValue);
	
	void set_hardware_property(const QString& property, int newValue);
	void set_hardware_property(const QString& property, const QString& newValue);
	
	void check_and_load_configuration();
	void reset_settings( );
	
	void init_input_engine();
	
	void save();

private:
	Config() {}

	Config(const Config&);


	// allow this function to create one instance
	friend Config& config();
	
	void load_configuration();
	
	QHash<QString, int>	m_intConfigs;
	QHash<QString, float>	m_floatConfigs;
	QHash<QString, QString>	m_stringConfigs;

};


// use this function to access the settings
Config& config();


#endif
