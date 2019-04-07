/*
Copyright (C) 2011 Remon Sijrier

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

#ifndef TKNOBVIEW_H
#define TKNOBVIEW_H


#include "ViewItem.h"

class Track;

class TKnobView : public ViewItem
{
	Q_OBJECT

public:
	TKnobView(ViewItem* parent);
	TKnobView(){}


	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void set_width(int width);
	void load_theme_data();

	void update_angle();

	double max_value() const {return m_maxValue;}
	double min_value() const {return m_minValue;}

    double get_value() {return m_value;}

protected:
    void set_value(double value);

private:
	double		m_angle;
	double		m_nTurns;
	double		m_minValue;
	double		m_maxValue;
    double      m_value;
	double		m_totalAngle;
	QLinearGradient	m_gradient2D;
};

class TPanKnobView : public TKnobView
{
	Q_OBJECT

public:
	TPanKnobView(ViewItem* parent, Track* track);

	Track* get_track() const {return m_track;}

private:
	Track*		m_track;

public slots:
	TCommand* pan_left();
	TCommand* pan_right();

private slots:
	void track_pan_changed();
};

#endif // TKNOBVIEW_H
