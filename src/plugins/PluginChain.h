/*
Copyright (C) 2006-2019 Remon Sijrier

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


#ifndef PLUGIN_CHAIN_H
#define PLUGIN_CHAIN_H

#include <ContextItem.h>
#include <QList>
#include <QDomNode>
#include "Plugin.h"
#include "GainEnvelope.h"

class TSession;
class AudioBus;

class PluginChain : public ContextItem
{
    Q_OBJECT

public:
    PluginChain(ContextItem* parent, TSession* session=nullptr);
    ~PluginChain();

    QDomNode get_state(QDomDocument doc);
    int set_state(const QDomNode & node );

    TCommand* add_plugin(Plugin* plugin, bool historable=true);
    TCommand* remove_plugin(Plugin* plugin, bool historable=true);
    void process_pre_fader(AudioBus* bus, unsigned long nframes);
    int process_post_fader(AudioBus* bus, unsigned long nframes);
    // 	void process_fader(audio_sample_t* buffer, nframes_t pos, nframes_t nframes) {m_fader->process_gain(buffer, pos, nframes);}

    void set_session(TSession* session);

    QList<Plugin*>  get_plugins() const {return m_plugins;}
    QList<Plugin*>  get_pre_fader_plugins();
    QList<Plugin*>  get_post_fader_plugins();
    GainEnvelope*   get_fader() const {return m_fader;}

private:
    APILinkedList	m_rtPlugins;
    QList<Plugin*>  m_plugins;
    GainEnvelope*	m_fader;
    TSession*	m_session{};

private slots:
    void private_add_plugin(Plugin* plugin);
    void private_remove_plugin(Plugin* plugin);
    void private_plugin_added(Plugin* plugin);
    void private_plugin_removed(Plugin* plugin);

signals:
    void pluginAdded(Plugin* plugin);
    void pluginRemoved(Plugin* plugin);
    void privatePluginRemoved(Plugin*);
    void privatePluginAdded(Plugin*);
};

inline void PluginChain::process_pre_fader(AudioBus * bus, unsigned long nframes)
{
    apill_foreach(Plugin* plugin, Plugin*, m_rtPlugins) {
        if (plugin == m_fader) {
            return;
        }
        plugin->process(bus, nframes);
    }
}

inline int PluginChain::process_post_fader(AudioBus * bus, unsigned long nframes)
{
    if (!m_rtPlugins.size()) {
        return 0;
    }

    bool faderWasReached = false;

    apill_foreach(Plugin* plugin, Plugin*, m_rtPlugins) {
        if (faderWasReached) {
            plugin->process(bus, nframes);
        } else if (plugin == m_fader) {
            faderWasReached = true;
        }
    }

    return 1;
}

#endif

//eof
