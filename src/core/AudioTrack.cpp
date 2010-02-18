/*
Copyright (C) 2005-2007 Remon Sijrier

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

#include "AudioTrack.h"

#include "Sheet.h"
#include "AudioClip.h"
#include "AudioClipManager.h"
#include <AudioBus.h>
#include <AudioDevice.h>
#include "PluginChain.h"
#include "Plugin.h"
#include "InputEngine.h"
#include "Information.h"
#include "ProjectManager.h"
#include "ResourcesManager.h"
#include "Project.h"
#include "SubGroup.h"
#include "Utils.h"
#include <limits.h>
#include <commands.h>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


AudioTrack::AudioTrack(Sheet* sheet, const QString& name, int height )
        : Track(sheet)
{
        PENTERCONS;
        m_name = name;
        m_height = height;
        m_pan = numtakes = 0;
        m_sortIndex = -1;
        m_id = create_id();

        m_busInName = "Capture 1";
        m_busOutName = "Master Out";

        init();
}

AudioTrack::AudioTrack( Sheet * sheet, const QDomNode node)
        : Track(sheet)
{
        PENTERCONS;
        Q_UNUSED(node);
        init();
}


AudioTrack::~AudioTrack()
{
        PENTERDES;
}

void AudioTrack::init()
{
        QObject::tr("Track");
        m_isSolo = mutedBySolo = m_isMuted = isArmed = false;
        m_fader->set_gain(1.0);

        connect(&audiodevice(), SIGNAL(busConfigChanged()), this, SLOT(rescan_busses()), Qt::DirectConnection);
}

QDomNode AudioTrack::get_state( QDomDocument doc, bool istemplate)
{
        QDomElement node = doc.createElement("Track");
        if (! istemplate ) {
                node.setAttribute("id", m_id);
        }
        node.setAttribute("name", m_name);
        node.setAttribute("pan", m_pan);
        node.setAttribute("mute", m_isMuted);
        node.setAttribute("solo", m_isSolo);
        node.setAttribute("mutedbysolo", mutedBySolo);
        node.setAttribute("height", m_height);
        node.setAttribute("sortindex", m_sortIndex);
        node.setAttribute("numtakes", numtakes);
        node.setAttribute("InBus", m_busInName);
        node.setAttribute("OutBus", m_busOutName);

        if (! istemplate ) {
                QDomNode clips = doc.createElement("Clips");

                apill_foreach(AudioClip* clip, AudioClip, m_clips) {
                        if (clip->get_length() == qint64(0)) {
                                PERROR("Clip length is 0! This shouldn't happen!!!!");
                                continue;
                        }

                        QDomElement clipNode = doc.createElement("Clip");
                        clipNode.setAttribute("id", clip->get_id() );
                        clips.appendChild(clipNode);
                }

                node.appendChild(clips);
        }

        QDomNode pluginChainNode = doc.createElement("PluginChain");
        pluginChainNode.appendChild(m_pluginChain->get_state(doc));
        node.appendChild(pluginChainNode);

        return node;
}


int AudioTrack::set_state( const QDomNode & node )
{
        QDomElement e = node.toElement();

        set_height(e.attribute( "height", "160" ).toInt() );
        m_sortIndex = e.attribute( "sortindex", "-1" ).toInt();
        m_name = e.attribute( "name", "" );
        set_muted(e.attribute( "mute", "" ).toInt());
        if (e.attribute( "solo", "" ).toInt()) {
                solo();
        }
        set_muted_by_solo(e.attribute( "mutedbysolo", "0").toInt());
        set_pan( e.attribute( "pan", "" ).toFloat() );
        set_input_bus(e.attribute( "InBus", "Capture 1"));
        set_output_bus(e.attribute( "OutBus", "Master Out"));
        m_id = e.attribute("id", "0").toLongLong();
        if (m_id == 0) {
                m_id = create_id();
        }
        numtakes = e.attribute( "numtakes", "").toInt();

        QDomElement ClipsNode = node.firstChildElement("Clips");
        if (!ClipsNode.isNull()) {
                QDomNode clipNode = ClipsNode.firstChild();
                while (!clipNode.isNull()) {
                        QDomElement clipElement = clipNode.toElement();
                        qint64 id = clipElement.attribute("id", "").toLongLong();

                        AudioClip* clip = resources_manager()->get_clip(id);
                        if (!clip) {
                                info().critical(tr("Track: AudioClip with id %1 not \
                                                found in Resources database!").arg(id));
                                break;
                        }

                        clip->set_sheet(m_sheet);
                        clip->set_track(this);
                        clip->set_state(clip->get_dom_node());
                        m_sheet->get_audioclip_manager()->add_clip(clip);
                        private_add_clip(clip);

                        clipNode = clipNode.nextSibling();
                }
        }

        QDomNode m_pluginChainNode = node.firstChildElement("PluginChain");
        if (!m_pluginChainNode.isNull()) {
                m_pluginChain->set_state(m_pluginChainNode);
        }

        return 1;
}



int AudioTrack::arm()
{
        PENTER;
        set_armed(true);
        AudioBus* bus = audiodevice().get_capture_bus(m_busInName);
        if (bus) {
                bus->set_monitor_peaks(true);
        }
        return 1;
}


int AudioTrack::disarm()
{
        PENTER;
        set_armed(false);
        AudioBus* bus = audiodevice().get_capture_bus(m_busInName);
        if (bus) {
                bus->set_monitor_peaks(false);
        }
        return 1;
}

bool AudioTrack::armed()
{
        return isArmed;
}

AudioClip* AudioTrack::init_recording()
{
        PENTER2;
        if(! isArmed) {
                return 0;
        }
        int number = m_sheet->get_track_index(m_id);
        QString snumber = QString::number(number);
        if (number < 10) {
                snumber.prepend("0");
        }
        QString name = 	"track-" + snumber +
                        "_take-" + QString::number(++numtakes);

        AudioClip* clip = resources_manager()->new_audio_clip(name);
        clip->set_sheet(m_sheet);
        clip->set_track(this);
        clip->set_track_start_location(m_sheet->get_transport_location());

        if (clip->init_recording(m_busInName) < 0) {
                PERROR("Could not create AudioClip to record to!");
                resources_manager()->destroy_clip(clip);
                return 0;
        } else {
                return clip;
        }

        return 0;
}


void AudioTrack::set_gain(float gain)
{
        if(gain < 0.0)
                gain = 0.0;
        if (gain > 2.0)
                gain = 2.0;
        m_fader->set_gain(gain);
        emit stateChanged();
}


void AudioTrack::set_armed( bool armed )
{
        isArmed = armed;
        emit armedChanged(isArmed);
}


//
//  Function called in RealTime AudioThread processing path
//
int AudioTrack::process( nframes_t nframes )
{
        int processResult = 0;

        if ( (m_isMuted || mutedBySolo) && ( ! isArmed) ) {
                return 0;
        }

        // Get the 'render bus' from sheet, a bit hackish solution, but
        // it avoids to have a dedicated render bus for each Track,
        // or buffers located on the heap...
        m_processBus = m_sheet->get_render_bus();
        m_processBus->silence_buffers(nframes);

        int result;
        float gainFactor, panFactor;

        m_pluginChain->process_pre_fader(m_processBus, nframes);

        apill_foreach(AudioClip* clip, AudioClip, m_clips) {
                if (isArmed && clip->recording_state() == AudioClip::NO_RECORDING) {
                        if (m_isMuted || mutedBySolo) {
                                continue;
                        }
                }


                result = clip->process(nframes);

                if (result <= 0) {
                        continue;
                }

                processResult |= result;
        }

        for (int chan=0; chan<m_processBus->get_channel_count(); ++chan) {
                gainFactor = get_gain();

                if ( (chan == 0) && (m_pan > 0)) {
                        panFactor = 1 - m_pan;
                        gainFactor *= panFactor;
                }

                if ( (chan == 1) && (m_pan < 0)) {
                        panFactor = 1 + m_pan;
                        gainFactor *= panFactor;
                }

                Mixer::apply_gain_to_buffer(m_processBus->get_buffer(chan, nframes), nframes, gainFactor);
        }

        processResult |= m_pluginChain->process_post_fader(m_processBus, nframes);

        bool applyFaderGain = false;
        send_to_output_buses(nframes, applyFaderGain);

        return processResult;
}


Command* AudioTrack::toggle_arm()
{
        if (isArmed)
                disarm();
        else
                arm();
        return (Command*) 0;
}


Command* AudioTrack::silence_others( )
{
        PCommand* command = new PCommand(this, "solo", tr("Silence Others"));
        command->set_historable(false);
        return command;
}

void AudioTrack::get_render_range(TimeRef& startlocation, TimeRef& endlocation )
{
        if(m_clips.size() == 0)
                return;

        endlocation = TimeRef();
        startlocation = LLONG_MAX;

        apill_foreach(AudioClip* clip, AudioClip, m_clips) {
                if (! clip->is_muted() ) {
                        if (clip->get_track_end_location() > endlocation) {
                                endlocation = clip->get_track_end_location();
                        }

                        if (clip->get_track_start_location() < startlocation) {
                                startlocation = clip->get_track_start_location();
                        }
                }
        }

}


void AudioTrack::clip_position_changed(AudioClip * clip)
{
        m_clips.sort(clip);
}


QList< AudioClip * > AudioTrack::get_cliplist() const
{
        QList<AudioClip*> list;
        apill_foreach(AudioClip* clip, AudioClip, m_clips) {
                list.append(clip);
        }
        return list;
}


AudioClip* AudioTrack::get_clip_after(const TimeRef& pos)
{
        apill_foreach(AudioClip* clip, AudioClip, m_clips) {
                if (clip->get_track_start_location() > pos) {
                        return clip;
                }
        }
        return (AudioClip*) 0;
}


AudioClip* AudioTrack::get_clip_before(const TimeRef& pos)
{
        apill_foreach(AudioClip* clip, AudioClip, m_clips) {
                if (clip->get_track_start_location() < pos) {
                        return clip;
                }
        }
        return (AudioClip*) 0;
}


Command* AudioTrack::remove_clip(AudioClip* clip, bool historable, bool ismove)
{
        PENTER;
        if (! ismove) {
                m_sheet->get_audioclip_manager()->remove_clip(clip);
        }
        return new AddRemove(this, clip, historable, m_sheet,
                "private_remove_clip(AudioClip*)", "audioClipRemoved(AudioClip*)",
                "private_add_clip(AudioClip*)", "audioClipAdded(AudioClip*)",
                tr("Remove Clip"));
}


Command* AudioTrack::add_clip(AudioClip* clip, bool historable, bool ismove)
{
        PENTER;
        clip->set_track(this);
        if (! ismove) {
                m_sheet->get_audioclip_manager()->add_clip(clip);
        }
        return new AddRemove(this, clip, historable, m_sheet,
                "private_add_clip(AudioClip*)", "audioClipAdded(AudioClip*)",
                "private_remove_clip(AudioClip*)", "audioClipRemoved(AudioClip*)",
                tr("Add Clip"));
}

void AudioTrack::private_add_clip(AudioClip* clip)
{
        m_clips.add_and_sort(clip);
}

void AudioTrack::private_remove_clip(AudioClip* clip)
{
        m_clips.remove(clip);
}

int AudioTrack::get_total_clips()
{
        return m_clips.size();
}
