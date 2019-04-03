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

#ifndef TTRACKLANEVIEW_H
#define TTRACKLANEVIEW_H

#include "TrackView.h"
#include "TrackPanelView.h"

class TTrackLanePanelView;

class TTrackLaneView : public ViewItem
{
	Q_OBJECT

public:
	TTrackLaneView(ViewItem* parent);
	~TTrackLaneView();

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    int get_height() const { return m_height;}
	void set_height(int height);
	void set_child_view(ViewItem* view);
	QString get_name() const;

	void move_to(int x, int y);
	void calculate_bounding_rect();
	void load_theme_data();

private:
	TTrackLanePanelView*	m_panel;
	QString		m_name;
	ViewItem*	m_childView;
	int		m_height;
	bool		m_paintBackground;
};

#endif // TTRACKLANEVIEW_H
