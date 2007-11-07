/* SLV2
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 *  
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _XOPEN_SOURCE 500

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <librdf.h>
#include "slv2_internal.h"
#include <slv2/plugin.h>
#include <slv2/types.h>
#include <slv2/util.h>
#include <slv2/values.h>
#include <slv2/pluginclass.h>
#include <slv2/pluginclasses.h>
#include <slv2/pluginuis.h>


/* private */
SLV2Plugin
slv2_plugin_new(SLV2World world, librdf_uri* uri, librdf_uri* bundle_uri, librdf_uri* binary_uri)
{
	struct _SLV2Plugin* plugin = malloc(sizeof(struct _SLV2Plugin));
	plugin->world = world;
	plugin->plugin_uri = librdf_new_uri_from_uri(uri);
	plugin->bundle_uri = librdf_new_uri_from_uri(bundle_uri);
	plugin->binary_uri = librdf_new_uri_from_uri(binary_uri);
	plugin->plugin_class = NULL;
	plugin->data_uris = slv2_values_new();
	plugin->ports = raptor_new_sequence((void (*)(void*))&slv2_port_free, NULL);
	plugin->storage = NULL;
	plugin->rdf = NULL;

	return plugin;
}


/* private */
void
slv2_plugin_free(SLV2Plugin p)
{
	librdf_free_uri(p->plugin_uri);
	p->plugin_uri = NULL;
	
	librdf_free_uri(p->bundle_uri);
	p->bundle_uri = NULL;
	
	librdf_free_uri(p->binary_uri);
	p->binary_uri = NULL;
	
	raptor_free_sequence(p->ports);
	p->ports = NULL;

	if (p->rdf) {
		librdf_free_model(p->rdf);
		p->rdf = NULL;
	}
	
	if (p->storage) {
		librdf_free_storage(p->storage);
		p->storage = NULL;
	}
	
	slv2_values_free(p->data_uris);
	p->data_uris = NULL;

	free(p);
}


// FIXME: ew
librdf_query_results*
slv2_plugin_query(SLV2Plugin plugin,
                  const char* sparql_str);


/*
SLV2Plugin
slv2_plugin_duplicate(SLV2Plugin p)
{
	assert(p);
	struct _Plugin* result = malloc(sizeof(struct _Plugin));
	result->world = p->world;
	result->plugin_uri = librdf_new_uri_from_uri(p->plugin_uri);

	//result->bundle_url = strdup(p->bundle_url);
	result->binary_uri = strdup(p->binary_uri);
	
	result->data_uris = slv2_values_new();
	for (unsigned i=0; i < slv2_values_size(p->data_uris); ++i)
		raptor_sequence_push(result->data_uris, strdup(slv2_values_get_at(p->data_uris, i)));
	
	result->ports = raptor_new_sequence((void (*)(void*))&slv2_port_free, NULL);
	for (int i=0; i < raptor_sequence_size(p->ports); ++i)
		raptor_sequence_push(result->ports, slv2_port_duplicate(raptor_sequence_get_at(p->ports, i)));

	result->storage = NULL;
	result->rdf = NULL;

	return result;
}
*/


/** comparator for sorting */
int
slv2_port_compare_by_index(const void* a, const void* b)
{
	SLV2Port port_a = *(SLV2Port*)a;
	SLV2Port port_b = *(SLV2Port*)b;

	if (port_a->index < port_b->index)
		return -1;
	else if (port_a->index == port_b->index)
		return 0;
	else //if (port_a->index > port_b->index)
		return 1;
}


void
slv2_plugin_load(SLV2Plugin p)
{
	//printf("Loading cache for %s\n", (const char*)librdf_uri_as_string(p->plugin_uri));

	if (!p->storage) {
		assert(!p->rdf);
		p->storage = librdf_new_storage(p->world->world, "hashes", NULL,
				"hash-type='memory'");
		p->rdf = librdf_new_model(p->world->world, p->storage, NULL);
	}

	// Parse all the plugin's data files into RDF model
	for (unsigned i=0; i < slv2_values_size(p->data_uris); ++i) {
		SLV2Value data_uri_val = slv2_values_get_at(p->data_uris, i);
		librdf_uri* data_uri = librdf_new_uri(p->world->world,
				(const unsigned char*)slv2_value_as_uri(data_uri_val));
		librdf_parser_parse_into_model(p->world->parser, data_uri, NULL, p->rdf);
		librdf_free_uri(data_uri);
	}

	// Load plugin_class
	const unsigned char* query = (const unsigned char*)
		"SELECT DISTINCT ?class WHERE { <> a ?class }";
	
	librdf_query* q = librdf_new_query(p->world->world, "sparql",
		NULL, query, p->plugin_uri);
	
	librdf_query_results* results = librdf_query_execute(q, p->rdf);
		
	while (!librdf_query_results_finished(results)) {
		librdf_node* class_node    = librdf_query_results_get_binding_value(results, 0);
		librdf_uri*  class_uri     = librdf_node_get_uri(class_node);
		assert(class_uri);
		const char*  class_uri_str = (const char*)librdf_uri_as_string(class_uri);
		
		if ( ! librdf_uri_equals(class_uri, p->world->lv2_plugin_class->uri) ) {

			SLV2PluginClass plugin_class = slv2_plugin_classes_get_by_uri(
					p->world->plugin_classes, class_uri_str);
			
			librdf_free_node(class_node);

			if (plugin_class) {
				p->plugin_class = plugin_class;
				break;
			}
		}

		librdf_query_results_next(results);
	}
	
	if (p->plugin_class == NULL)
		p->plugin_class = p->world->lv2_plugin_class;

	librdf_free_query_results(results);
	librdf_free_query(q);
	
	// Load ports
	query = (const unsigned char*)
		"PREFIX : <http://lv2plug.in/ns/lv2core#>\n"
		"SELECT DISTINCT ?port ?symbol ?index WHERE {\n"
		"<>    :port   ?port .\n"
		"?port :symbol ?symbol ;\n"
		"      :index  ?index .\n"
		"}";
	
	q = librdf_new_query(p->world->world, "sparql",
		NULL, query, p->plugin_uri);
	
	results = librdf_query_execute(q, p->rdf);

	while (!librdf_query_results_finished(results)) {
	
		//librdf_node* port_node = librdf_query_results_get_binding_value(results, 0);
		librdf_node* symbol_node = librdf_query_results_get_binding_value(results, 1);
		librdf_node* index_node = librdf_query_results_get_binding_value(results, 2);

		//assert(librdf_node_is_blank(port_node));
		assert(librdf_node_is_literal(symbol_node));
		assert(librdf_node_is_literal(index_node));

		//const char* id = (const char*)librdf_node_get_blank_identifier(port_node);
		const char* symbol = (const char*)librdf_node_get_literal_value(symbol_node);
		const char* index = (const char*)librdf_node_get_literal_value(index_node);

		//printf("%s: PORT: %s %s\n", p->plugin_uri, index, symbol);
		
		// Create a new SLV2Port
		SLV2Port port = slv2_port_new((unsigned)atoi(index), symbol);
		raptor_sequence_push(p->ports, port);
		
		librdf_free_node(symbol_node);
		librdf_free_node(index_node);
		
		librdf_query_results_next(results);
	}
	
	raptor_sequence_sort(p->ports, slv2_port_compare_by_index);
	
	librdf_free_query_results(results);
	librdf_free_query(q);

	//printf("%p %s: NUM PORTS: %d\n", (void*)p, p->plugin_uri, slv2_plugin_get_num_ports(p));
}


const char*
slv2_plugin_get_uri(SLV2Plugin p)
{
	assert(p);
	assert(p->plugin_uri);
	return (const char*)librdf_uri_as_string(p->plugin_uri);
}


const char*
slv2_plugin_get_bundle_uri(SLV2Plugin p)
{
	assert(p);
	assert(p->bundle_uri);
	return (const char*)librdf_uri_as_string(p->bundle_uri);
}


const char*
slv2_plugin_get_library_uri(SLV2Plugin p)
{
	assert(p);
	assert(p->binary_uri);
	return (const char*)librdf_uri_as_string(p->binary_uri);
}


SLV2Values
slv2_plugin_get_data_uris(SLV2Plugin p)
{
	return p->data_uris;
}


SLV2PluginClass
slv2_plugin_get_class(SLV2Plugin p)
{
	// FIXME: Typical use case this will bring every single plugin model
	// into memory
	
	if (!p->rdf)
		slv2_plugin_load(p);

	return p->plugin_class;
}


bool
slv2_plugin_verify(SLV2Plugin plugin)
{
	char* query_str = 
		"SELECT DISTINCT ?type ?name ?license ?port WHERE {\n"
		"<> a ?type ;\n"
		"doap:name    ?name ;\n"
		"doap:license ?license ;\n"
		"lv2:port     [ lv2:index ?port ] .\n}";

	librdf_query_results* results = slv2_plugin_query(plugin, query_str);

	bool has_type    = false;
	bool has_name    = false;
	bool has_license = false;
	bool has_port    = false;

	while (!librdf_query_results_finished(results)) {
		librdf_node* type_node = librdf_query_results_get_binding_value(results, 0);
		const char* const type_str = (const char*)librdf_node_get_literal_value(type_node);
		librdf_node* name_node = librdf_query_results_get_binding_value(results, 1);
		//const char* const name = (const char*)librdf_node_get_literal_value(name_node);
		librdf_node* license_node = librdf_query_results_get_binding_value(results, 2);
		librdf_node* port_node = librdf_query_results_get_binding_value(results, 3);

		if (!strcmp(type_str, "http://lv2plug.in/ns/lv2core#Plugin"))
			has_type = true;
		
		if (name_node)
			has_name = true;
		
		if (license_node)
			has_license = true;
		
		if (port_node)
			has_port = true;

		librdf_free_node(type_node);
		librdf_free_node(name_node);
		librdf_free_node(license_node);
		librdf_free_node(port_node);

		librdf_query_results_next(results);
	}

	librdf_free_query_results(results);

	if ( ! (has_type && has_name && has_license && has_port) ) {
		fprintf(stderr, "Invalid LV2 Plugin %s\n", slv2_plugin_get_uri(plugin));
		return false;
	} else {
		return true;
	}
}


char*
slv2_plugin_get_name(SLV2Plugin plugin)
{
	char* result     = NULL;
	SLV2Values prop = slv2_plugin_get_value(plugin, SLV2_QNAME, "doap:name");
	
	// FIXME: lang? guaranteed to be the untagged one?
	if (prop && slv2_values_size(prop) > 0) {
		SLV2Value val = slv2_values_get_at(prop, 0);
		if (slv2_value_is_string(val))
			result = strdup(slv2_value_as_string(val));
	}

	if (prop)
		slv2_values_free(prop);

	return result;
}


SLV2Values
slv2_plugin_get_value(SLV2Plugin  p,
                      SLV2URIType predicate_type,
                      const char* predicate)
{
	char* query = NULL;
	
	/* Hack around broken RASQAL, full URI predicates don't work :/ */

	if (predicate_type == SLV2_URI) {
		query = slv2_strjoin(
			"PREFIX slv2predicate: <", predicate, ">",
			"SELECT DISTINCT ?value WHERE { \n"
			"<> slv2predicate: ?value \n"
			"}\n", NULL);
	} else {
    	query = slv2_strjoin(
			"SELECT DISTINCT ?value WHERE { \n"
			"<> ", predicate, " ?value \n"
			"}\n", NULL);
	}

	SLV2Values result = slv2_plugin_simple_query(p, query, 0);
	
	free(query);

	return result;
}

	
SLV2Values
slv2_plugin_get_value_for_subject(SLV2Plugin  p,
                                  SLV2Value   subject,
                                  SLV2URIType predicate_type,
                                  const char* predicate)
{
	if ( ! slv2_value_is_uri(subject)) {
		fprintf(stderr, "slv2_plugin_get_value_for_subject error: "
				"passed non-URI subject\n");
		return NULL;
	}

	char* query = NULL;

	/* Hack around broken RASQAL, full URI predicates don't work :/ */

	char* subject_token = slv2_value_get_turtle_token(subject);

	if (predicate_type == SLV2_URI) {
		query = slv2_strjoin(
			"PREFIX slv2predicate: <", predicate, ">",
			"SELECT DISTINCT ?value WHERE { \n",
			subject_token, " slv2predicate: ?value \n"
			"}\n", NULL);
	} else {
    	query = slv2_strjoin(
			"SELECT DISTINCT ?value WHERE { \n",
			subject_token, " ", predicate, " ?value \n"
			"}\n", NULL);
	}

	SLV2Values result = slv2_plugin_simple_query(p, query, 0);
	
	free(query);
	free(subject_token);

	return result;
}


SLV2Values
slv2_plugin_get_properties(SLV2Plugin p)
{
	return slv2_plugin_get_value(p, SLV2_QNAME, "lv2:pluginProperty");
}


SLV2Values
slv2_plugin_get_hints(SLV2Plugin p)
{
	return slv2_plugin_get_value(p, SLV2_QNAME, "lv2:pluginHint");
}


uint32_t
slv2_plugin_get_num_ports(SLV2Plugin p)
{
	if (!p->rdf)
		slv2_plugin_load(p);
	
	return raptor_sequence_size(p->ports);
}


bool
slv2_plugin_has_latency(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?index WHERE {\n"
		"	<>      lv2:port         ?port .\n"
		"	?port   lv2:portProperty lv2:reportsLatency ;\n"
		"           lv2:index        ?index .\n"
		"}\n";

	SLV2Values results = slv2_plugin_simple_query(p, query, 0);
	const bool latent = (slv2_values_size(results) > 0);
	slv2_values_free(results);
	
	return latent;
}


uint32_t
slv2_plugin_get_latency_port(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?index WHERE {\n"
		"	<>      lv2:port         ?port .\n"
		"	?port   lv2:portProperty lv2:reportsLatency ;\n"
		"           lv2:index        ?index .\n"
		"}\n";

	SLV2Values result = slv2_plugin_simple_query(p, query, 0);
	
	// FIXME: need a sane error handling strategy
	assert(slv2_values_size(result) > 0);
	SLV2Value val = slv2_values_get_at(result, 0);
	assert(slv2_value_is_int(val));

	return slv2_value_as_int(val);
}


SLV2Values
slv2_plugin_get_supported_features(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature WHERE {\n"
		"	{ <>  lv2:optionalFeature ?feature }\n"
		"	UNION\n"
		"	{ <>  lv2:requiredFeature ?feature }\n"
		"}\n";

	SLV2Values result = slv2_plugin_simple_query(p, query, 0);
	
	return result;
}


SLV2Values
slv2_plugin_get_optional_features(SLV2Plugin p)
{
	return slv2_plugin_get_value(p, SLV2_QNAME, "lv2:optionalFeature");
}


SLV2Values
slv2_plugin_get_required_features(SLV2Plugin p)
{
	return slv2_plugin_get_value(p, SLV2_QNAME, "lv2:requiredFeature");
}


SLV2Port
slv2_plugin_get_port_by_index(SLV2Plugin p,
                              uint32_t   index)
{
	if (!p->rdf)
		slv2_plugin_load(p);
	
	return raptor_sequence_get_at(p->ports, (int)index);
}


SLV2Port
slv2_plugin_get_port_by_symbol(SLV2Plugin  p,
                               const char* symbol)
{
	if (!p->rdf)
		slv2_plugin_load(p);
	
	// FIXME: sort plugins and do a binary search
	for (int i=0; i < raptor_sequence_size(p->ports); ++i) {
		SLV2Port port = raptor_sequence_get_at(p->ports, i);
		if (!strcmp(port->symbol, symbol))
			return port;
	}

	return NULL;
}


SLV2UIs
slv2_plugin_get_uis(SLV2Plugin plugin)
{
    const char* const query_str =
		"PREFIX guiext: <http://ll-plugins.nongnu.org/lv2/ext/gui/dev/1#>\n"
		"SELECT DISTINCT ?uri ?type ?binary WHERE {\n"
		"<>   guiext:gui    ?uri .\n"
		"?uri a             ?type ;\n"
		"     guiext:binary ?binary .\n"
		"}\n";

	librdf_query_results* results = slv2_plugin_query(plugin, query_str);

	SLV2UIs result = slv2_uis_new();

	while (!librdf_query_results_finished(results)) {
		librdf_node* uri_node    = librdf_query_results_get_binding_value(results, 0);
		librdf_node* type_node   = librdf_query_results_get_binding_value(results, 1);
		librdf_node* binary_node = librdf_query_results_get_binding_value(results, 2);

		SLV2UI ui = slv2_ui_new(plugin->world,
				librdf_node_get_uri(uri_node),
				librdf_node_get_uri(type_node),
				librdf_node_get_uri(binary_node));

		raptor_sequence_push(result, ui);

		librdf_free_node(uri_node);
		librdf_free_node(type_node);
		librdf_free_node(binary_node);

		librdf_query_results_next(results);
	}

	librdf_free_query_results(results);

	if (slv2_uis_size(result) > 0) {
		return result;
	} else {
		slv2_uis_free(result);
		return NULL;
	}
}

#if 0
SLV2Value
slv2_plugin_get_ui_library_uri(SLV2Plugin plugin, 
                               SLV2Value  ui)
{
	assert(ui->type == SLV2_VALUE_UI);
	
	if (!plugin->rdf)
		slv2_plugin_load(plugin);

	SLV2Values values =  slv2_plugin_get_value_for_subject(plugin, ui, SLV2_URI,
			"http://ll-plugins.nongnu.org/lv2/ext/gui/dev/1#binary");

	if (!values || slv2_values_size(values) == 0) {
		slv2_values_free(values);
		return NULL;
	}

	SLV2Value value = slv2_values_get_at(values, 0);
	if (!slv2_value_is_uri(value)) {
		slv2_values_free(values);
		return NULL;
	}

	value = slv2_value_duplicate(value);
	slv2_values_free(values);

	return value;
}
#endif


void*
slv2_plugin_load_ui(SLV2Plugin plugin,
                    SLV2Value  ui)
{
#if 0
	SLV2Value lib_uri = slv2_plugin_get_ui_library_uri(plugin, ui);

	if (!lib_uri)
		return NULL;

	//LV2UI_Handle handle =
	//
#endif
	return NULL;
}

