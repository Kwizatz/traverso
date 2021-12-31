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

#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include "defines.h"

#include <QObject>

#include "RingBufferNPT.h"


class QString;

/// The base class for AudioSources like ReadSource and WriteSource
class AudioSource : public QObject
{
public :
	AudioSource();
	AudioSource(QString  dir, const QString& name);
        virtual ~AudioSource();
	
	void set_name(const QString& name);
	void set_dir(const QString& name);
	void set_original_bit_depth(uint bitDepth);
	void set_created_by_sheet(qint64 id);
	QString get_filename() const;
	QString get_dir() const;
	QString get_name() const;
	QString get_short_name() const;
        qint64 get_id() const {return m_id;}
	qint64 get_orig_sheet_id() const {return m_origSheetId;}
    uint get_rate() const;
        uint get_channel_count() const {return m_channelCount;}
    uint get_bit_depth() const;
	
protected:
	QList<RingBufferNPT<audio_sample_t>*> 	m_buffers;
	
    uint		m_bufferSize{};
    uint		m_chunkSize{};
	
    uint		m_channelCount;
    qint64		m_origSheetId{};
	QString 	m_dir;
	qint64		m_id{};
	QString 	m_name;
	QString		m_shortName;
    uint		m_origBitDepth{};
	QString		m_fileName;
    uint 		m_rate{};
	int		m_wasRecording;
};


#endif
