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
