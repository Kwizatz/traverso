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


#include "AudioClipEditDialog.h"

#include <QWidget>
#include "ui_AudioClipEditWidget.h"

#include "AudioClip.h"
#include "FadeCurve.h"
#include "ProjectManager.h"
#include "Project.h"
#include "Utils.h"
#include "defines.h"
#include "Mixer.h"
#include "Command.h"
#include "AudioClipExternalProcessing.h"
#include "InputEngine.h"

#define TIME_FORMAT "hh:mm:ss.zzz"

class AudioClipEditWidget : public QWidget, protected Ui::AudioClipEditWidget
{
	Q_OBJECT

public:
	AudioClipEditWidget(AudioClip* clip, QWidget* parent) : QWidget(parent), m_clip(clip)
	{
		setupUi(this);

		locked = false;

		clipStartEdit->setDisplayFormat(TIME_FORMAT);
		clipLengthEdit->setDisplayFormat(TIME_FORMAT);
		fadeInEdit->setDisplayFormat(TIME_FORMAT);
		fadeOutEdit->setDisplayFormat(TIME_FORMAT);

		fadeInModeBox->insertItem(1, "Bended");
		fadeInModeBox->insertItem(2, "S-Shape");
		fadeInModeBox->insertItem(3, "Long");

		fadeOutModeBox->insertItem(1, "Bended");
		fadeOutModeBox->insertItem(2, "S-Shape");
		fadeOutModeBox->insertItem(3, "Long");

		// Used to set gain and name
		clip_state_changed();
		
		// used for length, track start position
		clip_position_changed();
		
		// detect and set fade params
		fade_curve_added();
		
		connect(clip, SIGNAL(stateChanged()), this, SLOT(clip_state_changed()));
		connect(clip, SIGNAL(positionChanged(Snappable*)), this, SLOT(clip_position_changed()));
		connect(clip, SIGNAL(fadeAdded(FadeCurve*)), this, SLOT(fade_curve_added()));
		
		connect(clipGainSpinBox, SIGNAL(valueChanged(double)), this, SLOT(gain_spinbox_value_changed(double)));
		
		connect(clipStartEdit, SIGNAL(timeChanged(const QTime&)), this, SLOT(clip_start_edit_changed(const QTime&)));
		connect(clipLengthEdit, SIGNAL(timeChanged(const QTime&)), this, SLOT(clip_length_edit_changed(const QTime&)));
		
		connect(fadeInEdit, SIGNAL(timeChanged(const QTime&)), this, SLOT(fadein_edit_changed(const QTime&)));
		connect(fadeInModeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(fadein_mode_edit_changed(int)));
		connect(fadeInBendingBox, SIGNAL(valueChanged(double)), this, SLOT(fadein_bending_edit_changed(double)));
		connect(fadeInStrengthBox, SIGNAL(valueChanged(double)), this, SLOT(fadein_strength_edit_changed(double)));
		connect(fadeInLinearButton, SIGNAL(clicked()), this, SLOT(fadein_linear()));
		connect(fadeInDefaultButton, SIGNAL(clicked()), this, SLOT(fadein_default()));

		connect(fadeOutEdit, SIGNAL(timeChanged(const QTime&)), this, SLOT(fadeout_edit_changed(const QTime&)));
		connect(fadeOutModeBox, SIGNAL(currentIndexChanged(int)), this, SLOT(fadeout_mode_edit_changed(int)));
		connect(fadeOutBendingBox, SIGNAL(valueChanged(double)), this, SLOT(fadeout_bending_edit_changed(double)));
		connect(fadeOutStrengthBox, SIGNAL(valueChanged(double)), this, SLOT(fadeout_strength_edit_changed(double)));
		connect(fadeOutLinearButton, SIGNAL(clicked()), this, SLOT(fadeout_linear()));
		connect(fadeOutDefaultButton, SIGNAL(clicked()), this, SLOT(fadeout_default()));
		
		connect(externalProcessingButton, SIGNAL(clicked()), this, SLOT(external_processing()));
		connect(buttonBox, SIGNAL(accepted()), this, SLOT(save_changes()));
	}
	
	~AudioClipEditWidget() {}
	
private:
	AudioClip* m_clip;
	friend class AudioClipEditDialog;
	
	nframes_t qtime_to_nframes(const QTime& time, uint rate);
	QTime nframes_to_qtime(nframes_t nframes, uint rate);
	bool locked;
	
private slots:
	void external_processing();
	void clip_state_changed();
	void save_changes();
	void clip_position_changed();
	void gain_spinbox_value_changed(double value);

	void fadein_length_changed();
	void fadein_edit_changed(const QTime& time);
	void fadein_mode_changed();
	void fadein_mode_edit_changed(int index);
	void fadein_bending_changed();
	void fadein_bending_edit_changed(double value);
	void fadein_strength_changed();
	void fadein_strength_edit_changed(double value);
	void fadein_linear();
	void fadein_default();

	void fadeout_edit_changed(const QTime& time);
	void fadeout_length_changed();
	void fadeout_mode_changed();
	void fadeout_mode_edit_changed(int index);
	void fadeout_bending_changed();
	void fadeout_bending_edit_changed(double value);
	void fadeout_strength_changed();
	void fadeout_strength_edit_changed(double value);
	void fadeout_linear();
	void fadeout_default();

	void clip_start_edit_changed(const QTime& time);
	void clip_length_edit_changed(const QTime& time);
	void update_clip_end();
	
	void fade_curve_added();
};


AudioClipEditDialog::AudioClipEditDialog(AudioClip* clip, QWidget* parent)
	: QDialog(parent)
{
	m_edit = new AudioClipEditWidget(clip, this);
	
	QHBoxLayout* mainLayout = new QHBoxLayout(this);
	mainLayout->setMargin(0);
	mainLayout->addWidget(m_edit);
	
	setLayout(mainLayout);
	
	connect(m_edit->buttonBox, SIGNAL(rejected()), this, SLOT(close()));
}

void AudioClipEditWidget::external_processing()
{
	parentWidget()->hide();
	Command::process_command(new AudioClipExternalProcessing(m_clip));
}

void AudioClipEditWidget::clip_state_changed()
{
	if (m_clip->get_name() != clipNameLineEdit->text()) {
		parentWidget()->setWindowTitle(m_clip->get_name());
		clipNameLineEdit->setText(m_clip->get_name());
	}
	
	clipGainSpinBox->setValue(coefficient_to_dB(m_clip->get_gain()));
}

void AudioClipEditWidget::save_changes()
{
	parentWidget()->hide();
	QString name = clipNameLineEdit->text();
	if (!name.isEmpty()) {
		m_clip->set_name(name);
	} else {
		clipNameLineEdit->setText(m_clip->get_name());
	}		
}

void AudioClipEditWidget::gain_spinbox_value_changed(double value)
{
	float gain = dB_to_scale_factor(value);
	m_clip->set_gain(gain);
}

void AudioClipEditWidget::clip_position_changed()
{
	if (locked) return;

	QTime clipLengthTime = nframes_to_qtime(m_clip->get_length(), m_clip->get_rate());
	clipLengthEdit->setTime(clipLengthTime);
	
	QTime clipStartTime = nframes_to_qtime(m_clip->get_track_start_frame(), m_clip->get_rate());
	clipStartEdit->setTime(clipStartTime);

	update_clip_end();
}

void AudioClipEditWidget::fadein_length_changed()
{
	if (ie().is_holding()) return;
	if (locked) return;

	QTime fadeTime = nframes_to_qtime(m_clip->get_fade_in()->get_range(), m_clip->get_rate());
	fadeInEdit->setTime(fadeTime);
}

void AudioClipEditWidget::fadeout_length_changed()
{
	if (locked) return;

	QTime fadeTime = nframes_to_qtime(m_clip->get_fade_out()->get_range(), m_clip->get_rate());
	fadeOutEdit->setTime(fadeTime);
}

void AudioClipEditWidget::fadein_edit_changed(const QTime& time)
{
	// Hmm, we can't distinguish between hand editing the time edit
	// or moving the clip with the mouse! In the latter case this function
	// causes trouble when moving the right edge with the mouse! 
	// This 'fixes' it .....
	if (ie().is_holding()) return;

	locked = true;
	nframes_t frames = qtime_to_nframes(time, m_clip->get_rate());
	if (frames == 0) {
		m_clip->set_fade_in(1);
	} else {
		m_clip->set_fade_in(frames);
	}
	locked = false;
}

void AudioClipEditWidget::fadeout_edit_changed(const QTime& time)
{
	if (ie().is_holding()) return;

	locked = true;
	nframes_t frames = qtime_to_nframes(time, m_clip->get_rate());
	if (frames == 0) {
		m_clip->set_fade_out(1);
	} else {
		m_clip->set_fade_out(frames);
	}
	locked = false;
}

void AudioClipEditWidget::clip_length_edit_changed(const QTime& time)
{
	if (ie().is_holding()) return;

	locked = true;
	uint rate = m_clip->get_rate();
	uint frames = qtime_to_nframes(time, rate);

	if (frames >= m_clip->get_source_length()) {
		frames = m_clip->get_source_length();
		QTime clipLengthTime = nframes_to_qtime(frames, m_clip->get_rate());
		clipLengthEdit->setTime(clipLengthTime);
	}

	m_clip->set_right_edge(frames + m_clip->get_track_start_frame());
	update_clip_end();
	locked = false;
}

void AudioClipEditWidget::clip_start_edit_changed(const QTime& time)
{
	if (ie().is_holding()) return;

	locked = true;
	m_clip->set_track_start_frame(qtime_to_nframes(time, m_clip->get_rate()));
	update_clip_end();
	locked = false;
}

void AudioClipEditWidget::fadein_mode_changed()
{
	if (locked) return;

	int m = m_clip->get_fade_in()->get_mode();
	fadeInModeBox->setCurrentIndex(m);
}

void AudioClipEditWidget::fadeout_mode_changed()
{
	if (locked) return;

	int m = m_clip->get_fade_out()->get_mode();
	fadeOutModeBox->setCurrentIndex(m);
}

void AudioClipEditWidget::fadein_bending_changed()
{
	if (locked) return;
	fadeInBendingBox->setValue(m_clip->get_fade_in()->get_bend_factor());
}

void AudioClipEditWidget::fadeout_bending_changed()
{
	if (locked) return;
	fadeOutBendingBox->setValue(m_clip->get_fade_out()->get_bend_factor());
}

void AudioClipEditWidget::fadein_strength_changed()
{
	if (locked) return;
	fadeInStrengthBox->setValue(m_clip->get_fade_in()->get_strength_factor());
}

void AudioClipEditWidget::fadeout_strength_changed()
{
	if (locked) return;
	fadeOutStrengthBox->setValue(m_clip->get_fade_out()->get_strength_factor());
}

void AudioClipEditWidget::fadein_mode_edit_changed(int index)
{
	locked = true;
	m_clip->get_fade_in()->set_mode(index);
	locked = false;
}

void AudioClipEditWidget::fadeout_mode_edit_changed(int index)
{
	locked = true;
	m_clip->get_fade_out()->set_mode(index);
	locked = false;
}

void AudioClipEditWidget::fadein_bending_edit_changed(double value)
{
	locked = true;
	m_clip->get_fade_in()->set_bend_factor(value);
	locked = false;
}

void AudioClipEditWidget::fadeout_bending_edit_changed(double value)
{
	locked = true;
	m_clip->get_fade_out()->set_bend_factor(value);
	locked = false;
}

void AudioClipEditWidget::fadein_strength_edit_changed(double value)
{
	locked = true;
	m_clip->get_fade_in()->set_strength_factor(value);
	locked = false;
}

void AudioClipEditWidget::fadeout_strength_edit_changed(double value)
{
	locked = true;
	m_clip->get_fade_out()->set_strength_factor(value);
	locked = false;
}

void AudioClipEditWidget::fadein_linear()
{
	fadeInBendingBox->setValue(0.5);
	fadeInStrengthBox->setValue(0.5);
}

void AudioClipEditWidget::fadein_default()
{
	fadeInBendingBox->setValue(0.0);
	fadeInStrengthBox->setValue(0.5);
}

void AudioClipEditWidget::fadeout_linear()
{
	fadeOutBendingBox->setValue(0.5);
	fadeOutStrengthBox->setValue(0.5);
}

void AudioClipEditWidget::fadeout_default()
{
	fadeOutBendingBox->setValue(0.0);
	fadeOutStrengthBox->setValue(0.5);
}

nframes_t AudioClipEditWidget::qtime_to_nframes(const QTime & time, uint rate)
{
	return time.hour() * 3600 * rate + time.minute() * 60 * rate + time.second() * rate + (time.msec() * rate) / 1000;
}

QTime AudioClipEditWidget::nframes_to_qtime(nframes_t nframes, uint rate)
{
	long unsigned int remainder;
	int hours, mins, secs, msec;

	remainder = nframes;
	hours = remainder / (3600 * rate);
	remainder -= ( hours * 3600 * rate );
	mins = remainder / ( 60 * rate );
	remainder -= ( mins * 60 * rate );
	secs = remainder / rate;
	remainder -= secs * rate;
	msec = remainder * 1000 / rate;

	QTime time(hours, mins, secs, msec);
	return time;
}

void AudioClipEditWidget::fade_curve_added()
{
	if (m_clip->get_fade_in()) {
		fadein_length_changed();
		fadein_mode_changed();
		fadein_bending_changed();
		fadein_strength_changed();
		connect(m_clip->get_fade_in(), SIGNAL(rangeChanged()), this, SLOT(fadein_length_changed()));
		connect(m_clip->get_fade_in(), SIGNAL(modeChanged()), this, SLOT(fadein_mode_changed()));
		connect(m_clip->get_fade_in(), SIGNAL(bendValueChanged()), this, SLOT(fadein_bending_changed()));
		connect(m_clip->get_fade_in(), SIGNAL(strengthValueChanged()), this, SLOT(fadein_strength_changed()));
	}
	if (m_clip->get_fade_out()) {
		fadeout_length_changed();
		fadeout_mode_changed();
		fadeout_bending_changed();
		fadeout_strength_changed();
		connect(m_clip->get_fade_out(), SIGNAL(rangeChanged()), this, SLOT(fadeout_length_changed()));
		connect(m_clip->get_fade_out(), SIGNAL(modeChanged()), this, SLOT(fadeout_mode_changed()));
		connect(m_clip->get_fade_out(), SIGNAL(bendValueChanged()), this, SLOT(fadeout_bending_changed()));
		connect(m_clip->get_fade_out(), SIGNAL(strengthValueChanged()), this, SLOT(fadeout_strength_changed()));
	}
}

void AudioClipEditWidget::update_clip_end()
{
	nframes_t clipEndFrame = m_clip->get_track_start_frame() + m_clip->get_length();
	QTime clipEndTime = nframes_to_qtime(clipEndFrame, m_clip->get_rate());
	clipEndLineEdit->setText(clipEndTime.toString(TIME_FORMAT));
}

#include "AudioClipEditDialog.moc"