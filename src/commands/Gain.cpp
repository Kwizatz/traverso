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

#include "Gain.h"

#include "ContextItem.h"
#include "ContextPointer.h"
#include "Mixer.h"
#include <ViewPort.h>
#include <Track.h>
#include "Sheet.h"
#include "TBusTrack.h"
#include "Utils.h"

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

/**
 *	\class Gain
    \brief Change (jog) the Gain of an AudioTrack, BusTrack or AudioClip, or set to a pre-defined value

    \sa TraversoCommands
 */


Gain::Gain(ContextItem* context, const QVariantList& args)
    : TCommand(context, "")
{
    m_gainObject = context;
    m_canvasCursorFollowsMouseCursor = false;

    audio_sample_t gain = -1;
    QString des = "";
    ProcessingData* data = qobject_cast<ProcessingData*>(m_gainObject);
    QString name;
    if (data) {
        name = data->get_name();
    }

    if (!args.empty()) {
        gain = audio_sample_t(args.at(0).toDouble());
        des = QString(context->metaObject()->className()) + ": Reset gain";
    } else {
        des = "Gain (" + QString(context->metaObject()->className()) + " " + name + ")";
    }

    setText(des);

    if (gain >= 0) {
        m_newGain = gain;
        get_gain_from_object(m_origGain);
    }

    Track* track = qobject_cast<Track*>(context);
    if (track && qFuzzyCompare(m_origGain, 0.5f)) {
        m_newGain = 1.0;
    } else {
        Sheet* sheet = qobject_cast<Sheet*>(context);
        if (sheet) {
            // if context == sheet, then use sheets master out
            // as the gain object as sheet itself doesn't apply any gain.
            m_gainObject = sheet->get_master_out_bus_track();
            if (qFuzzyCompare(m_origGain, 0.5f)) {
                m_newGain = 1.0;
            }
        }

    }
}


Gain::~Gain()
{}

int Gain::prepare_actions()
{
    if (qFuzzyCompare(m_origGain, m_newGain)) {
        // Nothing happened!
        return -1;
    }
    return 1;
}

int Gain::begin_hold()
{
    if ( ! get_gain_from_object(m_origGain)) {
        return -1;
    }
    m_newGain = m_origGain;
    m_origPos = cpointer().scene_pos();

    cpointer().setCursorText(coefficient_to_dbstring(m_newGain));
    return 1;
}

int Gain::finish_hold()
{
    return 1;
}

int Gain::do_action()
{
    PENTER;
    if ( ! QMetaObject::invokeMethod(m_gainObject, "set_gain", Q_ARG(float, m_newGain))) {
        PWARN("Gain::do_action QMetaObject::invokeMethod failed");
        return 0;
    }

    return 1;
}

int Gain::undo_action()
{
    PENTER;
    if ( ! QMetaObject::invokeMethod(m_gainObject, "set_gain", Q_ARG(float, m_origGain)) ) {
        PWARN("Gain::undo_action QMetaObject::invokeMethod failed");
        return 0;
    }

    return 1;
}


void Gain::cancel_action()
{
    finish_hold();
    undo_action();
}

void Gain::process_collected_number(const QString & collected)
{
    if (collected.size() == 0) {
        cpointer().setCursorText(" dB");
        return;
    }

    bool ok;
    audio_sample_t dbFactor = audio_sample_t(collected.toDouble(&ok));
    if (!ok) {
        if (collected.contains(".") || collected.contains("-")) {
            QString s = collected;
            s.append(" dB");
            cpointer().setCursorText(s);
        }
        return;
    }

    int rightfromdot = 0;
    if (collected.contains(".")) {
        rightfromdot = collected.size() - collected.lastIndexOf(".") - 1;
    }

    m_newGain = dB_to_scale_factor(dbFactor);

    if (m_newGain < 0.0f)
        m_newGain = 0.0;
    if (m_newGain > 2.0f)
        m_newGain = 2.0;

    // Update the vieport's hold cursor with the _actuall_ gain value!
    if(rightfromdot) {
        cpointer().setCursorText(QByteArray::number(double(dbFactor), 'f', rightfromdot).append(" dB"));
    } else {
        cpointer().setCursorText(QByteArray::number(double(dbFactor)).append(" dB"));
    }

}


void Gain::set_cursor_shape(int useX, int useY)
{
    Q_UNUSED(useX);
    Q_UNUSED(useY);

    cpointer().setCursorShape(":/cursorGain");
}


void Gain::increase_gain(  )
{
    audio_sample_t dbFactor = coefficient_to_dB(m_newGain);
    dbFactor += 0.2f;
    m_newGain = dB_to_scale_factor(dbFactor);
    QMetaObject::invokeMethod(m_gainObject, "set_gain", Q_ARG(float, m_newGain));

    // now we get the new gain value from gainObject, since we don't know if
    // gainobject accepted the change or not!
    get_gain_from_object(m_newGain);

    // Update the vieport's hold cursor with the _actuall_ gain value!
    cpointer().setCursorText(coefficient_to_dbstring(m_newGain));
}

void Gain::decrease_gain()
{
    audio_sample_t dbFactor = coefficient_to_dB(m_newGain);
    dbFactor -= 0.2f;
    m_newGain = dB_to_scale_factor(dbFactor);

    QMetaObject::invokeMethod(m_gainObject, "set_gain", Q_ARG(float, m_newGain));

    // now we get the new gain value from gainObject, since we don't know if
    // gainobject accepted the change or not!
    get_gain_from_object(m_newGain);

    // Update the vieport's hold cursor with the _actuall_ gain value!
    cpointer().setCursorText(coefficient_to_dbstring(m_newGain));
}


int Gain::jog()
{
    PENTER;

    qreal of = 0;

    audio_sample_t dbFactor = coefficient_to_dB(m_newGain);

    qreal diff;

    diff = m_origPos.y() - cpointer().scene_y();

    if (dbFactor > -1) {
        of = diff * 0.05;
    }
    if (dbFactor <= -1) {
        of = diff * ((1 - double(dB_to_scale_factor(dbFactor))) / 3);
    }


    m_newGain = dB_to_scale_factor( dbFactor + float(of) );

    // Set the gain for gainObject
    QMetaObject::invokeMethod(m_gainObject, "set_gain", Q_ARG(float, m_newGain));

    // now we get the new gain value from gainObject, since we don't know if
    // gainobject accepted the change or not!
    int result = get_gain_from_object(m_newGain);

    // Update the vieport's hold cursor!
    cpointer().setCursorPos(m_origPos);
    cpointer().setCursorText(coefficient_to_dbstring(m_newGain));

    return result;
}

int Gain::get_gain_from_object(float& gain)
{
    if ( ! QMetaObject::invokeMethod(m_gainObject, "get_gain",
                                     Qt::DirectConnection,
                                     Q_RETURN_ARG(float, gain)) ) {
        PWARN("Gain::get_gain_from_object QMetaObject::invokeMethod failed");
        return 0;
    }

    return 1;
}
