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

#ifndef __OPCUA_OPEN62541_H__
#define __OPCUA_OPEN62541_H__

#include <glib.h>
#include <open62541/server.h>

#include "opcua_server.h"

/**
 * ua_server_init:
 * @ctx: application context.
 * @port: TCP port number for the server
 * @log_level: log level
 * @err: return location for a #GError
 *
 * Creates an OPC-UA server object and initializes and configures all necessary
 * parameters in the server config (#UA_ServerConfig).
 *
 * Returns: TRUE if setup was successful, FALSE otherwise.
 */
gboolean
ua_server_init(app_context_t *ctx,
               const UA_UInt16 port,
               UA_LogLevel log_level,
               GError **err);

/**
 * ua_server_run:
 * @data: application context
 * @thread_id: (out) the id of the newly created #GThread
 *
 * Starts an OPC-UA Server in a new thread and returns its thread id.
 *
 * Returns: TRUE if the server thread was successfully created, FALSE otherwise.
 */
gboolean
ua_server_run(gpointer data, GThread **thread_id);

#endif /* __OPCUA_OPEN62541_H__ */
