/**
 * MIT License
 *
 * Copyright (c) 2025 Axis Communications AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <glib.h>
#include "gmodule.h"
#include <open62541/types.h>

#define ACAP_MODULES_PATH "/usr/local/packages/" APPNAME "/lib"

/* plugin constructor: allocates resources and initializes the OPC-UA
 * information model associated with the plugin */
typedef gboolean (*ua_create_t)(UA_Server *server,
                                UA_Logger *logger,
                                gpointer *params,
                                GError **err);

/* plugin destructor: releases all allocated resources */
typedef void (*ua_destroy_t)(void);

/* returns the plugin name set in the constructor */
typedef const gchar *(*ua_get_plugin_name_t)(void);

/* function set defining the (API) interface of an OPC-UA plugin */
typedef struct conf_plugin_func_set {
  ua_create_t ua_create;                   /* plugin constructor */
  ua_destroy_t ua_destroy;                 /* plugin destructor */
  ua_get_plugin_name_t ua_get_plugin_name; /* returns the plugin name */
} conf_plugin_func_set_t;

/* structure used for the housekeeping of an OPC-UA plugin */
typedef struct opc_plugin {
  /* the function set of the plugin */
  conf_plugin_func_set_t fs;
  /* full path and name to the shared library (.so) implementing the plugin */
  gchar *filename;
  /* associated #GModule handle of the dynamically-loaded library */
  GModule *module;
} opc_plugin_t;

/**
 * plugin_get_names:
 * @logger: an open62541 logger instance
 *
 * Returns a list of all available OPC-UA plugins. Each element in the list is
 * a string representing the name of the plugin (.so) file.
 *
 * Returns: a #GSList with all the available plugins
 */
GSList *
plugin_get_names(UA_Logger *logger);

/**
 * plugin_load:
 * @plugin_name: the file name of the plugin
 * @logger: an open62541 logger instance
 * @err: return location for a #GError
 *
 * Loads the @plugin_name and returns a pointer to a newly allocated
 * #opc_plugin_t structure.
 *
 * Returns: a pointer to #opc_plugin_t on success, NULL if @err is set.
 */
opc_plugin_t *
plugin_load(const gchar *plugin_name, UA_Logger *logger, GError **err);

/**
 * plugin_unload:
 * @plugin: pointer to an #opc_plugin_t structure as returned by plugin_load()
 * @logger: an open62541 logger instance
 *
 * Unloads a previously loaded plugin and frees the associated #opc_plugin_t.
 */
void
plugin_unload(opc_plugin_t *plugin, UA_Logger *logger);

#endif /* __PLUGIN_H__ */
