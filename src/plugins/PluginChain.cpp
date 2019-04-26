/*
Copyright (C) 2006 Remon Sijrier

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
 
#include "PluginChain.h"

#include "Plugin.h"
#include "PluginManager.h"
#include <Tsar.h>
#include "TInputEventDispatcher.h"
#include <Sheet.h>
#include <AddRemove.h>
#include "AudioBus.h"
#include "GainEnvelope.h"
#include "Curve.h"
#include "Mixer.h"

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

PluginChain::PluginChain(ContextItem * parent)
	: ContextItem(parent)
{
	m_fader = new GainEnvelope(nullptr);
}

PluginChain::PluginChain(ContextItem* parent, TSession* session)
	: ContextItem(parent)
{
        m_fader = new GainEnvelope(session);
        set_session(session);
}

PluginChain::~ PluginChain()
{
	PENTERDES;
	foreach(Plugin* plugin, m_pluginList) {
		delete plugin;
	}
	
	delete m_fader;
}


QDomNode PluginChain::get_state(QDomDocument doc)
{
	QDomNode pluginsNode = doc.createElement("Plugins");
	
	foreach(Plugin* plugin, m_pluginList) {
		pluginsNode.appendChild(plugin->get_state(doc));
	}
	
	pluginsNode.appendChild(m_fader->get_state(doc));
	
	return pluginsNode;
}

int PluginChain::set_state( const QDomNode & node )
{
	QDomNode pluginsNode = node.firstChildElement("Plugins");
	QDomNode pluginNode = pluginsNode.firstChild();
	
	while(!pluginNode.isNull()) {
		if (pluginNode.toElement().attribute( "type", "") == "GainEnvelope") {
			m_fader->set_state(pluginNode);
		} else {
			Plugin* plugin = PluginManager::instance()->get_plugin(pluginNode);
			if (!plugin) {
				pluginNode = pluginNode.nextSibling();
				continue;
			}
			plugin->set_history_stack(get_history_stack());
			private_add_plugin(plugin);
		}
		
		pluginNode = pluginNode.nextSibling();
	}
	
	return 1;
}


TCommand* PluginChain::add_plugin(Plugin * plugin, bool historable)
{
	plugin->set_history_stack(get_history_stack());
	
        return new AddRemove( this, plugin, historable, m_session,
		"private_add_plugin(Plugin*)", "pluginAdded(Plugin*)",
		"private_remove_plugin(Plugin*)", "pluginRemoved(Plugin*)",
		tr("Add Plugin (%1)").arg(plugin->get_name()));
}


TCommand* PluginChain::remove_plugin(Plugin* plugin, bool historable)
{
        return new AddRemove( this, plugin, historable, m_session,
		"private_remove_plugin(Plugin*)", "pluginRemoved(Plugin*)",
		"private_add_plugin(Plugin*)", "pluginAdded(Plugin*)",
		tr("Remove Plugin (%1)").arg(plugin->get_name()));
}


void PluginChain::private_add_plugin( Plugin * plugin )
{
	m_pluginList.append(plugin);
}


void PluginChain::private_remove_plugin( Plugin * plugin )
{
	int index = m_pluginList.indexOf(plugin);
	
	if (index >=0 ) {
		m_pluginList.removeAt(index);
	} else {
		PERROR("Plugin not found in list, this is invalid plugin remove!!!!!");
	}
}

void PluginChain::set_session(TSession * session)
{
        m_session = session;
        set_history_stack(m_session->get_history_stack());
        m_fader->set_session(session);
}

