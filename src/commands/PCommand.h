/*
    Copyright (C) 2005-2009 Remon Sijrier
 
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

#ifndef PCOMMAND_H
#define PCOMMAND_H

#include "TCommand.h"

#include <QVariant>

class ContextItem;

class PCommand : public TCommand
{
        Q_OBJECT

public :
        PCommand(ContextItem* item, const char* slot, const QString& des);
	PCommand(ContextItem* item, const char* slot, const QVariant &doValue, const QVariant &undoValue, const QString& des);
        ~PCommand();

        int prepare_actions();
        int do_action();
        int undo_action();
        bool is_hold_command() const {return false;}

private :
        ContextItem*	m_contextitem;
        const char*	m_slot;
	QVariant	m_doValue;
	QVariant	m_undoValue;

};

#endif
 
