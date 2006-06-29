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

$Id: Tsar.h,v 1.2 2006/06/29 22:43:24 r_sijrier Exp $
*/

#ifndef TSAR_H
#define TSAR_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QStack>

#define THREAD_SAVE_ADD(ObjectToAdd, ObjectAddedTo, functionName)  { \
		connect(&tsar(), SIGNAL(addRemoveFinished()), ObjectAddedTo, SIGNAL(functionName##_Signal())); \
		tsar().process_object(ObjectToAdd, ObjectAddedTo, #functionName); \
	}
#define THREAD_SAVE_REMOVE  THREAD_SAVE_ADD

class ContextItem;

struct TsarDataStruct {
	QObject*	objectToBeAdded;
	QObject*	objectToAddTo;
	char*		slot;
};

class Tsar : public QObject
{
	Q_OBJECT

public:

	void add_remove_items_in_audio_processing_path();
	
	void process_object(QObject* objectToBeAdded, QObject* objectsToBeProcessed, char* slot);

private:
	Tsar();
	Tsar(const Tsar&);

	// allow this function to create one instance
	friend Tsar& tsar();

	QTimer			addRemoveRetryTimer;
	QTimer			finishProcessedObjectsTimer;
	QMutex			mutex;
	
	QList<TsarDataStruct >	objectsToBeProcessed;
	QList<TsarDataStruct >	processedObjects;
	
	volatile size_t		processAddRemove;
	

signals:
	void addRemoveFinished();
	void objectsProcessed();

private slots:
	void start_add_remove( );
	void finish_processed_objects();
	
};

#endif

// use this function to access the context pointer
Tsar& tsar();

//eof



