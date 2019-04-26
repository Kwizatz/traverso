/*
Copyright (C) 2007 Remon Sijrier 

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

#ifndef NEW_TRACK_DIALOG_H
#define NEW_TRACK_DIALOG_H

#include "ui_NewTrackDialog.h"

#include <QDialog>
#include <QCompleter>
#include <QTimer>

class Project;
class QAbstractButton;
class QCompleter;

class NewTrackDialog : public QDialog, protected Ui::NewTrackDialog
{
	Q_OBJECT

public:
    NewTrackDialog(QWidget* parent = nullptr);
        ~NewTrackDialog() {}

protected:
        void showEvent ( QShowEvent * event );


private:
	Project* m_project{};
        QTimer      m_timer;
        QCompleter  m_completer;
        void update_driver_info();


private slots:
        void close_clicked ();
        void set_project(Project* project);
        void update_buses_comboboxes();
        void reset_information_label();
        void create_track();
        void update_completer(const QString &text);

};

#endif
