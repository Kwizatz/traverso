/*
    Copyright (C) 2009 Nicola Döbelin
 
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

#ifndef AUDIO_IO_DIALOG_H
#define AUDIO_IO_DIALOG_H

#include "ui_AudioIODialog.h"

#include <QDialog>
#include "defines.h"

class AudioIODialog : public QDialog, protected Ui::AudioIODialog
{
	Q_OBJECT

public:
	AudioIODialog(QWidget* parent = 0);
	~AudioIODialog() {};


private:
	QStringList m_inputChannelList;
	QStringList m_outputChannelList;

	void initInput();
	void initOutput();
	void accept();
        QList<bus_config> outputBusConfig();
        QList<bus_config> inputBusConfig();
	
private slots:
	void addMonoInput();
	void addStereoInput();
	void removeInput();
        void inputSelectionChanged(QTreeWidgetItem *, QTreeWidgetItem *);

	void addMonoOutput();
	void addStereoOutput();
	void removeOutput();
        void outputSelectionChanged(QTreeWidgetItem *, QTreeWidgetItem *);

        void addJackInput();
        void addJackOutput();
        void removeJackInput();
        void removeJackOutput();

        void itemChanged(QTreeWidgetItem *, int);
        void itemDoubleClicked(QTreeWidgetItem *, int);
};


#endif


