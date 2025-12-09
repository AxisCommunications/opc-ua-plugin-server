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

#ifndef __THERMAL_PLUGIN_H__
#define __THERMAL_PLUGIN_H__

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

#endif /* __THERMAL_PLUGIN_H__ */
