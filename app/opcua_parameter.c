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

#include <axsdk/axparameter.h>

#include "error.h"
#include "opcua_parameter.h"
#include "opcua_server.h"

DEFINE_GQUARK("opcua-parameter")

static gboolean
handle_loglevel(app_context_t *ctx, gint val, GError **err)
{
  g_assert(ctx != NULL);
  g_assert(err == NULL || *err == NULL);

  if (val < LOG_LEVEL_MIN || val > LOG_LEVEL_MAX) {
    SET_ERROR(err, -1, "LogLevel value is out of range");
    return FALSE;
  }

  switch (val) {
  case 0:
    ctx->log_level = UA_LOGLEVEL_DEBUG;
    break;
  case 1:
    ctx->log_level = UA_LOGLEVEL_INFO;
    break;
  case 2:
    ctx->log_level = UA_LOGLEVEL_WARNING;
    break;
  case 3:
    ctx->log_level = UA_LOGLEVEL_ERROR;
    break;
  case 4:
    ctx->log_level = UA_LOGLEVEL_FATAL;
    break;
  default:
    break;
  }

  ctx->logger.context = (void *) ctx->log_level;

  return TRUE;
}

static gboolean
handle_port(app_context_t *ctx, gint val, GError **err)
{
  g_assert(ctx != NULL);
  g_assert(err == NULL || *err == NULL);

  if (val < MIN_PORT || val > MAX_PORT) {
    SET_ERROR(err, -1, "Port value is out of range");
    return FALSE;
  }
  ctx->port = val;

  return TRUE;
}

static gboolean
handle_param(app_context_t *ctx,
             const gchar *name,
             const gchar *value,
             GError **err)
{
  gint64 val;

  g_assert(NULL != ctx);
  g_assert(NULL != name);
  g_assert(NULL != value);
  g_assert(err == NULL || *err == NULL);

  if (g_strcmp0(name, "LogLevel") == 0) {
    val = g_ascii_strtoll(value, NULL, 10);
    if (!handle_loglevel(ctx, val, err)) {
      g_prefix_error(err, "handle_loglevel() failed: ");
      return FALSE;
    }
  } else if (g_strcmp0(name, "Port") == 0) {
    val = g_ascii_strtoll(value, NULL, 10);
    if (!handle_port(ctx, val, err)) {
      g_prefix_error(err, "handle_port() failed: ");
      return FALSE;
    }
  } else {
    SET_ERROR(err, -1, "Axparam: %s is not supported", name);
    return FALSE;
  }

  return TRUE;
}

static gboolean
setup_param(app_context_t *ctx,
            const gchar *name,
            AXParameter *axparam,
            GError **err)
{
  gchar *value;

  g_assert(NULL != ctx);
  g_assert(NULL != name);
  g_assert(NULL != axparam);
  g_assert(err == NULL || *err == NULL);

  if (!ax_parameter_get(axparam, name, &value, err)) {
    g_prefix_error(err, "ax_parameter_get() failed: ");
    return FALSE;
  }

  if (!handle_param(ctx, name, value, err)) {
    g_prefix_error(err, "handle_param() failed: ");
    g_clear_pointer(&value, g_free);
    return FALSE;
  }
  g_clear_pointer(&value, g_free);

  return TRUE;
}

gboolean
init_ua_parameters(app_context_t *ctx, const gchar *app_name, GError **err)
{
  g_return_val_if_fail(NULL != ctx, FALSE);
  g_return_val_if_fail(NULL != app_name, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  ctx->axparam = ax_parameter_new(app_name, err);

  if (ctx->axparam == NULL) {
    g_prefix_error(err, "ax_parameter_new() failed: ");
    return FALSE;
  }

  if (!setup_param(ctx, "LogLevel", ctx->axparam, err)) {
    g_prefix_error(err, "setup_param() failed: ");
    return FALSE;
  }

  if (!setup_param(ctx, "Port", ctx->axparam, err)) {
    g_prefix_error(err, "setup_param() failed: ");
    return FALSE;
  }

  return TRUE;
}
