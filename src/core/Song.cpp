/*
    Copyright (C) 2005-2006 Remon Sijrier 
 
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
 
    $Id: Song.cpp,v 1.4 2006/05/01 21:21:37 r_sijrier Exp $
*/

#include <QTextStream>
#include <QMessageBox>
#include <QString>
#include <QFileDialog>
#include <QSettings>
#include <QDir>

#include <libtraverso.h>
#include <commands.h>

#include <Client.h>
#include "ProjectManager.h"
#include "Song.h"
#include "Project.h"
#include "Track.h"
#include "Mixer.h"
#include "AudioSource.h"
#include "AudioClip.h"
#include "MtaRegion.h"
#include "MtaRegionList.h"
#include "Peak.h"
#include "AudioPluginChain.h"
#include "AudioPluginSelector.h"
#include "Export.h"
#include "DiskIO.h"
#include "WriteSource.h"
#include "AudioClipManager.h"

#include "LocatorView.h"
#include "ContextItem.h"

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


Song::Song(Project* project, int number)
		: ContextItem(), m_project(project), m_id(number)
{
	PENTERCONS;
	title="Untitled";
	masterGain = 0.0f;
	artists = "No artists name yet";
	QSettings settings;
	m_hzoom = settings.value("hzoomLevel").toInt();
	int tracksToCreate = settings.value("trackCreationCount").toInt();
	regionList = (MtaRegionList*) 0;

	init();
	emit m_project->newSongCreated( this );

	activeTrackNumber = 1;
	for (int i=1; i <= tracksToCreate; i++)
		create_track();

	connect_to_audiodevice();
}

Song::Song(Project* project, const QDomNode node)
		: ContextItem(), m_project(project)
{
	PENTERCONS;
	init();
	emit m_project->newSongCreated( this );
	set_state( node );
	
	connect_to_audiodevice();
}

Song::~Song()
{
	PENTERDES;
	
	if (transport) {
		qCritical("Song still running on deletion! (song_%d : %s) \n\n"
		"PLEASE report this error, it's a very critical problem!!!\n\n", m_id, title.toAscii().data());
	}

	delete [] mixdown;
	delete diskio;

	delete regionList;
	delete masterOut;
	delete m_hs;
}

void Song::init()
{
	PENTER2;
	diskio = new DiskIO();

	connect(this, SIGNAL(seekStart(uint )), diskio, SLOT(seek( uint )), Qt::QueuedConnection);
	connect(&audiodevice(), SIGNAL(clientRequestsProcesssed()), this, SLOT (audiodevice_client_request_processed() ), Qt::QueuedConnection);
	connect(&audiodevice(), SIGNAL(started()), this, SLOT(audiodevice_started()));
	connect(&audiodevice(), SIGNAL(driverParamsChanged()), this, SLOT(resize_buffer()), Qt::DirectConnection);
	connect(diskio, SIGNAL(seekFinished()), this, SLOT(seek_finished()), Qt::QueuedConnection);
	connect (diskio, SIGNAL(outOfSync()), this, SLOT(handle_diskio_outofsync()));

	mixdown = new audio_sample_t[audiodevice().get_buffer_size()];
	masterOut = new AudioBus("Master Out", 2);
	regionList = new MtaRegionList();
	m_hs = new HistoryStack();
	acmanager = new AudioClipManager(this);
	connect(acmanager, SIGNAL(lastFramePositionChanged()), this, SIGNAL(lastFramePositionChanged()));
	
	set_context_item( acmanager );

	playBackBus = audiodevice().get_playback_bus("Playback 1");
	
	transport = stopTransport = resumeTransport = false;
	realtimepath = false;
	scheduleForDeletion = false;
	isSnapOn=true;
	changed = rendering = false;
	firstFrame=workingFrame=activeTrackNumber=0;
}

int Song::set_state( const QDomNode & node )
{
	PENTER;
	QDomNode propertiesNode = node.firstChildElement("Properties");
	m_id = node.toElement().attribute( "id", "" ).toInt();

	QDomElement e = propertiesNode.toElement();

	title = e.attribute( "title", "" );
	artists = e.attribute( "artists", "" );
	activeTrackNumber = e.attribute( "activeTrackNumber", "" ).toInt();
	set_hzoom(e.attribute( "hzoom", "" ).toInt());
	set_first_block(e.attribute( "firstFrame", "" ).toUInt());
	set_work_at(e.attribute( "workingFrame", "0").toUInt());
	int trackBaseY = LocatorView::LOCATOR_HEIGHT;

	QDomNode tracksNode = node.firstChildElement("Tracks");
	QDomNode trackNode = tracksNode.firstChild();
	while(!trackNode.isNull()) {
		Track* track = new Track(this, trackNode);
		track->set_baseY(trackBaseY);
		foreach(Track* existingTrack, m_tracks) {
			if (existingTrack->is_solo()) {
				track->set_muted_by_solo( true );
				break;
			}
		}
		add_track( track, track->get_id());
		trackBaseY += track->get_height();
		trackNode = trackNode.nextSibling();
	}

	return 1;
}

QDomNode Song::get_state(QDomDocument doc)
{
	QDomElement songNode = doc.createElement("Song");
	songNode.setAttribute("id", m_id);
	QDomElement properties = doc.createElement("Properties");
	properties.setAttribute("title", title);
	properties.setAttribute("artists", artists);
	properties.setAttribute("activeTrackNumber", activeTrackNumber);
	properties.setAttribute("firstFrame", firstFrame);
	properties.setAttribute("workingFrame", workingFrame);
	properties.setAttribute("hzoom", m_hzoom);
	songNode.appendChild(properties);

	doc.appendChild(songNode);

	QDomNode tracksNode = doc.createElement("Tracks");

	foreach(Track* track, m_tracks) {
		tracksNode.appendChild(track->get_state(doc));
	}

	songNode.appendChild(tracksNode);
	return songNode;
}

void Song::connect_to_audiodevice( )
{
	PENTER;
	
        audiodeviceClient = audiodevice().new_client("song_" + QByteArray::number(get_id()));
        audiodeviceClient->set_process_callback( MakeDelegate(this, &Song::process) );
	
	audiodevice().process_client_request();
}

void Song::disconnect_from_audiodevice_and_delete()
{
	PENTER;
	
	if (transport) {
		stopTransport = true;
	}
	
	PMESG("Song : Scheduling for deletion !!");
	scheduleForDeletion = true;
	
	audiodeviceClient->delete_client();
	
	audiodevice().process_client_request();
}

void Song::audiodevice_client_request_processed( )
{
	PENTER;
	
	PMESG("Song :: Thread id:  %ld", QThread::currentThreadId ());
	if (scheduleForDeletion) {
		PMESG("Song : deleting myself!!!!!");
		delete this;
	}
}

void Song::add_track( Track* track, int id)
{
	m_tracks.insert(id, track);
}

bool Song::any_track_armed()
{
	foreach(Track* track, m_tracks) {
		if (track->armed()) {
			return true;
		}
	}
	return false;
}

int Song::get_clips_count_for_audio(AudioSource* )
{
	int count=0;
	
	return count;
}

int Song::get_floorY()
{
	if (m_tracks.size() > 0) {
		int ct = m_tracks.size()-1;
		foreach(Track* track, m_tracks) {
			if (track->get_id() == ct) {
				int by = track->get_baseY();
				int h = track->get_height();
				int fy = by+h;
				return fy;
			}
		}
	}
	return 0;
}

int Song::process_go(int )
{
	return 1;
}

int Song::remove_all_clips_for_audio(AudioSource* )
{
	PENTER;
	int counter=0;
	
	return counter;
}

int Song::prepare_export(ExportSpecification* spec)
{
	PENTER;

	if (transport) {
		stopTransport = true;
	}

	rendering = true;
	transport_frame = 0;

	spec->start_frame = 0;
	spec->end_frame = get_last_block();
	spec->total_frames = spec->end_frame - spec->start_frame;
	spec->pos = 0;
	spec->progress = 0;

	spec->blocksize = audiodevice().get_buffer_size();
	QString idString = QString::number(m_id);
	if (m_id < 10)
		idString.prepend("0");
	spec->name =  idString +" - " + title + spec->extension;

	if (spec->start_frame >= spec->end_frame) {
		PWARN("illegal frame range in export specification");
		return -1;
	}

	if (spec->channels == 0) {
		PWARN("Illegal channel count (0) in export specification");
		return -1;
	}

	exportSource = new WriteSource(spec);

	return 1;
}

int Song::finish_audio_export()
{
	exportSource->finish_export();
	delete exportSource;
	return 0;
}

int Song::render(ExportSpecification* spec)
{
	uint32_t chn;
	uint32_t x;
	int ret = -1;
	int progress;
	nframes_t this_nframes;
	nframes_t nframes = spec->blocksize;

	if (!spec->running || spec->stop || (this_nframes = std::min ((spec->end_frame - spec->pos), nframes)) == 0) {
		process (nframes);
		/*		PWARN("Finished Rendering for this song");
				PWARN("running is %d", spec->running);
				PWARN("stop is %d", spec->stop);
				PWARN("this_nframes is %d", this_nframes);*/
		return finish_audio_export();
	}

	/* do the usual stuff */

	process_export(nframes);

	/* and now export the results */

	nframes = this_nframes;

	memset (spec->dataF, 0, sizeof (spec->dataF[0]) * nframes * spec->channels);

	/* foreach output channel ... */

	float* buf;

	for (chn = 0; chn < spec->channels; ++chn) {
		buf = masterOut->get_buffer(chn, nframes);

		if (!buf) {
			// Seem we are exporting at least to Stereo from an AudioBus with only one channel...
			// Use the first channel..
			buf = masterOut->get_buffer(0, nframes);
		}

		for (x = 0; x < nframes; ++x) {
			spec->dataF[chn+(x*spec->channels)] = buf[x];
		}
	}


	if (exportSource->process (nframes)) {
		goto out;
	}

	spec->pos += nframes;

	progress = ( 100 * (spec->pos) ) / spec->total_frames;
	if (progress > spec->progress) {
		spec->progress = progress;
		m_project->set_song_export_progress(progress);
	}


	/* and we're good to go */

	ret = 1;

out:
	if (!ret) {
		spec->running = false;
		spec->status = ret;
		rendering = false;
	}

	return ret;
}

int Song::snapped_x(int x)
{
	int nx = x;
	if (isSnapOn) {
		int wx = block_to_xpos(workingFrame);
		// TODO check if it is close to upper or lower nearest clip's edges
		if (nx<10) // check if it is close to track begin
			nx=0;
		if (abs(nx-wx)<10) // first try to snap to working block
		{
			PMESG("snapping !");
			nx=wx;
		}
	}
	return nx;
}

nframes_t Song::xpos_to_block(int xpos)
{
	return (firstFrame + xpos * Peak::zoomStep[m_hzoom]);
}

Track* Song::get_track_under_y(int y)
{
	foreach(Track* track, m_tracks) {
		if ( (y >= track->real_baseY() ) && ( y <= track->real_baseY() + track->get_height()) )
			return track;
	}
	return (Track*) 0;
}

Track* Song::get_track(int trackNumber)
{
	return m_tracks.value(trackNumber);
}

void Song::set_artists(QString pArtists)
{
	artists = pArtists;
}

void Song::set_master_gain(float pMasterGain)
{
	masterGain = pMasterGain;
}

void Song::set_title(QString sTitle)
{
	title=sTitle;
}

void Song::set_active_track(int trackNumber)
{
	if (!m_tracks.value(trackNumber))
		return;
	
	m_tracks.value(activeTrackNumber)->deactivate();
	activeTrackNumber=trackNumber;
	m_tracks.value(activeTrackNumber)->activate();
}

int Song::block_to_xpos(nframes_t block)
{
	float pos = (float) block;
	float start = (float)firstFrame;
	return (int) ((pos - start)  / Peak::zoomStep[m_hzoom]);
}

void Song::set_first_block(nframes_t pos)
{
	PENTER;
	firstFrame = pos;
	emit firstBlockChanged();
}

void Song::set_work_at(nframes_t pos)
{
	printf("entering set_work_at\n");
	newTransportFramePos = pos;
	workingFrame = pos;

	// If there is no transport, start_seek() will _not_ be
	// called from within process(). So we do it now!
	if (!transport)
		start_seek();

	seeking = true;
}

void Song::start_seek()
{
	printf("Song :: entering start_seek\n");
	printf("Song :: thread id is: %ld\n", QThread::currentThreadId ());
	printf("Song::start_seek()\n");
	if (transport) {
		transport = false;
		realtimepath = false;
		resumeTransport = true;
	}

	diskio->prepare_for_seek();

	emit seekStart(newTransportFramePos);
	printf("Song :: leaving start_seek\n\n");
}

void Song::seek_finished()
{
	printf("Song :: entering seek_finished\n");
	transport_frame = newTransportFramePos;
	seeking = false;

	if (resumeTransport) {
		transport = true;
		realtimepath = true;
		resumeTransport = false;
	}

	// 	emit workingPosChanged(block_to_xpos(transport_frame));
	printf("Song :: leaving seek_finished\n\n");
}

Command* Song::toggle_snap()
{
	isSnapOn=!isSnapOn;
	
	emit snapChanged();
	
	return (Command*) 0;
}

/******************************** SLOTS *****************************/

Command* Song::create_track()
{
	int trackBaseY = LocatorView::LOCATOR_HEIGHT;
	foreach(Track* track, m_tracks)
	trackBaseY += track->get_height();

	int trackNumber = m_tracks.size() + 1;
	Track* track = new Track(this, trackNumber, "Unnamed", trackBaseY, Track::INITIAL_HEIGHT);
	add_track( track, trackNumber);
	return (Command*) 0;
}

Command* Song::create_region()
{
	if (ie().is_holding()) {
		origBlockL = xpos_to_block(cpointer().clip_area_x());
		origBlockR = 0;
		// 		ie().set_jogging(true);
	} else {
		origBlockR = xpos_to_block(cpointer().clip_area_x());
		MtaRegion* m = new MtaRegion(origBlockL,origBlockR);
		regionList->add_region(m);
	}
	return (Command*) 0;
}

Command* Song::create_region_start()
{
	// int xpos ...
	//TODO
	return (Command*) 0;
}

Command* Song::create_region_end()
{
	// int xpos ...
	//TODO
	return (Command*) 0;
}

Command* Song::delete_region_under_x()
{
	// int xpos...
	return (Command*) 0;
}

Command* Song::delete_track()
{
	Track* track = m_tracks.take(activeTrackNumber);

	if (!track)
		return (Command*) -1;


	if (track->get_cliplist().count()) {
		QString s1;
		s1.setNum(track->get_total_clips());
		QString mesg = "There are "+s1+" clips on this tracks\nAre you sure you want to delete it ?";
		if ( QMessageBox::warning( 0, "Delete Track", mesg,"&YES", "&CANCEL", 0 , 0, 1 ) != 0)
			return (Command*) -1;
	}

	delete track;

	int numberTracks = m_tracks.size();
	int newActiveTrackNumber = activeTrackNumber - 1;
	if (numberTracks > 0 && (newActiveTrackNumber <= numberTracks))
		activeTrackNumber = newActiveTrackNumber;
	else if (numberTracks > 0)
		activeTrackNumber = 1;
	else
		activeTrackNumber = -1;

	return (Command*) 0;
}

Command* Song::go()
{
// 	printf("Song-%d::go transport is %d\n", m_id, transport);
	
	if (transport) {
		stopTransport = true;
	} else {
		emit transferStarted();
		transport = true;
// 		printf("transport is %d\n", transport);
		realtimepath = true;
	}
	return (Command*)0;
}

Command* Song::go_loop_regions()
{
	int r=0;
	if (this->process_go(10)) {
		//TODO
		return (Command*) 0;
	}
	r = process_go(r);
	return (Command*) 0;
}

Command* Song::go_regions()
{
	/*	int r=0;
		if (process_go(10))
			{
			if (regionList)
				{
				MtaRegion* firstRegion = regionList->head();
				r = mixer->go(firstRegion);
				info().information(tr("GOING THRU REGIONS ..."));
				PMESG("GOING (REGIONS) FROM %d ...", (int) firstRegion->beginBlock );
				}
			else
				{
				info().information(tr("No region to play !"));
				}
			}
		r = process_go(r);*/
	return (Command*) 0;
}

Command* Song::in_crop()
{
	// 	return new Crop(this);
	return (Command*) 0;
}

Command* Song::jog_create_region()
{
	return (Command*) 0;
}

Command* Song::set_editing_mode()
{
	info().information(tr("CURRENT MODE : EDITING"));
	foreach(Track* track, m_tracks)
		track->set_blur(false);
	foreach(Track* track, m_tracks)
		track->audioPluginChain->deactivate();
	return (Command*) 0;
}

Command* Song::set_curve_mode()
{
	info().information(tr("CURRENT MODE : TRACK CURVES"));
	foreach(Track* track, m_tracks)
		track->set_blur(true);
	m_tracks.value(activeTrackNumber)->audioPluginChain->activate();
	return (Command*) 0;
}

void Song::solo_track(Track* t)
{
	bool wasSolo = t->is_solo();
	
	foreach(Track* track, m_tracks) {
		track->set_solo(false);
		track->set_muted_by_solo(false);
	}
	
	if (!wasSolo) {
		foreach(Track* track, m_tracks) {
			track->set_muted_by_solo(true);
		}
		t->set_solo(true);
	}
	
	
}

Command* Song::work_next_edge()
{
	nframes_t w = acmanager->get_last_frame();
	
	foreach(Track* track, m_tracks) {
		AudioClip* c=track->get_clip_after(workingFrame);
		if ((c) && (c->get_track_first_block()<w))
			w=c->get_track_first_block();
	}
	
	if ( w != acmanager->get_last_frame() )
		set_work_at(w);
	
	return (Command*) 0;
}

Command* Song::work_previous_edge()
{
	nframes_t w = 0;
	foreach(Track* track, m_tracks) {
		AudioClip* c = track->get_clip_before(workingFrame);
		if ((c) && (c->get_track_first_block() >= w))
			w=c->get_track_first_block();
	}
	set_work_at(w);
	return (Command*) 0;
}

Command* Song::undo()
{
	Command* a = m_hs->undo();
	if (a) {
		a->undo_action();
	}
	return (Command*) 0;
}

Command* Song::redo()
{
	Command* a = m_hs->redo();
	if (a) {
		a->do_action();
	}
	return (Command*) 0;
}

void Song::set_hzoom( int hzoom )
{
	if (hzoom > (Peak::ZOOM_LEVELS - 1))
		hzoom = (Peak::ZOOM_LEVELS - 1);
	if (hzoom  < 0)
		hzoom = 0;
	m_hzoom = hzoom;
	emit hzoomChanged();
}

int Song::process( nframes_t nframes )
{
	// If no need for playback/record, return.
// 	printf("Song-%d::process transport is %d\n", m_id, transport);
	if (!transport)
		return 0;

	if (stopTransport) {
		emit transferStopped();
		transport = false;
		realtimepath = false;
		stopTransport = false;
		
		if (scheduleForDeletion) {
			audiodeviceClient->delete_client();
		}
		return 0;
	}

	// zero the masterOut buffers
	masterOut->silence_buffers(nframes);

	int processResult = 0;
	// Process all Tracks.
	foreach(Track* track, m_tracks) {
		processResult |= track->process(nframes);
	}

	// update the transport_frame
	transport_frame += nframes;

	if (!processResult) {
		return 0;
	}

	// Mix the result into the AudioDevice "physical" buffers
	if (playBackBus) {
		Mixer::mix_buffers_no_gain(playBackBus->get_buffer(0, nframes), masterOut->get_buffer(0, nframes), nframes);
		Mixer::mix_buffers_no_gain(playBackBus->get_buffer(1, nframes), masterOut->get_buffer(1, nframes), nframes);
		playBackBus->monitor_peaks();
	}

	if (seeking) {
		start_seek();
	}

	return 1;
}

int Song::process_export( nframes_t nframes )
{
	// Get the masterout buffers, and fill with zero's
	masterOut->silence_buffers(nframes);

	// Process all Tracks.
	foreach(Track* track, m_tracks) {
		track->process(nframes);
	}

	// update the transport_frame
	transport_frame += nframes;

	return 1;
}

void Song::resize_buffer( )
{
	delete [] mixdown;
	mixdown = new audio_sample_t[audiodevice().get_buffer_size()];
}

int Song::get_bitdepth( )
{
	return m_project->get_bitdepth();
}

int Song::get_rate( )
{
	return m_project->get_rate();
}

nframes_t Song::get_firstblock( ) const
{
	return firstFrame;
}

void Song::update_cursor_pos()
{
	if (!transport)
		emit cursorPosChanged();
}

QHash< int, Track * > Song::get_tracks( ) const
{
	return m_tracks;
}

DiskIO * Song::get_diskio( )
{
	return diskio;
}

AudioClipManager * Song::get_audioclip_manager( )
{
	return acmanager;
}

void Song::handle_diskio_outofsync( )
{
	if (transport) {
		PWARN("Out of sync signal received!");
		// Stop transport, and resync diskio, we can handle this just fine
		// with a seek request! set_work_at() starts the seek routine...
		// 		transport = false;
		set_work_at(transport_frame);
	} else {
		PWARN("diskio out of sync received, but no transport????");
	}
}

void Song::audiodevice_started( )
{
	playBackBus = audiodevice().get_playback_bus("Playback 1");
}

nframes_t Song::get_last_block( ) const
{
	return acmanager->get_last_frame();
}


// Command* Song::node_setup()
// {
// 	m_tracks.value(activeTrackNumber)->audioPluginChain->node_setup();
// 	return (Command*) 0;
// }
// 
// Command* Song::remove_current_audio_plugin_controller()
// {
// 	m_tracks.value(activeTrackNumber)->audioPluginChain->remove_controller();
// 	return (Command*) 0;
// }
// 
// Command* Song::remove_audio_plugin_controller()
// {
// 	m_tracks.value(activeTrackNumber)->audioPluginChain->select_controller(ie().is_holding());
// 	if (!ie().is_holding())  //FIXME Do you really want to have this behaviour?????? beginHold first selects the controller, second beginHold removes it!!!!!
// 		m_tracks.value(activeTrackNumber)->audioPluginChain->remove_controller();
// 	return (Command*) 0;
// }
// 
// Command* Song::add_audio_plugin_controller()
// {
// 	if (ie().is_holding()) {
// 		// 		ie().set_jogging(true);
// 		audioPluginSelector->start();
// 	} else {
// 		audioPluginSelector->stop();
// 		QString filterType = audioPluginSelector->get_selected_filter_type();
// 		m_tracks.value(activeTrackNumber)->audioPluginChain->add_audio_plugin_controller(filterType);
// 	}
// 	return (Command*) 0;
// }
// 
// Command* Song::add_node()
// {
// 	m_tracks.value(activeTrackNumber)->audioPluginChain->add_node();
// 	return (Command*) 0;
// }
// 
// Command* Song::drag_and_drop_node()
// {
// 	m_tracks.value(activeTrackNumber)->audioPluginChain->drag_node(ie().is_holding());
// 	return (Command*) 0;
// }
// 
// Command* Song::audio_plugin_setup()
// {
// 	m_tracks.value(activeTrackNumber)->audioPluginChain->audio_plugin_setup();
// 	return (Command*) 0;
// }
// 
// Command* Song::select_audio_plugin_controller()
// {
// 	if (ie().is_holding())
// 		// 		ie().set_jogging(true);
// 		m_tracks.value(activeTrackNumber)->audioPluginChain->select_controller(ie().is_holding());
// 	return (Command*) 0;
// }

// eof
