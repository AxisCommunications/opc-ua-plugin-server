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
