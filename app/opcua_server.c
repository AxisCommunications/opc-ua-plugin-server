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

#include <glib-unix.h>
#include <syslog.h>

#include "error.h"
#include "log.h"
#include "plugin.h"
#include "opcua_parameter.h"
#include "opcua_open62541.h"
#include "opcua_server.h"

static void
open_syslog(const gchar *app_name)
{
  g_assert(app_name);

  openlog(app_name, LOG_PID, LOG_LOCAL4);
}

static void
close_syslog(app_context_t *ctx)
{
  g_assert(ctx != NULL);

  LOG_I(&ctx->logger, "Closing syslog...");
  closelog();
}

static gboolean
signal_handler(gpointer user_data)
{
  app_context_t *ctx = user_data;

  g_assert(ctx != NULL);
  g_assert(ctx->main_loop);

  LOG_I(&ctx->logger, "Quitting main loop...");
  g_main_loop_quit(ctx->main_loop);

  return G_SOURCE_REMOVE;
}

static void
init_signal_handlers(app_context_t *ctx)
{
  g_assert(ctx != NULL);

  g_unix_signal_add(SIGTERM, signal_handler, ctx);
  g_unix_signal_add(SIGINT, signal_handler, ctx);
}

static void
init_ua_logger(app_context_t *ctx)
{
  g_assert(ctx != NULL);

  /* initial log level until we get to read in our configuration parameters */
  ctx->logger = UA_Log_Syslog_withLevel(UA_LOGLEVEL_WARNING);
}

static void
init_ua_plugin(gpointer data, gpointer user_data)
{
  gchar *name = (gchar *) data;
  app_context_t *ctx = (app_context_t *) user_data;
  opc_plugin_t *plugin;
  GError *lerr = NULL;

  g_assert(name != NULL);
  g_assert(ctx != NULL);

  plugin = plugin_load(name, &ctx->logger, &lerr);

  if (plugin == NULL) {
    LOG_E(&ctx->logger,
          "Failed to load plugin '%s': %s",
          name,
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return;
  }

  if (!plugin->fs.ua_create(ctx->server, &ctx->logger, NULL, &lerr)) {
    LOG_E(&ctx->logger,
          "Failed to create plugin '%s': %s",
          name,
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return;
  }

  LOG_I(&ctx->logger, "Loaded plugin: %s", plugin->fs.ua_get_plugin_name());
  ctx->plugins = g_slist_append(ctx->plugins, plugin);
}

static gboolean
launch_ua_server(app_context_t *ctx)
{
  GError *lerr = NULL;
  GSList *plugin_names;

  g_assert(ctx != NULL);
  g_assert(!ctx->ua_server_running);

  /* Create an OPC UA server */
  LOG_D(&ctx->logger, "Create UA server listening on port: %u", ctx->port);
  if (!ua_server_init(ctx, ctx->port, ctx->log_level, &lerr)) {
    LOG_E(&ctx->logger, "ua_server_init() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return FALSE;
  }

  /* Search the lib dir and load all opc-plugins */
  plugin_names = plugin_get_names(&ctx->logger);

  if (plugin_names == NULL) {
    LOG_W(&ctx->logger,
          "No plugins found... Starting the server without plugins");
    g_slist_free(plugin_names);
  } else {
    g_slist_foreach(plugin_names, init_ua_plugin, ctx);
    g_slist_free_full(plugin_names, g_free);
  }

  ctx->ua_server_running = TRUE;
  LOG_D(&ctx->logger, "Starting UA server on port %u ...", ctx->port);
  if (!ua_server_run(ctx, &ctx->ua_server_thread_id)) {
    LOG_E(&ctx->logger, "Failed to launch UA server!");
    ctx->ua_server_running = FALSE;
    return FALSE;
  }

  return TRUE;
}

static void
free_plugins(gpointer data, gpointer user_data)
{
  opc_plugin_t *p = (opc_plugin_t *) data;
  app_context_t *ctx = (app_context_t *) user_data;

  g_assert(p != NULL);
  g_assert(user_data != NULL);

  LOG_I(&ctx->logger, "Unload plugin '%s'", p->fs.ua_get_plugin_name());

  p->fs.ua_destroy();

  plugin_unload(p, &ctx->logger);
}

static void
cleanup(app_context_t *ctx)
{
  g_assert(ctx != NULL);

  g_clear_pointer(&ctx->main_loop, g_main_loop_unref);

  if (ctx->ua_server_running) {
    /* flag the UA server that we want it to finish */
    ctx->ua_server_running = FALSE;
    LOG_D(&ctx->logger,
          "ua_server_running: set to FALSE, waiting for OPC-UA server thread.");

    /* Wait for the server thread to finish up */
    g_thread_join(ctx->ua_server_thread_id);
    LOG_D(&ctx->logger, "OPC-UA server thread finished...");

  } else if (ctx->server != NULL) {
    /* the server thread is not running but a UA_Server struct was allocated */
    UA_Server_delete(ctx->server);
  }

  g_slist_foreach(ctx->plugins, free_plugins, ctx);
  g_slist_free(ctx->plugins);

  ax_parameter_free(ctx->axparam);
}

int
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
  int retval = EXIT_FAILURE;
  app_context_t ctx = { 0 };
  GError *lerr = NULL;

  open_syslog(APPNAME);

  init_ua_logger(&ctx);

  if (!init_ua_parameters(&ctx, APPNAME, &lerr)) {
    LOG_E(&ctx.logger, "init_ua_parameters() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
    goto err_out;
  }

  LOG_I(&ctx.logger, "%s: Starting", APPNAME);

  init_signal_handlers(&ctx);

  if (!launch_ua_server(&ctx)) {
    LOG_E(&ctx.logger, "Failed to launch UA server");
    goto err_out;
  }

  /* Main loop */
  ctx.main_loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(ctx.main_loop);

  retval = EXIT_SUCCESS;

err_out:

  cleanup(&ctx);

  LOG_I(&ctx.logger, "%s: Exiting", APPNAME);
  close_syslog(&ctx);

  return retval;
}
