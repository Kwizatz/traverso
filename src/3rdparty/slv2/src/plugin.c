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
#include <slv2/plugin.h>
#include <slv2/types.h>
#include <slv2/util.h>
#include <slv2/stringlist.h>
#include "private_types.h"


/* private */
SLV2Plugin
slv2_plugin_new(SLV2World world, librdf_uri* uri, const char* binary_uri)
{
	struct _Plugin* plugin = malloc(sizeof(struct _Plugin));
	plugin->world = world;
	plugin->plugin_uri = librdf_new_uri_from_uri(uri);
	plugin->binary_uri = strdup(binary_uri);
	plugin->data_uris = slv2_strings_new();
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
	
	//free(p->bundle_url);
	free(p->binary_uri);
	
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
	
	slv2_strings_free(p->data_uris);

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
	
	result->data_uris = slv2_strings_new();
	for (unsigned i=0; i < slv2_strings_size(p->data_uris); ++i)
		raptor_sequence_push(result->data_uris, strdup(slv2_strings_get_at(p->data_uris, i)));
	
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
	for (unsigned i=0; i < slv2_strings_size(p->data_uris); ++i) {
		const char* data_uri_str = slv2_strings_get_at(p->data_uris, i);
		librdf_uri* data_uri = librdf_new_uri(p->world->world, (const unsigned char*)data_uri_str);
		librdf_parser_parse_into_model(p->world->parser, data_uri, NULL, p->rdf);
		librdf_free_uri(data_uri);
	}

	// Load ports
	const unsigned char* query = (const unsigned char*)
		"PREFIX : <http://lv2plug.in/ontology#>\n"
		"SELECT DISTINCT ?port ?symbol ?index WHERE {\n"
		"<>    :port   ?port .\n"
		"?port :symbol ?symbol ;\n"
		"      :index  ?index .\n"
		"}";
	
	librdf_query* q = librdf_new_query(p->world->world, "sparql",
		NULL, query, p->plugin_uri);
	
	librdf_query_results* results = librdf_query_execute(q, p->rdf);

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
	
	if (results)
		librdf_free_query_results(results);
	
	librdf_free_query(q);

	//printf("%p %s: NUM PORTS: %d\n", (void*)p, p->plugin_uri, slv2_plugin_get_num_ports(p));
}


const char*
slv2_plugin_get_uri(SLV2Plugin p)
{
	return (const char*)librdf_uri_as_string(p->plugin_uri);
}


SLV2Strings
slv2_plugin_get_data_uris(SLV2Plugin p)
{
	return p->data_uris;
}


const char*
slv2_plugin_get_library_uri(SLV2Plugin p)
{
	return p->binary_uri;
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

		if (!strcmp(type_str, "http://lv2plug.in/ontology#Plugin"))
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
	SLV2Strings prop = slv2_plugin_get_value(plugin, "doap:name");
	
	// FIXME: guaranteed to be the untagged one?
	if (prop && slv2_strings_size(prop) >= 1)
		result = strdup(slv2_strings_get_at(prop, 0));

	if (prop)
		slv2_strings_free(prop);

	return result;
}


SLV2Strings
slv2_plugin_get_value(SLV2Plugin  p,
                      const char* predicate)
{
	assert(predicate);

    char* query = slv2_strjoin(
		"SELECT DISTINCT ?value WHERE {"
		"<> ", predicate, " ?value .\n"
		"}\n", NULL);

	SLV2Strings result = slv2_plugin_simple_query(p, query, "value");
	
	free(query);

	return result;
}

	
SLV2Strings
slv2_plugin_get_value_for_subject(SLV2Plugin  p,
                                  const char* subject,
                                  const char* predicate)
{
	assert(predicate);

    char* query = slv2_strjoin(
		"SELECT DISTINCT ?value WHERE {\n",
		subject, " ", predicate, " ?value .\n"
		"}\n", NULL);

	SLV2Strings result = slv2_plugin_simple_query(p, query, "value");
	
	free(query);

	return result;
}


SLV2Strings
slv2_plugin_get_properties(SLV2Plugin p)
{
	return slv2_plugin_get_value(p, "lv2:pluginProperty");
}


SLV2Strings
slv2_plugin_get_hints(SLV2Plugin p)
{
	return slv2_plugin_get_value(p, "lv2:pluginHint");
}


uint32_t
slv2_plugin_get_num_ports(SLV2Plugin p)
{
	return raptor_sequence_size(p->ports);
}


bool
slv2_plugin_has_latency(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?port WHERE {\n"
		"	<> lv2:port     ?port .\n"
		"	?port   lv2:portHint lv2:reportsLatency .\n"
		"}\n";

	SLV2Strings results = slv2_plugin_simple_query(p, query, "port");
	
	bool exists = (slv2_strings_size(results) > 0);
	
	slv2_strings_free(results);

	return exists;
}


uint32_t
slv2_plugin_get_latency_port(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?value WHERE {\n"
		"	<> lv2:port     ?port .\n"
		"	?port   lv2:portHint lv2:reportsLatency ;\n"
		"           lv2:index    ?index .\n"
		"}\n";

	SLV2Strings result = slv2_plugin_simple_query(p, query, "index");
	
	// FIXME: need a sane error handling strategy
	assert(slv2_strings_size(result) == 1);
	char* endptr = 0;
	uint32_t index = strtol(slv2_strings_get_at(result, 0), &endptr, 10);
	// FIXME: check.. stuff..

	return index;
}


SLV2Strings
slv2_plugin_get_supported_features(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature WHERE {\n"
		"	{ <>  lv2:optionalHostFeature  ?feature }\n"
		"		UNION\n"
		"	{ <>  lv2:requiredHostFeature  ?feature }\n"
		"}\n";

	SLV2Strings result = slv2_plugin_simple_query(p, query, "feature");
	
	return result;
}


SLV2Strings
slv2_plugin_get_optional_features(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature WHERE {\n"
		"	<>  lv2:optionalHostFeature  ?feature .\n"
		"}\n";

	SLV2Strings result = slv2_plugin_simple_query(p, query, "feature");
	
	return result;
}


SLV2Strings
slv2_plugin_get_required_features(SLV2Plugin p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature WHERE {\n"
		"	<>  lv2:requiredHostFeature  ?feature .\n"
		"}\n";

	SLV2Strings result = slv2_plugin_simple_query(p, query, "feature");
	
	return result;
}


SLV2Port
slv2_plugin_get_port_by_index(SLV2Plugin p,
                              uint32_t   index)
{
	return raptor_sequence_get_at(p->ports, (int)index);
}


SLV2Port
slv2_plugin_get_port_by_symbol(SLV2Plugin  p,
                               const char* symbol)
{
	// FIXME: sort plugins and do a binary search
	for (int i=0; i < raptor_sequence_size(p->ports); ++i) {
		SLV2Port port = raptor_sequence_get_at(p->ports, i);
		if (!strcmp(port->symbol, symbol))
			return port;
	}

	return NULL;
}

