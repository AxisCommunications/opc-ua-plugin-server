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

#ifndef __OPCUA_SERVER_H__
#define __OPCUA_SERVER_H__

#include <axsdk/axparameter.h>
#include <glib.h>
#include <open62541/plugin/log.h>
#include <open62541/types.h>

#include "plugin.h"

typedef struct {
  /* application main loop */
  GMainLoop *main_loop;
  /* handle to application configuration parameters */
  AXParameter *axparam;
  /* list of actively loaded OPC-UA plugins */
  GSList *plugins;
  /* an open62541 logger instance */
  UA_Logger logger;
  /* runtime logging level (user configurable parameter) */
  UA_LogLevel log_level;
  /* TCP listening port of the OPC-UA server (user configurable parameter) */
  guint port;
  /* an open62541 server instance */
  UA_Server *server;
  /* flag to signal the server thread to finish */
  volatile UA_Boolean ua_server_running;
  /* a GLib thread handle for the OPC-UA thread */
  GThread *ua_server_thread_id;
} app_context_t;

#endif /* __OPCUA_SERVER_H__ */
