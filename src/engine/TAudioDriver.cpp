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

$Id: Driver.cpp,v 1.6 2007/03/19 11:18:57 r_sijrier Exp $
*/

#include "TAudioDriver.h"
#include "AudioDevice.h"
#include "AudioChannel.h"

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


TAudioDriver::TAudioDriver(AudioDevice* dev , uint rate, nframes_t bufferSize)
{
	device = dev;
	frame_rate = rate;
	frames_per_cycle = bufferSize;

        read = MakeDelegate(this, &TAudioDriver::_read);
        write = MakeDelegate(this, &TAudioDriver::_write);
        run_cycle = RunCycleCallback(this, &TAudioDriver::_run_cycle);
}

TAudioDriver::~ TAudioDriver( )
{
	PENTERDES;
        while( ! m_captureChannels.isEmpty())
                device->delete_channel(m_captureChannels.takeFirst());

        while( ! m_playbackChannels.isEmpty())
                device->delete_channel(m_playbackChannels.takeFirst());
}

int TAudioDriver::_run_cycle( )
{
	// * 1000, we want it in millisecond
	// / 2, 2 bytes (16 bit)
	device->transport_cycle_end (get_microseconds());

	device->mili_sleep(23);

	device->transport_cycle_start (get_microseconds());

	return device->run_cycle( frames_per_cycle, 0);
}

int TAudioDriver::_read( nframes_t  )
{
	return 1;
}

int TAudioDriver::_write( nframes_t nframes )
{
        foreach(AudioChannel* chan, m_playbackChannels) {
                chan->silence_buffer(nframes);
        }

        return 1;
}

int TAudioDriver::_null_cycle( nframes_t  )
{
	return 1;
}

int TAudioDriver::attach( )
{
        int port_flags;
        char buf[32];
        AudioChannel* chan;

        device->set_buffer_size (frames_per_cycle);
        device->set_sample_rate (frame_rate);

        port_flags = PortIsOutput|PortIsPhysical|PortIsTerminal;

        // Create 2 fake capture channels
        for (uint chn=0; chn<2; chn++) {
                snprintf (buf, sizeof(buf) - 1, "capture_%d", chn+1);

                chan = add_capture_channel(buf);
                chan->set_latency( frames_per_cycle + capture_frame_latency );
        }

        // Create 2 fake playback channels
        for (uint chn=0; chn<2; chn++) {
                snprintf (buf, sizeof(buf) - 1, "playback_%d", chn+1);

                chan = add_playback_channel(buf);
                chan->set_latency( frames_per_cycle + capture_frame_latency );
        }

        return 1;
}

AudioChannel* TAudioDriver::add_capture_channel(const QString& chanName)
{
        PENTER;
        AudioChannel* chan = audiodevice().create_channel(chanName, m_captureChannels.size(), ChannelIsInput);
        m_captureChannels.append(chan);
        return chan;
}

AudioChannel* TAudioDriver::add_playback_channel(const QString& chanName)
{
        PENTER;
        AudioChannel* chan = audiodevice().create_channel(chanName, m_playbackChannels.size(), ChannelIsOutput);
        m_playbackChannels.append(chan);
        return chan;
}


AudioChannel* TAudioDriver::get_capture_channel_by_name(const QString& name)
{
        foreach(AudioChannel* chan, m_captureChannels) {
                if (chan->get_name() == name) {
                        return chan;
                }
        }
        return nullptr;
}


AudioChannel* TAudioDriver::get_playback_channel_by_name(const QString& name)
{
        foreach(AudioChannel* chan, m_playbackChannels) {
                if (chan->get_name() == name) {
                        return chan;
                }
        }
        return nullptr;
}



int TAudioDriver::detach( )
{
	return 0;
}

int TAudioDriver::start( )
{
	return 1;
}

int TAudioDriver::stop( )
{
	return 1;
}

QString TAudioDriver::get_device_name( )
{
	return "Null Audio Device";
}

QString TAudioDriver::get_device_longname( )
{
	return "Null Audio Device";
}


//eof
