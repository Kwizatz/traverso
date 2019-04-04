/*
Copyright (C) 2007 Remon Sijrier 

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

#include "AudioFileMerger.h"
#include <QFile>
#include <QMutexLocker>

#include "Export.h"
#include "AbstractAudioReader.h"
#include "ReadSource.h"
#include "WriteSource.h"
#include "Peak.h"
#include "defines.h"

AudioFileMerger::AudioFileMerger()
{
	m_stopMerging = false;
	moveToThread(this);
	start();
	connect(this, SIGNAL(dequeueTask()), this, SLOT(dequeue_tasks()), Qt::QueuedConnection);
}

void AudioFileMerger::enqueue_task(ReadSource * source0, ReadSource * source1, const QString& dir, const QString & outfilename)
{
	MergeTask task;
	task.readsource0 = source0;
	task.readsource1 = source1;
	task.outFileName = outfilename;
	task.dir = dir;
	
	m_mutex.lock();
	m_tasks.enqueue(task);
	m_mutex.unlock();
	
	emit dequeueTask();
}

void AudioFileMerger::dequeue_tasks()
{
	m_mutex.lock();
	if (m_tasks.size()) {
		MergeTask task = m_tasks.dequeue();
		m_mutex.unlock();
		process_task(task);
		return;
	}
	m_mutex.unlock();
}

void AudioFileMerger::process_task(MergeTask task)
{
	QString name = task.readsource0->get_name();
	int length = name.length();
	emit taskStarted(name.left(length-28));
	uint buffersize = 16384;
	DecodeBuffer decodebuffer0;
	DecodeBuffer decodebuffer1;
	
	ExportSpecification* spec = new ExportSpecification();
	spec->startLocation = TimeRef();
	spec->endLocation = task.readsource0->get_length();
	spec->totalTime = spec->endLocation;
	spec->pos = TimeRef();
	spec->isRecording = false;
	
	spec->exportdir = task.dir;
	spec->writerType = "sndfile";
	spec->extraFormat["filetype"] = "wav";
	spec->data_width = 1;	// 1 means float
	spec->channels = 2;
	spec->sample_rate = task.readsource0->get_rate();
	spec->blocksize = buffersize;
	spec->name = task.outFileName;
	spec->dataF = new audio_sample_t[buffersize * 2];
	
	WriteSource* writesource = new WriteSource(spec);
	if (writesource->prepare_export() == -1) {
		delete writesource;
        writesource = nullptr;
		delete [] spec->dataF;
        spec->dataF = nullptr;
		delete spec;
        spec = nullptr;
		return;
	}
	// Enable on the fly generation of peak data to speedup conversion 
	// (no need to re-read all the audio files to generate peaks)
	writesource->set_process_peaks(true);
	
	int oldprogress = 0;
	do {
		// if the user asked to stop processing, jump out of this 
		// loop, and cleanup any resources in use.
		if (m_stopMerging) {
			goto out;
		}
			
		nframes_t diff = (spec->endLocation - spec->pos).to_frame(task.readsource0->get_rate());
		nframes_t this_nframes = std::min(diff, buffersize);
		nframes_t nframes = this_nframes;
		
		memset (spec->dataF, 0, sizeof (spec->dataF[0]) * nframes * spec->channels);
		
		task.readsource0->file_read(&decodebuffer0, spec->pos, nframes);
		task.readsource1->file_read(&decodebuffer1, spec->pos, nframes);
			
		for (uint x = 0; x < nframes; ++x) {
			spec->dataF[x*spec->channels] = decodebuffer0.destination[0][x];
			spec->dataF[1+(x*spec->channels)] = decodebuffer1.destination[0][x];
		}
		
		// due the fact peak generating does _not_ happen in writesource->process
		// but in a function used by DiskIO, we have to hack the peak processing 
		// in here.
		writesource->get_peak()->process(0, decodebuffer0.destination[0], nframes);
		writesource->get_peak()->process(1, decodebuffer1.destination[0], nframes);
		
		// Process the data, and write to disk
		writesource->process(buffersize);
		
		spec->pos.add_frames(nframes, task.readsource0->get_rate());
		
		int currentprogress = int(double(spec->pos.universal_frame()) / double(spec->totalTime.universal_frame()) * 100);
		if (currentprogress > oldprogress) {
			oldprogress = currentprogress;
			emit progress(currentprogress);
		}
			
	} while (spec->pos != spec->totalTime);
		
	
	out:
	writesource->finish_export();
	delete writesource;
	delete [] spec->dataF;
	delete spec;
	
	//  The user asked to stop processing, exit the event loop
	// and signal we're done.
	if (m_stopMerging) {
		exit(0);
		wait(1000);
		m_tasks.clear();
		emit processingStopped();
		return;
	}
	
	emit taskFinished(name.left(length-28));
}

void AudioFileMerger::stop_merging()
{
	m_stopMerging = true;
}

