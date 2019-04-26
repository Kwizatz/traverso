/*
    Copyright (C) 2010 Remon Sijrier
 
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

#ifndef GAIN_H
#define GAIN_H

#include "TCommand.h"
#include <QPoint>
#include <QVariantList>

class ContextItem;


class Gain : public TCommand
{
	Q_OBJECT
	
public :
        Gain(ContextItem* context, const QVariantList& args);
        ~Gain();

        int begin_hold();
        int finish_hold();
        int prepare_actions();
        int do_action();
        int undo_action();
        void cancel_action();
	void process_collected_number(const QString & collected);
        
        int jog();
        
        void set_cursor_shape(int useX, int useY);
	bool restoreCursorPosition() const {return true;}

private :
        ContextItem*	m_gainObject;
        float 		m_origGain{};
        float 		m_newGain;
        QPointF		m_origPos;
	
	int get_gain_from_object(float& gain);
	
public slots:
        void increase_gain();
        void decrease_gain();
};

#endif
 
