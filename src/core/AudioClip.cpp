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

#include <cfloat>
#include <QInputDialog>

#include "ContextItem.h"
#include "ReadSource.h"
#include "AudioClip.h"
#include "AudioSource.h"
#include "WriteSource.h"
#include "Song.h"
#include "SnapList.h"
#include "Track.h"
#include "AudioChannel.h"
#include <AudioBus.h>
#include <AudioDevice.h>
#include "Mixer.h"
#include "DiskIO.h"
#include "Export.h"
#include "AudioClipManager.h"
#include "ResourcesManager.h"
#include "Curve.h"
#include "FadeCurve.h"
#include "Tsar.h"
#include "ProjectManager.h"
#include "Peak.h"
#include "ContextPointer.h"
#include "Project.h"
#include "Utils.h"
#include "Information.h"
#include <Config.h>

#include <commands.h>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


AudioClip::AudioClip(const QString& name)
	: ContextItem()
	, Snappable()
	, m_name(name)
{
	PENTERCONS;
	m_gain = m_normfactor = 1.0;
	m_length = sourceStartFrame = sourceEndFrame = trackEndFrame = 0;
	isMuted=false;
	m_id = create_id();
	m_readSourceId = m_songId = 0;
	init();
}


AudioClip::AudioClip(const QDomNode& node)
	: ContextItem()
	, Snappable()
{
	PENTERCONS;
	QDomNode clipNode = node.firstChild();
	
	// It makes sense to set these values at this time allready
	// they are for example used by the ResourcesManager!
	QDomElement e = node.toElement();
	m_id = e.attribute("id", "").toLongLong();
	m_readSourceId = e.attribute("source", "").toLongLong();
	m_songId = e.attribute("sheet", "0").toLongLong();
	m_name = e.attribute( "clipname", "" ) ;
	isMuted =  e.attribute( "mute", "" ).toInt();
	m_length = e.attribute( "length", "0" ).toUInt();
	sourceStartFrame = e.attribute( "sourcestart", "" ).toUInt();
	sourceEndFrame = sourceStartFrame + m_length;
	set_track_start_frame( e.attribute( "trackstart", "" ).toUInt());
	m_domNode = node.cloneNode();
	init();
}

AudioClip::~AudioClip()
{
	PENTERDES;
	if (m_readSource) {
		m_song->get_diskio()->unregister_read_source(m_readSource);
		delete m_readSource;
	}
}

void AudioClip::init()
{
	m_song = 0;
	m_track = 0;
	m_readSource = 0;
	m_recordingStatus = NO_RECORDING;
	isSelected = m_invalidReadSource = false;
	isLocked = config().get_property("AudioClip", "LockByDefault", false).toBool();
	fadeIn = 0;
	fadeOut = 0;
	m_refcount = 0;
	m_gainEnvelope = 0;
}

int AudioClip::set_state(const QDomNode& node)
{
	PENTER;
	
	QDomElement e = node.toElement();

	isTake = e.attribute( "take", "").toInt();
	set_gain( e.attribute( "gain", "" ).toFloat() );
	m_normfactor =  e.attribute( "normfactor", "1.0" ).toFloat();

	isLocked = e.attribute( "locked", "0" ).toInt();

	if (e.attribute("selected", "0").toInt() == 1) {
		m_song->get_audioclip_manager()->select_clip(this);
	}

	m_readSourceId = e.attribute("source", "").toLongLong();
	m_songId = e.attribute("sheet", "0").toLongLong();
	isMuted =  e.attribute( "mute", "" ).toInt();

	sourceStartFrame = e.attribute( "sourcestart", "" ).toUInt();
	m_length = e.attribute( "length", "0" ).toUInt();
	sourceEndFrame = sourceStartFrame + m_length;
	set_track_start_frame( e.attribute( "trackstart", "" ).toUInt());
	
	QDomElement curvesNode = node.firstChildElement("Curves");
	if (!curvesNode.isNull()) {
		QDomElement fadeInNode = curvesNode.firstChildElement("FadeIn");
		if (!fadeInNode.isNull()) {
			fadeIn = new FadeCurve(this, m_song, "FadeIn");
			fadeIn->set_state( fadeInNode );
			fadeIn->set_history_stack(get_history_stack());
			private_add_fade(fadeIn);
		}

		QDomElement fadeOutNode = curvesNode.firstChildElement("FadeOut");
		if (!fadeOutNode.isNull()) {
			fadeOut = new FadeCurve(this, m_song, "FadeOut");
			fadeOut->set_state( fadeOutNode );
			fadeOut->set_history_stack(get_history_stack());
			private_add_fade(fadeOut);
		}
		
		QDomElement m_gainEnvelopeNode = curvesNode.firstChildElement("GainCurve");
		if (!m_gainEnvelopeNode.isNull()) {
			m_gainEnvelope->set_state( m_gainEnvelopeNode );
		} else {
			init_gain_envelope();
		}
	}

	return 1;
}

QDomNode AudioClip::get_state( QDomDocument doc )
{
	QDomElement node = doc.createElement("Clip");
	node.setAttribute("trackstart", trackStartFrame);
	node.setAttribute("sourcestart", sourceStartFrame);
	node.setAttribute("length", m_length);
	node.setAttribute("gain", m_gain);
	node.setAttribute("normfactor", m_normfactor);
	node.setAttribute("mute", isMuted);
	node.setAttribute("take", isTake);
	node.setAttribute("clipname", m_name );
	node.setAttribute("selected", isSelected );
	node.setAttribute("id", m_id );
	node.setAttribute("sheet", m_songId );
	node.setAttribute("locked", isLocked);

	node.setAttribute("source", m_readSourceId);

	QDomNode curves = doc.createElement("Curves");

	if (fadeIn) {
		curves.appendChild(fadeIn->get_state(doc));
	}
	if (fadeOut) {
		curves.appendChild(fadeOut->get_state(doc));
	}
	curves.appendChild(m_gainEnvelope->get_state(doc, "GainCurve"));

	node.appendChild(curves);

	return node;
}

void AudioClip::toggle_mute()
{
	PENTER;
	isMuted=!isMuted;
	set_sources_active_state();
	emit muteChanged();
}

void AudioClip::toggle_lock()
{
	isLocked = !isLocked;
	emit lockChanged();
}

void AudioClip::track_audible_state_changed()
{
	set_sources_active_state();
}

void AudioClip::set_sources_active_state()
{
	if (! m_track) {
		return;
	}
	
	if (! m_readSource) {
		return;
	}
	
	if ( m_track->is_muted() || m_track->is_muted_by_solo() || is_muted() ) {
		m_readSource->set_active(false);
	} else {
		m_readSource->set_active(true);
	}

}

void AudioClip::set_left_edge(long newFrame)
{
	if (newFrame < 0) {
		newFrame = 0;
	}
	
	if (newFrame < (long)trackStartFrame) {

		int availableFramesLeft = sourceStartFrame;

		int movingToLeft = trackStartFrame - newFrame;

		if (movingToLeft > availableFramesLeft) {
			movingToLeft = availableFramesLeft;
		}

		trackStartFrame -= movingToLeft;
		set_source_start_frame( sourceStartFrame - movingToLeft );

	} else if (newFrame > (long)trackStartFrame) {

		int availableFramesRight = m_length;

		int movingToRight = newFrame - trackStartFrame;

		if (movingToRight > availableFramesRight) {
			movingToRight = availableFramesRight;
		}

		trackStartFrame += movingToRight;
		set_source_start_frame( sourceStartFrame + movingToRight );

	} else {
		return;
	}

	emit positionChanged(this);
}

void AudioClip::set_right_edge(long newFrame)
{
	if (newFrame < 0) {
		newFrame = 0;
	}
	
	if (newFrame > (long)trackEndFrame) {

		int availableFramesRight = sourceLength - sourceEndFrame;

		int movingToRight = newFrame - trackEndFrame;

		if (movingToRight > availableFramesRight) {
			movingToRight = availableFramesRight;
		}

		set_track_end_frame( trackEndFrame + movingToRight );
		set_source_end_frame( sourceEndFrame + movingToRight );

	} else if (newFrame < (long)trackEndFrame) {

		int availableFramesLeft = m_length;

		int movingToLeft = trackEndFrame - newFrame;

		if (movingToLeft > availableFramesLeft) {
			movingToLeft = availableFramesLeft;
		}

		set_track_end_frame( trackEndFrame - movingToLeft );
		set_source_end_frame( sourceEndFrame - movingToLeft);

	} else {
		return;
	}

	emit positionChanged(this);
}

void AudioClip::set_source_start_frame(nframes_t frame)
{
	sourceStartFrame = frame;
	m_length = sourceEndFrame - sourceStartFrame;
}

void AudioClip::set_source_end_frame(nframes_t frame)
{
	sourceEndFrame = frame;
	m_length = sourceEndFrame - sourceStartFrame;
}

void AudioClip::set_track_start_frame(nframes_t newTrackStartFrame)
{
	trackStartFrame = newTrackStartFrame;

	set_track_end_frame(trackStartFrame + m_length);

	emit positionChanged(this);
}

void AudioClip::set_track_end_frame( nframes_t endFrame )
{
// 	PWARN("trackEndFrame is %d", endFrame);
	trackEndFrame = endFrame;
	emit trackEndFrameChanged();
}

void AudioClip::set_fade_in(nframes_t b)
{
	if (!fadeIn) {
		create_fade_in();
	}
	fadeIn->set_range( b );
}

void AudioClip::set_fade_out(nframes_t b)
{
	if (!fadeOut) {
		create_fade_out();
	}
	fadeOut->set_range( b );
}

void AudioClip::set_gain(float gain)
{
	PENTER3;
	if (gain < 0.0)
		gain = 0.0;
	if (gain > 32.0)
		gain = 32.0;
	m_gain = gain;
	emit gainChanged();
}

void AudioClip::set_selected(bool selected)
{
	isSelected = selected;
	emit stateChanged();
}

//
//  Function called in RealTime AudioThread processing path
//
int AudioClip::process(nframes_t nframes, audio_sample_t* buffer, uint channel)
{
	Q_ASSERT(m_song);
	
	if (m_recordingStatus == RECORDING) {
		process_capture(nframes, channel);
		return 0;
	}

	if (m_invalidReadSource) {
		return -1;
	}

	if (isMuted || ( (m_gain * m_normfactor) == 0.0f) ) {
		return 0;
	}
	
	Q_ASSERT(m_readSource);
	
	if (channel >= m_readSource->get_channel_count()) {
		return -1;
	}
	
	nframes_t mix_pos;
	audio_sample_t* mixdown;


	nframes_t transportFrame = m_song->get_transport_frame();
	nframes_t upperRange = transportFrame + nframes;
	
	if ( (trackStartFrame < upperRange) && (trackEndFrame > transportFrame) ) {
		if (transportFrame < trackStartFrame) {
			uint offset = trackStartFrame - transportFrame;
			mix_pos = sourceStartFrame;
			mixdown = buffer + offset;
			nframes = nframes - offset;
		} else {
			mix_pos = transportFrame - trackStartFrame + sourceStartFrame;
			mixdown = buffer;
		}
	} else {
		return 0;
	}


	nframes_t read_frames = 0;


	if (m_song->realtime_path()) {
		read_frames = m_readSource->rb_read(channel, mixdown, mix_pos, nframes);
	} else {
		read_frames = m_readSource->file_read(channel, mixdown, mix_pos, nframes);
	}

	if (read_frames == 0) {
		return 0;
	}


	for (int i=0; i<m_fades.size(); ++i) {
		m_fades.at(i)->process(mixdown, read_frames);
	}
	
	m_gainEnvelope->process(mixdown, (m_song->get_transport_frame() - (trackStartFrame - sourceStartFrame)), read_frames);
	
	return 1;
}

//
//  Function called in RealTime AudioThread processing path
//
void AudioClip::process_capture( nframes_t nframes, uint channel )
{
	if (channel == 0) {
		if ( ! m_track->capture_left_channel() ) {
			return;
		}
	}
		
	if (channel == 1) {
		if ( ! m_track->capture_right_channel()) {
			return;
		}
	}
	
	if (!m_captureBus) {
		return;
	}
	
	m_length += (nframes / writeSources.size());
	
	int index = 0;
	if (m_track->capture_left_channel() && m_track->capture_right_channel()) {
		index = channel;
	}
	
	WriteSource* source = writeSources.at(index);

	nframes_t written = source->rb_write(m_captureBus->get_buffer(channel, nframes), nframes);

	if (written != nframes) {
		printf("couldn't write nframes %d to recording buffer, only %d\n", nframes, written);
	}
}

int AudioClip::init_recording( QByteArray name )
{
	Q_ASSERT(m_song);
	Q_ASSERT(m_track);
	
	m_captureBusName = name;
	get_capture_bus();

	if (!m_captureBus) {
		info().critical(tr("Unable to Record to Track"));
		info().warning(tr("AudioDevice doesn't have this Capture Bus: %1 (Track %2)").
				arg(name.data()).arg(m_track->get_id()) );
		return -1;
	}

	int channelnumber = 0;
	int channelcount = m_captureBus->get_channel_count();
	if (! (m_track->capture_left_channel() && m_track->capture_right_channel()) ) {
		channelcount = 1;
	}

	ReadSource* rs = resources_manager()->create_recording_source(
				pm().get_project()->get_root_dir() + "/audiosources/",
				m_name, channelcount, m_song->get_id());
	
	resources_manager()->set_source_for_clip(this, rs);
	QString sourceid = QString::number(rs->get_id());
	
	for (int chan=0; chan<m_captureBus->get_channel_count(); chan++) {
		if (chan == 0) {
			if ( ! m_track->capture_left_channel() ) {
				continue;
			}
		}
		
		if (chan == 1) {
			if ( ! m_track->capture_right_channel()) {
				continue;
			}
		}
		
		if (m_track->capture_left_channel() && m_track->capture_right_channel()) {
			channelnumber = chan;
		}
		
		ExportSpecification* spec = new ExportSpecification;

		spec->exportdir = pm().get_project()->get_root_dir() + "/audiosources/";
		spec->format = SF_FORMAT_WAV;
		spec->data_width = 1;	// 1 means float
		spec->format |= SF_FORMAT_FLOAT;
		spec->channels = 1;
		spec->sample_rate = audiodevice().get_sample_rate();
		spec->src_quality = SRC_SINC_MEDIUM_QUALITY;
		spec->isRecording = true;
		spec->start_frame = 0;
		spec->end_frame = 0;
		spec->total_frames = 0;
		spec->blocksize = audiodevice().get_buffer_size();
		spec->name = m_name + "-" + sourceid;
		spec->dataF = m_captureBus->get_buffer( chan, audiodevice().get_buffer_size());

		WriteSource* ws = new WriteSource(spec, channelnumber, channelcount);
		ws->set_process_peaks( true );
		ws->set_recording( true );

		connect(ws, SIGNAL(exportFinished(WriteSource*)), this, SLOT(finish_write_source(WriteSource*)));

		writeSources.insert(channelnumber, ws);
		m_song->get_diskio()->register_write_source( ws );
	}

	sourceStartFrame = 0;
	isTake = 1;
	m_recordingStatus = RECORDING;
	
	init_gain_envelope();
	
	connect(m_song, SIGNAL(transferStopped()), this, SLOT(finish_recording()));
	connect(&audiodevice(), SIGNAL(driverParamsChanged()), this, SLOT(get_capture_bus()));

	return 1;
}

Command* AudioClip::mute()
{
	PCommand* cmd = new PCommand(this, "toggle_mute", tr("Toggle Mute"));
	cmd->set_historable(false);
	return cmd;
}

Command* AudioClip::lock()
{
	PCommand* cmd = new PCommand(this, "toggle_lock", tr("Toggle Lock"));
	cmd->set_historable(false);
	return cmd;
}

Command* AudioClip::reset_fade_in()
{
	return new FadeRange(this, fadeIn, 1.0);
}

Command* AudioClip::reset_fade_out()
{
	return new FadeRange(this, fadeOut, 1.0);
}

Command* AudioClip::reset_fade_both()
{
	CommandGroup* group = new CommandGroup(this, tr("Reset Fades"));
	group->add_command(reset_fade_in());
	group->add_command(reset_fade_out());

	return group;
}

AudioClip * AudioClip::prev_clip( )
{
	Q_ASSERT(m_track);
	return m_track->get_cliplist().prev(this);
}

AudioClip * AudioClip::next_clip( )
{
	Q_ASSERT(m_track);
	return m_track->get_cliplist().next(this);
}

AudioClip* AudioClip::create_copy( )
{
	PENTER;
	Q_ASSERT(m_song);
	Q_ASSERT(m_track);
	QDomDocument doc("AudioClip");
	QDomNode clipState = get_state(doc);
	AudioClip* clip = new AudioClip(m_name);
	clip->set_song(m_song);
	clip->set_track(m_track);
	clip->set_state(clipState);
	return clip;
}

Peak* AudioClip::get_peak_for_channel( int chan ) const
{
	PENTER2;
	Q_ASSERT(m_readSource);
	return m_readSource->get_peak(chan);
}

void AudioClip::set_audio_source(ReadSource* rs)
{
	PENTER;
	
	if (!rs) {
		m_invalidReadSource = true;
		return;
	}
	
	if (rs->get_error() < 0) {
		m_invalidReadSource = true;
	} else {
		m_invalidReadSource = false;
	}
		
	m_readSource = rs;
	m_readSourceId = rs->get_id();
	sourceLength = rs->get_nframes();

	// If m_length isn't set yet, it means we are importing stuff instead of reloading from project file.
	// it's a bit weak this way, hopefull I'll get up something better in the future.
	// The positioning-length-offset and such stuff is still a bit weak :(
	// NOTE: don't change, audio recording (finish_writesource()) assumes there is checked for length == 0 !!!
	if (m_length == 0) {
		sourceEndFrame = rs->get_nframes();
		m_length = sourceEndFrame;
	}

	set_track_end_frame( trackStartFrame + sourceLength - sourceStartFrame);

	set_sources_active_state();

	rs->set_audio_clip(this);

	emit stateChanged();
}

void AudioClip::finish_write_source( WriteSource * ws )
{
	PENTER;

	if (writeSources.contains(ws)) {
		writeSources.removeAll(ws);
		if (ws->m_peak->finish_processing() < 0) {
			PERROR("write source peak::finish_processing() failed!");
		}
		delete ws;
	} else {
		qFatal("AudioClip: finished writesource not in writesources list !!");
	}
		
	
	if (writeSources.isEmpty()) {
		Q_ASSERT(m_readSource);
		
		if (m_readSource->set_file(m_readSource->get_filename()) < 0) {
			PERROR("Setting file for ReadSource failed after finishing recording");
		}
		
		m_song->get_diskio()->register_read_source(m_readSource);
		// re-inits the lenght from the audiofile due calling rsm->set_source_for_clip()
		m_length = 0;
		resources_manager()->set_source_for_clip(this, m_readSource);
		m_recordingStatus = NO_RECORDING;
		
		emit recordingFinished();
	}
}

void AudioClip::finish_recording()
{
	PENTER;
	
	m_recordingStatus = FINISHING_RECORDING;

	foreach(WriteSource* ws, writeSources) {
		ws->set_recording(false);
	}

	disconnect(m_song, SIGNAL(transferStopped()), this, SLOT(finish_recording()));
	connect(&audiodevice(), SIGNAL(driverParamsChanged()), this, SLOT(get_capture_bus()));
}

int AudioClip::get_channels( ) const
{
	if (m_readSource) {
		return m_readSource->get_channel_count();
	} else {
		if (writeSources.size()) {
			return writeSources.size();
		}
	}
	
	return 0;
}

Song* AudioClip::get_song( ) const
{
	Q_ASSERT(m_song);
	return m_song;
}

Track* AudioClip::get_track( ) const
{
	Q_ASSERT(m_track);
	return m_track;
}

void AudioClip::set_song( Song * song )
{
	m_song = song;
	if (m_readSource) {
		m_song->get_diskio()->register_read_source( m_readSource );
	} else {
		PWARN("AudioClip::set_song() : Setting Song, but no ReadSource available!!");
	}
	
	m_songId = song->get_id();
	
	set_history_stack(m_song->get_history_stack());
	
	if (!m_gainEnvelope) {
		m_gainEnvelope = new Curve(this, m_song);
	}
	
	m_gainEnvelope->set_history_stack(get_history_stack());

	set_snap_list(m_song->get_snap_list());
}


void AudioClip::set_track( Track * track )
{
	if (m_track) {
		disconnect(m_track, SIGNAL(audibleStateChanged()), this, SLOT(track_audible_state_changed()));
	}
	
	m_track = track;
	
	connect(m_track, SIGNAL(audibleStateChanged()), this, SLOT(track_audible_state_changed()));
	set_sources_active_state();
}

void AudioClip::set_name( const QString& name )
{
	m_name = name;
}

float AudioClip::get_gain( ) const
{
	return m_gain;
}

float AudioClip::get_norm_factor( ) const
{
	return m_normfactor;
}

bool AudioClip::is_selected( ) const
{
	return isSelected;
}

bool AudioClip::is_take( ) const
{
	return isTake;
}

bool AudioClip::is_muted( ) const
{
	return isMuted;
}

bool AudioClip::is_locked( ) const
{
	return isLocked;
}

QString AudioClip::get_name( ) const
{
	return m_name;
}

int AudioClip::get_bitdepth( ) const
{
	if (m_readSource) {
		return m_readSource->get_bit_depth();
	}
	
	return audiodevice().get_bit_depth();

}

int AudioClip::get_rate( ) const
{
	if (m_readSource) {
		return m_readSource->get_rate();
	}
	
	return audiodevice().get_sample_rate();
}

nframes_t AudioClip::get_source_length( ) const
{
	return sourceLength;
}

nframes_t AudioClip::get_length() const
{
	return m_length;
}

int AudioClip::recording_state( ) const
{
	return m_recordingStatus;
}

nframes_t AudioClip::get_source_end_frame( ) const
{
	return sourceEndFrame;
}

nframes_t AudioClip::get_source_start_frame( ) const
{
	return sourceStartFrame;
}

nframes_t AudioClip::get_track_end_frame( ) const
{
	return trackEndFrame;
}

nframes_t AudioClip::get_track_start_frame( ) const
{
	return trackStartFrame;
}


Command * AudioClip::clip_fade_in( )
{
	if (!fadeIn) {
		create_fade_in();
	}
	return new FadeRange(this, fadeIn);
}

Command * AudioClip::clip_fade_out( )
{
	if (!fadeOut) {
		create_fade_out();
	}
	return new FadeRange(this, fadeOut);
}

Command * AudioClip::normalize( )
{
        bool ok;
        double d = QInputDialog::getDouble(0, tr("Normalization"),
                                           tr("Set Normalization level:"), 0.0, -120, 0, 1, &ok);
        if (ok) {
		calculate_normalization_factor(d);
	}

	emit gainChanged();

	return (Command*) 0;
}

Command * AudioClip::denormalize( )
{
	m_normfactor = 1.0;
	// Hmm, this is not entirely true, but "almost" ;-)
	emit gainChanged();

	return (Command*) 0;
}

void AudioClip::calculate_normalization_factor(float targetdB)
{
	double maxamp = 0;

	float target = dB_to_scale_factor (targetdB);

	if (target == 1.0f) {
		/* do not normalize to precisely 1.0 (0 dBFS), to avoid making it appear
		   that we may have clipped.
		*/
		target -= FLT_EPSILON;
	}

	for (uint i=0; i<m_readSource->get_channel_count(); ++i) {
		maxamp = f_max(m_readSource->get_peak(i)->get_max_amplitude(sourceStartFrame, sourceEndFrame), maxamp);
	}

	if (maxamp == 0.0f) {
		printf("AudioClip::normalization: max amplitude == 0\n");
		/* don't even try */
		return;
	}

	if (maxamp == target) {
		printf("AudioClip::normalization: max amplitude == target amplitude\n");
		/* we can't do anything useful */
		return;
	}

	/* compute scale factor */
	m_normfactor = target/maxamp;
}

FadeCurve * AudioClip::get_fade_in( )
{
	return fadeIn;
}

FadeCurve * AudioClip::get_fade_out( )
{
	return fadeOut;
}

void AudioClip::private_add_fade( FadeCurve* fade )
{
	m_fades.append(fade);
	
	if (fade->get_fade_type() == FadeCurve::FadeIn) {
		fadeIn = fade;
	} else if (fade->get_fade_type() == FadeCurve::FadeOut) {
		fadeOut = fade;
	}
}

void AudioClip::private_remove_fade( FadeCurve * fade )
{
	if (fade == fadeIn) {
		fadeIn = 0;
	} else if (fade == fadeOut) {
		fadeOut = 0;
	}
	
	m_fades.removeAll(fade);
}

// int AudioClip::get_ref_count( ) const
// {
// 	return m_refcount;
// }

void AudioClip::create_fade_in( )
{
	fadeIn = new FadeCurve(this, m_song, "FadeIn");
	fadeIn->set_shape("Linear");
	fadeIn->set_history_stack(get_history_stack());
	THREAD_SAVE_CALL_EMIT_SIGNAL(this, fadeIn, private_add_fade(FadeCurve*), fadeAdded(FadeCurve*));
}

void AudioClip::create_fade_out( )
{
	fadeOut = new FadeCurve(this, m_song, "FadeOut");
	fadeOut->set_shape("Linear");
	fadeOut->set_history_stack(get_history_stack());
	THREAD_SAVE_CALL_EMIT_SIGNAL(this, fadeOut, private_add_fade(FadeCurve*), fadeAdded(FadeCurve*));
}

void AudioClip::init_gain_envelope()
{
	// FIXME Somehow Curves always should have 1 node, which is not allowed 
	// to be removed, or moved horizontally, to avoid code like below..... !!!!!
	// Add the default (first) node to the Gain Curve
	CurveNode* node = new CurveNode(m_gainEnvelope, 0.0, 1.0);
	Command::process_command(m_gainEnvelope->add_node(node, false));
}

QDomNode AudioClip::get_dom_node() const
{
	return m_domNode;
}

bool AudioClip::has_song() const
{
	if (m_song) {
		return true;
	}
	return false;
}

ReadSource * AudioClip::get_readsource() const
{
	return m_readSource;
}

void AudioClip::get_capture_bus()
{
	m_captureBus = audiodevice().get_capture_bus(m_captureBusName);
}

// eof

