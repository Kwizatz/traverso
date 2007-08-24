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

#include "WPAudioWriter.h"

#include <QString>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


WPAudioWriter::WPAudioWriter()
 : AbstractAudioWriter()
{
	m_wp = 0;
	m_firstBlock = 0;
	m_firstBlockSize = 0;
	m_tmp_buffer = 0;
	m_tmpBufferSize = 0;
	m_configFlags = 0;
}


WPAudioWriter::~WPAudioWriter()
{
	if (m_wp) {
		close_private();
	}
	if (m_firstBlock) {
		delete [] m_firstBlock;
	}
}

const char* WPAudioWriter::get_extension()
{
	return ".wv";
}


bool WPAudioWriter::set_format_attribute(const QString& key, const QString& value)
{
	if (key == "quality") {
		// Clear quality before or-ing in the new quality value
		m_configFlags &= ~(CONFIG_FAST_FLAG | CONFIG_HIGH_FLAG | CONFIG_VERY_HIGH_FLAG);
		
		if (value == "fast") {
			m_configFlags |= CONFIG_FAST_FLAG;
			return true;
		}
		else if (value == "high") {
			// CONFIG_HIGH_FLAG (default) ~ 1.5 times slower then FAST, ~ 20% extra compression then FAST
			m_configFlags |= CONFIG_HIGH_FLAG;
			return true;
		}
		else if (value == "very_high") {
			// CONFIG_VERY_HIGH_FLAG ~ 2 times slower then FAST, ~ 25 % extra compression then FAST
			m_configFlags |= CONFIG_HIGH_FLAG;
			m_configFlags |= CONFIG_VERY_HIGH_FLAG;
			return true;
		}
	}
	
	if (key == "skip_wvx") {
		if (value == "true") {
			// This option reduces the storage of some floating-point data files by up to about 10% by eliminating some 
			// information that has virtually no effect on the audio data. While this does technically make the compression 
			// lossy, it retains all the advantages of floating point data (>600 dB of dynamic range, no clipping, and 25 bits 
			// of resolution). This also affects large integer compression by limiting the resolution to 24 bits.
			m_configFlags |= CONFIG_SKIP_WVX;
			return true;
		}
		else if (value == "false") {
			m_configFlags &= ~CONFIG_SKIP_WVX;
			return true;
		}
	}
	
	return false;
}


bool WPAudioWriter::open_private()
{
	m_file = fopen(m_fileName.toUtf8().data(), "wb");
	if (!m_file) {
		PERROR("Couldn't open file %s.", m_fileName.toUtf8().data());
		return false;
	}
	
	m_wp = WavpackOpenFileOutput(WPAudioWriter::write_block, (void *)this, NULL);
	if (!m_wp) {
		fclose(m_file);
		return false;
	}

	memset (&m_config, 0, sizeof(m_config));
	m_config.bytes_per_sample = (m_sampleWidth == 1) ? 4 : m_sampleWidth/8;
	m_config.bits_per_sample = (m_sampleWidth == 1) ? 32 : m_sampleWidth;
	if (m_sampleWidth == 1) m_config.float_norm_exp = 127; // config->float_norm_exp,  select floating-point data (127 for +/-1.0)
	m_config.channel_mask = (m_channels == 2) ? 3 : 4; // Microsoft standard (mono = 4, stereo = 3)
	m_config.num_channels = m_channels;
	m_config.sample_rate = m_rate;
	m_config.flags = m_configFlags;
	
	WavpackSetConfiguration(m_wp, &m_config, -1);
	
	if (!WavpackPackInit(m_wp)) {
		fclose(m_file);
		WavpackCloseFile(m_wp);
		m_wp = 0;
		return false;
	}
	
	m_firstBlock = 0;
	m_firstBlockSize = 0;
	
	return true;
}


int WPAudioWriter::write_to_file(void *lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t *lpNumberOfBytesWritten)
{
	uint32_t bcount;
	
	*lpNumberOfBytesWritten = 0;
	
	while (nNumberOfBytesToWrite) {
		bcount = fwrite((uchar *) lpBuffer + *lpNumberOfBytesWritten, 1, nNumberOfBytesToWrite, m_file);
	
		if (bcount) {
			*lpNumberOfBytesWritten += bcount;
			nNumberOfBytesToWrite -= bcount;
		}
		else {
			break;
		}
	}
	int err = ferror(m_file);
	return !err;
}


int WPAudioWriter::write_block(void *id, void *data, int32_t length)
{
	WPAudioWriter* writer = (WPAudioWriter*) id;
	uint32_t bcount;
	
	if (writer && writer->m_file && data && length) {
		if (writer->m_firstBlock == 0) {
			writer->m_firstBlock = new char[length];
			memcpy(writer->m_firstBlock, data, length);
			writer->m_firstBlockSize = length;
		}
		if (!writer->write_to_file(data, (uint32_t)length, (uint32_t*)&bcount) || bcount != (uint32_t)length) {
			fclose(writer->m_file);
			writer->m_wp = 0;
			return false;
		}
	}

	return true;
}


bool WPAudioWriter::rewrite_first_block()
{
	if (!m_firstBlock || !m_file || !m_wp) {
		return false;
	}
	WavpackUpdateNumSamples (m_wp, m_firstBlock);
	if (fseek(m_file, 0, SEEK_SET) != 0) {
		return false;
	}
	if (!write_block(this, m_firstBlock, m_firstBlockSize)) {
		return false;
	}
	
	return true;
}


nframes_t WPAudioWriter::write_private(void* buffer, nframes_t frameCount)
{
	// FIXME:
	// Instead of this block, add an option to gdither to leave each
	// 8bit or 16bit sample in a 0-padded, int32_t
	// 
	if (m_sampleWidth > 1 && m_sampleWidth < 24) { // Not float, or 32bit int, or 24bit int
		if (frameCount > m_tmpBufferSize) {
			if (m_tmp_buffer) {
				delete [] m_tmp_buffer;
			}
			m_tmp_buffer = new int32_t[frameCount * m_channels];
			m_tmpBufferSize = frameCount;
		}
		for (nframes_t s = 0; s < frameCount * m_channels; s++) {
			switch (m_sampleWidth) {
				case 8:
					m_tmp_buffer[s] = ((int8_t*)buffer)[s];
					break;
				case 16:
					m_tmp_buffer[s] = ((int16_t*)buffer)[s];
					break;
				default:
					// Less than 24 bit, but not 8 or 16 ?  This won't end well...
					break;
			}
		}
		WavpackPackSamples(m_wp, m_tmp_buffer, frameCount);
		return frameCount;
	}
	
	WavpackPackSamples(m_wp, (int32_t *)buffer, frameCount);
	return frameCount;
}


void WPAudioWriter::close_private()
{
	WavpackFlushSamples(m_wp);
	rewrite_first_block();
	WavpackCloseFile(m_wp);
	fclose(m_file);
	m_wp = 0;

	if (m_tmp_buffer) {
		delete [] m_tmp_buffer;
		m_tmp_buffer = 0;
	}
	m_tmpBufferSize = 0;

	if (m_firstBlock) {
		delete [] m_firstBlock;
		m_firstBlock = 0;
		m_firstBlockSize = 0;
	}
}

