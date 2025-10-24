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

#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>

#include "error.h"
#include "log.h"
#include "plugin.h"

DEFINE_GQUARK("plugin")

GSList *
plugin_get_names(UA_Logger *logger)
{
  GError *lerr = NULL;
  GFileEnumerator *enumerator;
  GFile *root;
  GSList *plugin_names = NULL;

  g_return_val_if_fail(logger != NULL, NULL);

  root = g_file_new_for_path(ACAP_MODULES_PATH);

  enumerator = g_file_enumerate_children(root,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME
                                         "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         NULL,
                                         &lerr);

  if (enumerator == NULL) {
    LOG_E(logger, "g_file_enumerate_children() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
    goto err_out;
  }

  while (TRUE) {
    GFileInfo *info = NULL;
    const gchar *name = NULL;
    GFileType type;

    info = g_file_enumerator_next_file(enumerator, NULL, NULL);
    if (info == NULL)
      break;

    name = g_file_info_get_name(info);
    type = g_file_info_get_file_type(info);

    if (type == G_FILE_TYPE_REGULAR && g_str_has_prefix(name, "libopcua")) {
      gchar *filename;

      filename = g_build_filename(name, NULL);
      plugin_names = g_slist_append(plugin_names, g_strdup(filename));
      LOG_I(logger, "added plugin to list: %s", filename);
      g_clear_pointer(&filename, g_free);
    }

    g_object_unref(info);
  }

  g_object_unref(enumerator);

err_out:
  g_object_unref(root);

  return plugin_names;
}

opc_plugin_t *
plugin_load(const gchar *plugin_name, UA_Logger *logger, GError **err)
{
  gchar *filename;
  opc_plugin_t *p;
  GModule *module;

  g_return_val_if_fail(plugin_name != NULL, NULL);
  g_return_val_if_fail(logger != NULL, NULL);
  g_return_val_if_fail(err == NULL || *err == NULL, NULL);

  filename = g_build_filename(ACAP_MODULES_PATH, plugin_name, NULL);

  LOG_D(logger, "%s", filename);

  module = g_module_open(filename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  if (module == NULL) {
    SET_ERROR(err,
              -1,
              "g_module_open %s failed, %s",
              filename,
              g_module_error());
    g_clear_pointer(&filename, g_free);
    return NULL;
  }

  p = g_slice_new0(opc_plugin_t);

  p->module = module;
  p->filename = filename;

  g_module_symbol(p->module, "opc_ua_create", (gpointer *) &p->fs.ua_create);
  g_module_symbol(p->module, "opc_ua_destroy", (gpointer *) &p->fs.ua_destroy);
  g_module_symbol(p->module,
                  "opc_ua_get_plugin_name",
                  (gpointer *) &p->fs.ua_get_plugin_name);

  if (p->fs.ua_create == NULL || p->fs.ua_destroy == NULL ||
      p->fs.ua_get_plugin_name == NULL) {
    SET_ERROR(err, -1, "Plugin setup failure");
    if (!g_module_close(p->module)) {
      LOG_W(logger, "Failed to unload %s: %s", p->filename, g_module_error());
    }

    g_clear_pointer(&p->filename, g_free);
    p->module = NULL;
    g_slice_free(opc_plugin_t, p);
    return NULL;
  }

  return p;
}

void
plugin_unload(opc_plugin_t *plugin, UA_Logger *logger)
{
  if (plugin == NULL) {
    return;
  }

  g_return_if_fail(logger != NULL);
  g_return_if_fail(plugin->module != NULL);
  g_return_if_fail(plugin->filename != NULL);

  if (!g_module_close(plugin->module)) {
    LOG_W(logger,
          "Failed to unload '%s': %s",
          plugin->filename,
          g_module_error());
  }

  g_clear_pointer(&plugin->filename, g_free);
  g_slice_free(opc_plugin_t, plugin);
}
