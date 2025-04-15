/**
 * Copyright (C), Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BDI_PLUGIN_H__
#define __BDI_PLUGIN_H__

/**
 * opc_ua_create:
 * @server: OPC-UA Server object
 * @logger: an open62541 logger instance
 * @params: pointer to user data (can be used to pass in additional parameters
 *   for the initialization of the plugin)
 * @err: return location for a #GError
 *
 * Sets up and initializes the plugin.
 *
 * Returns: TRUE if setup was successful, FALSE otherwise.
 */
gboolean
opc_ua_create(UA_Server *server,
              UA_Logger *logger,
              G_GNUC_UNUSED gpointer *params,
              GError **err);

/**
 * opc_ua_destroy:
 *
 * Frees allocated resources and performs any necessary shutdown operations for
 * the plugin.
 */
void
opc_ua_destroy(void);

/**
 * opc_ua_get_plugin_name:
 *
 * Retrieves the name of the plugin.
 *
 * Returns: the plugin name as set in the plugin constructor or an error string
 *   if no name was set.
 */
const gchar *
opc_ua_get_plugin_name(void);

#endif /* __BDI_PLUGIN_H__ */
