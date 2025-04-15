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

#include <open62541/server_config_default.h>
#include <open62541/plugin/accesscontrol_default.h>

#include "error.h"
#include "log.h"
#include "opcua_open62541.h"
#include "opcua_server.h"

DEFINE_GQUARK("opc-ua-open62541")

/**
 * run_ua_server:
 * @gdata: pointer to the application context #app_context_t
 *
 * Thread function (a #GThreadFunc) running the OPC-UA server
 */
static gpointer
run_ua_server(gpointer data)
{
  app_context_t *ctx = data;
  UA_StatusCode status;

  g_assert(NULL != ctx);
  g_assert(NULL != ctx->server);

  LOG_D(&ctx->logger, "Starting UA server ...");
  status = UA_Server_run(ctx->server, &ctx->ua_server_running);

  LOG_D(&ctx->logger, "UA Server exit status: %s", UA_StatusCode_name(status));
  UA_Server_delete(ctx->server);
  ctx->server = NULL;

  return NULL;
}

gboolean
ua_server_init(app_context_t *ctx,
               const UA_UInt16 port,
               UA_LogLevel log_level,
               GError **err)
{
  UA_StatusCode status;
  UA_ServerConfig *config;

  g_return_val_if_fail(NULL != ctx, FALSE);
  g_return_val_if_fail(NULL == ctx->server, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  ctx->server = UA_Server_new();
  if (ctx->server == NULL) {
    SET_ERROR(err, -1, "UA_Server_new() failed!");
    return FALSE;
  }

  config = UA_Server_getConfig(ctx->server);
  if (config == NULL) {
    SET_ERROR(err, -1, "UA_Server_getConfig() failed: ");
    return FALSE;
  }

  status = UA_ServerConfig_setMinimal(config, port, NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_ServerConfig_setMinimal() failed: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  /* Adjust the logging level for the server thread to be in sync with the
   * logging level set for the ACAP via the 'LogLevel' configuration parameter.
   */
  config->logging->context = (void *) log_level;

  /* Name of the server */
  UA_String_clear(&config->applicationDescription.applicationName.text);
  config->applicationDescription.applicationName.text =
          UA_String_fromChars("axis:axis_opcua_server");

  /* custom Application URI */
  UA_String_clear(&config->applicationDescription.applicationUri);
  config->applicationDescription.applicationUri =
          UA_String_fromChars("urn:axis.opcua.server");

  return TRUE;
}

gboolean
ua_server_run(gpointer data, GThread **thread_id)
{
  app_context_t *ctx = data;

  g_return_val_if_fail(NULL != ctx, FALSE);
  g_return_val_if_fail(NULL != ctx->server, FALSE);
  g_return_val_if_fail(NULL != thread_id, FALSE);

  /* spawn a new thread that will run 'run_ua_server' and return the
   * thread id in '*thread_id' */
  *thread_id = g_thread_new("opc_ua_server_thread", run_ua_server, data);

  return (*thread_id != NULL);
}
