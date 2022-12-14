/*
Copyright (C) 2007 Ben Levitt

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

#include "AbstractAudioReader.h"
#include "SFAudioReader.h"
#include "FlacAudioReader.h"
#if defined MP3_DECODE_SUPPORT
#include "MadAudioReader.h"
#endif
#include "WPAudioReader.h"
#include "VorbisAudioReader.h"
#include "ResampleAudioReader.h"
#include "Utils.h"

#include <QString>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


AbstractAudioReader::AbstractAudioReader(const QString& filename)
{
    m_fileName = filename;
    m_readPos = m_channels = m_nframes = 0;
    m_rate = 0;
    m_length = TimeRef();
}


AbstractAudioReader::~AbstractAudioReader()
= default;


// Read cnt frames starting at start from the AudioReader, into dst
// uses seek() and read() from AudioReader subclass
nframes_t AbstractAudioReader::read_from(DecodeBuffer* buffer, nframes_t start, nframes_t count)
{
// 	printf("read_from:: before_seek from %d, framepos is %d\n", start, m_readPos);

    if (!seek(start)) {
        return 0;
    }

    return read(buffer, count);
}


uint AbstractAudioReader::get_num_channels()
{
    return m_channels;
}


uint AbstractAudioReader::get_file_rate()
{
    return m_rate;
}


bool AbstractAudioReader::eof()
{
    return (m_readPos >= m_nframes);
}


nframes_t AbstractAudioReader::pos()
{
    return m_readPos;
}


bool AbstractAudioReader::seek(nframes_t start)
{
    PENTER;
    if (m_readPos != start) {
        if (!seek_private(start)) {
            return false;
        }
        m_readPos = start;
    }

    return true;
}


nframes_t AbstractAudioReader::read(DecodeBuffer* buffer, nframes_t count)
{
    if (count && m_readPos < m_nframes) {

        // Make sure the read buffer is big enough for this read
        buffer->check_buffers_capacity(count, m_channels);

        // printf("read_from:: after_seek from %d, framepos is %d\n", start, m_readPos);
        nframes_t framesRead = read_private(buffer, count);

        m_readPos += framesRead;

        return framesRead;
    }

    return 0;
}


// Static method used by other classes to get an AudioReader for the correct file type
AbstractAudioReader* AbstractAudioReader::create_audio_reader(const QString& filename, const QString& decoder)
{
    AbstractAudioReader* newReader = nullptr;

    if ( ! (decoder.isEmpty() || decoder.isNull()) ) {
        if (decoder == "sndfile") {
            newReader = new SFAudioReader(filename);
        } else if (decoder == "wavpack") {
            newReader = new WPAudioReader(filename);
        } else if (decoder == "flac") {
            newReader = new FlacAudioReader(filename);
        } else if (decoder == "vorbis") {
            newReader = new VorbisAudioReader(filename);
        }
#if defined MP3_DECODE_SUPPORT
        else if (decoder == "mad") {
            auto madAudioReader = new MadAudioReader(filename);
            madAudioReader->init();
            newReader = madAudioReader;
        }
#endif

        if (newReader && !newReader->is_valid()) {
//            PERROR("new %s reader is invalid! (channels: %d, frames: %d)", QS_C(newReader->decoder_type()), newReader->get_num_channels(), newReader->get_nframes());
            delete newReader;
            newReader = nullptr;
        }
    }

    if (!newReader) {

                if (FlacAudioReader::can_decode(filename)) {
            newReader = new FlacAudioReader(filename);
        }
        else if (VorbisAudioReader::can_decode(filename)) {
            newReader = new VorbisAudioReader(filename);
        }
        else if (WPAudioReader::can_decode(filename)) {
            newReader = new WPAudioReader(filename);
        }
                else if (SFAudioReader::can_decode(filename)) {
                        newReader = new SFAudioReader(filename);
                }
#if defined MP3_DECODE_SUPPORT
        else if (MadAudioReader::can_decode(filename)) {
                    auto madAudioReader = new MadAudioReader(filename);
                    madAudioReader->init();
                    newReader = madAudioReader;
        }
#endif
    }

    if (newReader && !newReader->is_valid()) {
//        PERROR("new %s reader is invalid! (channels: %d, frames: %d)", QS_C(newReader->decoder_type()), newReader->get_num_channels(), newReader->get_nframes());
        delete newReader;
        newReader = nullptr;
    }

    return newReader;
}

DecodeBuffer::DecodeBuffer()
{
    destination = nullptr;
    readBuffer = nullptr;
    m_channels = destinationBufferSize = readBufferSize = 0;
    m_bufferSizeCheckCounter = m_totalCheckSize = m_smallerReadCounter = 0;
}


void DecodeBuffer::check_buffers_capacity(uint size, uint channels)
{
/*	m_bufferSizeCheckCounter++;
    m_totalCheckSize += size;

    float meanvalue = (m_totalCheckSize / (float)m_bufferSizeCheckCounter);

    if (meanvalue < destinationBufferSize && ((meanvalue + 256) < destinationBufferSize) && !(destinationBufferSize == size)) {
        m_smallerReadCounter++;
        if (m_smallerReadCounter > 8) {
            delete_destination_buffers();
            delete_readbuffer();
            m_bufferSizeCheckCounter = m_smallerReadCounter = 0;
            m_totalCheckSize = 0;
        }
    }*/


    if (destinationBufferSize < size || m_channels < channels) {

        delete_destination_buffers();

        m_channels = channels;

        destination = new audio_sample_t*[m_channels];

        for (uint chan = 0; chan < m_channels; chan++) {
            destination[chan] = new audio_sample_t[size];
        }

        destinationBufferSize = size;
// 		printf("resizing destination to %.3f KB\n", (float)size*4/1024);
    }

    if (readBufferSize < (size*m_channels)) {

        delete_readbuffer();

        readBuffer = new audio_sample_t[size*m_channels];
        readBufferSize = (size*m_channels);
    }
}

void DecodeBuffer::delete_destination_buffers()
{
    if (destination) {
        for (uint chan = 0; chan < m_channels; chan++) {
            delete [] destination[chan];
        }

        delete [] destination;

        destination = nullptr;
        destinationBufferSize = 0;
        m_channels = 0;
    }
}

void DecodeBuffer::delete_readbuffer()
{
    if (readBuffer) {

        delete [] readBuffer;

        readBuffer = nullptr;
        readBufferSize = 0;
    }
}
