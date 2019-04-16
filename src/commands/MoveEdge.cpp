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

#include "MoveEdge.h"

#include "AudioClip.h"
#include "ContextPointer.h"
#include "TInputEventDispatcher.h"
#include "Sheet.h"
#include "SnapList.h"
#include <SheetView.h>
#include <AudioClipView.h>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

// FIXME: MoveEdge::jog() continuously calls Snaplist::mark_dirty()

MoveEdge::MoveEdge(AudioClipView* cv, SheetView* sv, QByteArray whichEdge)
	: MoveCommand(cv->get_clip(), tr("Move Clip Edge"))
{
	m_sv = sv;
	m_clip = cv->get_clip();
	m_edge = whichEdge;
}


MoveEdge::~MoveEdge()
{}

int MoveEdge::prepare_actions()
{
	PENTER;

	if (m_newPos == m_originalPos) {
		// Nothing happened!
		return -1;
	}

	return 1;
}

int MoveEdge::begin_hold()
{
	PENTER;
	if (m_edge == "set_left_edge") {
		m_newPos = m_originalPos = m_clip->get_track_start_location();
		m_otherEdgePos = m_clip->get_track_end_location();
		cpointer().setCursorText(tr("Left Edge"), 800);
	}
	if (m_edge == "set_right_edge") {
		m_newPos = m_originalPos = m_clip->get_track_end_location();
		m_otherEdgePos = m_clip->get_track_start_location();
		cpointer().setCursorText(tr("Right Edge"), 800);
	}

	m_clip->set_snappable(false);
	m_sv->stop_follow_play_head();

	return 1;
}


int MoveEdge::finish_hold()
{
	m_clip->set_snappable(true);

	return 1;
}


void MoveEdge::cancel_action()
{
	finish_hold();
	undo_action();
}


int MoveEdge::do_action()
{
	if (m_edge == "set_right_edge") {
		m_clip->set_right_edge(m_newPos);
	}

	if (m_edge == "set_left_edge") {
		m_clip->set_left_edge(m_newPos);
	}

	return 1;
}


int MoveEdge::undo_action()
{
	if (m_edge == "set_right_edge") {
		m_clip->set_right_edge(m_originalPos);
	}

	if (m_edge == "set_left_edge") {
		m_clip->set_left_edge(m_originalPos);
	}

	return 1;
}


int MoveEdge::jog()
{
	m_newPos = TimeRef(cpointer().scene_x() * m_sv->timeref_scalefactor);

	if (m_sv->get_sheet()->is_snap_on()) {
		SnapList* slist = m_sv->get_sheet()->get_snap_list();
		m_newPos = slist->get_snap_value(m_newPos);
	}

	if (m_edge == "set_right_edge" && m_newPos < (m_otherEdgePos + (2 * m_sv->timeref_scalefactor)) ) {
		m_newPos = m_otherEdgePos + (2 * m_sv->timeref_scalefactor);
	}

	if (m_edge == "set_left_edge" && m_newPos > (m_otherEdgePos - (2 * m_sv->timeref_scalefactor)) ) {
		m_newPos = m_otherEdgePos - (2 * m_sv->timeref_scalefactor);
	}

	if (m_edge == "set_right_edge") {
		m_clip->set_right_edge(m_newPos);
		m_newPos = m_clip->get_track_end_location();
	}

	if (m_edge == "set_left_edge") {
		m_clip->set_left_edge(m_newPos);
		m_newPos = m_clip->get_track_start_location();
	}

	cpointer().setCursorText(timeref_to_text(m_newPos, m_sv->timeref_scalefactor));

	return 1;
}


void MoveEdge::move_left()
{
	if (m_doSnap)
	{
        return prev_snap_pos();
	}
	m_newPos = m_newPos - (m_sv->timeref_scalefactor * m_speed);
	do_keyboard_move();
}

void MoveEdge::move_right()
{
	if (m_doSnap)
	{
        return next_snap_pos();
	}
	m_newPos = m_newPos + (m_sv->timeref_scalefactor * m_speed);
	do_keyboard_move();
}

void MoveEdge::next_snap_pos()
{
	
	SnapList* slist = m_sv->get_sheet()->get_snap_list();
	m_newPos = slist->next_snap_pos(m_newPos);
	do_keyboard_move();
}

void MoveEdge::prev_snap_pos()
{
	
	SnapList* slist = m_sv->get_sheet()->get_snap_list();
	m_newPos = slist->prev_snap_pos(m_newPos);
	do_keyboard_move();
}

void MoveEdge::do_keyboard_move()
{
	ied().bypass_jog_until_mouse_movements_exceeded_manhattenlength();

	do_action();

	if (m_edge == "set_right_edge") {
		m_newPos = m_clip->get_track_end_location();
	}

	if (m_edge == "set_left_edge") {
		m_newPos = m_clip->get_track_start_location();
	}

	cpointer().setCursorText(timeref_to_text(m_newPos, m_sv->timeref_scalefactor));
}

// eof

