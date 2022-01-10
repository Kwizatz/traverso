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

#include "SFAudioWriter.h"
#include "Utils.h"

#include <QString>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


SFAudioWriter::SFAudioWriter()
 : AbstractAudioWriter()
{
	m_sf = 0;
}


SFAudioWriter::~SFAudioWriter()
{
	if (m_sf) {
        SFAudioWriter::close_private();
	}
}


const char* SFAudioWriter::get_extension()
{
	if (m_fileType == SF_FORMAT_WAV) {
		return ".wav";
	}
	else if (m_fileType == SF_FORMAT_W64) {
		return ".w64";
	}
	else if (m_fileType == SF_FORMAT_AIFF) {
		return ".aiff";
	}
	else if (m_fileType == SF_FORMAT_FLAC) {
		return ".flac";
	}
	return ".raw";
}


bool SFAudioWriter::set_format_attribute(const QString& key, const QString& value)
{
	if (key == "filetype") {
		if (value == "wav") {
			m_fileType = SF_FORMAT_WAV;
			return true;
		}
		else if (value == "w64") {
			m_fileType = SF_FORMAT_W64;
			return true;
		}
		else if (value == "aiff") {
			m_fileType = SF_FORMAT_AIFF;
			return true;
		}
		else if (value == "flac") {
			m_fileType = SF_FORMAT_FLAC;
			return true;
		}
	}
	
	return false;
}


bool SFAudioWriter::open_private()
{
	char errbuf[256];
	
	memset (&m_sfinfo, 0, sizeof(m_sfinfo));
	m_sfinfo.format = get_sf_format();
	m_sfinfo.frames = 48000*100;
	m_sfinfo.samplerate = m_rate;
	m_sfinfo.channels = m_channels;
	//m_sfinfo.frames = m_spec->endLocation - m_spec->startLocation + 1;
	
	m_file.setFileName(m_fileName);
	
	if (!m_file.open(QIODevice::WriteOnly)) {
		qWarning("SFAudioReader::open_private: Could not create file (%s)", QS_C(m_fileName));
		return false;
	}
	
	m_sf = sf_open_fd(m_file.handle(), SFM_WRITE, &m_sfinfo, false);
	
    if (m_sf == nullptr) {
        sf_error_str (nullptr, errbuf, sizeof (errbuf) - 1);
        PWARN(QString("Export: cannot open output file \"%1\" (%2)").arg(m_fileName, errbuf).toLatin1().data());
		return false;
	}
	
	return true;
}


nframes_t SFAudioWriter::write_private(void* buffer, nframes_t frameCount)
{
	int written = 0;
	char errbuf[256];
	
	switch (m_sampleWidth) {
		case 8:
			written = sf_write_raw (m_sf, (void*) buffer, frameCount * m_channels);
			break;

		case 16:
			written = sf_writef_short (m_sf, (short*) buffer, frameCount);
			break;

		case 24:
		case 32:
			written = sf_writef_int (m_sf, (int*) buffer, frameCount);
			break;

		default:
			written = sf_writef_float (m_sf, (float*) buffer, frameCount);
			break;
	}
	
	if ((nframes_t) written != frameCount) {
		sf_error_str (m_sf, errbuf, sizeof (errbuf) - 1);
//		PERROR("Export: could not write data to output file (%s)\n", errbuf);
		return -1;
	}
	
	return written;
}


bool SFAudioWriter::close_private()
{
	bool success = (sf_close(m_sf) == 0);
	
	m_sf = nullptr;
	
	return success;
}


int SFAudioWriter::get_sf_format()
{
	int sfBitDepth;
	
	switch (m_sampleWidth) {
		case 8:
			sfBitDepth = SF_FORMAT_PCM_S8;
			break;
		case 16:
			sfBitDepth = SF_FORMAT_PCM_16;
			break;
		case 24:
			sfBitDepth = SF_FORMAT_PCM_24;
			break;
		case 32:
			sfBitDepth = SF_FORMAT_PCM_32;
			break;
		default:
			sfBitDepth = SF_FORMAT_FLOAT;
			break;
	}
	
	return (sfBitDepth | m_fileType);
}

