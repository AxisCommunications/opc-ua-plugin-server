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

#include <curl/curl.h>
#include <glib.h>
#include <jansson.h>
#include <open62541/server.h>

#include "error.h"
#include "ioports_ns.h"
#include "ioports_vapix.h"
#include "vapix_utils.h"

DEFINE_GQUARK("ioports-vapix")

#define IO_VAPIX_CGI_ENDPOINT "io/portmanagement.cgi"
#define IO_VAPIX_GET_API_VER  "getSupportedVersions"
#define IO_VAPIX_GET_PORTS    "getPorts"
#define IO_VAPIX_SET_PORTS    "setPorts"
/* the API version we currently support */
#define IO_VAPIX_VERSION "1.1"

/* JSON payload: getSupportedVersions */
#define IO_VAPIX_GET_API_VER_FMT                                               \
  "{"                                                                          \
  "  \"method\": \"" IO_VAPIX_GET_API_VER "\""                                 \
  "}"

/* JSON payload: getPorts */
#define IO_VAPIX_GET_PORTS_FMT                                                 \
  "{"                                                                          \
  "  \"apiVersion\": \"" IO_VAPIX_VERSION "\","                                \
  "  \"method\": \"" IO_VAPIX_GET_PORTS "\""                                   \
  "}"

/* JSON payload: setPorts
 * NOTE: we do a single property for a single port at time with this request
 *       %d - port number
 *       %s - key (name/usage/direction/state/normalState)
 *       %s - value
 **/
#define IO_VAPIX_SET_PORT_FMT                                                  \
  "{"                                                                          \
  "  \"apiVersion\": \"" IO_VAPIX_VERSION "\","                                \
  "  \"method\": \"" IO_VAPIX_SET_PORTS "\","                                  \
  "  \"params\": {"                                                            \
  "               \"ports\": ["                                                \
  "                            {"                                              \
  "                              \"port\": \"%d\","                            \
  "                              \"%s\":   \"%s\""                             \
  "                            }"                                              \
  "                          ]"                                                \
  "              }"                                                            \
  "}"

/* clang-format off */
typedef enum json_key_type {
  J_STRING,
  J_BOOLEAN
} json_type_t;
/* clang-format on */

typedef struct port_json_obj {
  const gchar *jkey;
  json_type_t jtype;
  const gchar *browse_name;
} port_json_t;

/* frees up an 'ioport_obj_t' structure pointed to by 'data' */
static void
free_ioport_obj(gpointer data)
{
  ioport_obj_t *iop = data;

  if (iop == NULL) {
    return;
  }

  g_clear_pointer(&iop->name, g_free);
  g_clear_pointer(&iop->usage, g_free);
  g_clear_pointer(&iop, g_free);

  return;
}

gboolean
iop_vapix_check_api_ver(CURL *curl_h, const gchar *credentials, GError **err)
{
  gboolean found = FALSE;
  gchar *response;
  json_error_t parse_err;
  json_t *json_response = NULL;

  json_t *json_err = NULL;
  json_t *json_err_msg = NULL;

  json_t *data;
  json_t *api_versions;
  json_t *version;
  size_t size;
  guint i = 0;

  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  response = vapix_request(curl_h,
                           credentials,
                           IO_VAPIX_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           IO_VAPIX_GET_API_VER_FMT,
                           err);
  if (response == NULL) {
    g_prefix_error(err,
                   "Failed to get %s API versions: ",
                   IO_VAPIX_CGI_ENDPOINT);
    goto err_out;
  }

  json_response = json_loads(response, 0, &parse_err);
  if (json_response == NULL) {
    SET_ERROR(err,
              -1,
              "Invalid JSON response: L:%d/C:%d: %s",
              parse_err.line,
              parse_err.column,
              parse_err.text);
    goto err_out;
  }

  /* is this an "error" response? */
  json_err = json_object_get(json_response, IO_VAPIX_JSON_ERR);
  if (json_err) {
    json_err_msg = json_object_get(json_err, IO_VAPIX_JSON_ERRMSG);
    if (json_err_msg) {
      SET_ERROR(err,
                -1,
                "'%s' error: %s",
                IO_VAPIX_GET_API_VER,
                json_string_value(json_err_msg));
    } else {
      SET_ERROR(err, -1, "'%s': unknown error", IO_VAPIX_GET_API_VER);
    }
    goto err_out;
  }

  data = json_object_get(json_response, IO_VAPIX_JSON_DATA);
  if (data == NULL) {
    SET_ERROR(err, -1, "No '%s' key in response", IO_VAPIX_JSON_DATA);
    goto err_out;
  }

  api_versions = json_object_get(data, IO_VAPIX_JSON_APIVER);
  if (api_versions == NULL) {
    SET_ERROR(err, -1, "No '%s' key in response", IO_VAPIX_JSON_APIVER);
    goto err_out;
  }

  if (!json_is_array(api_versions)) {
    SET_ERROR(err, -1, "No valid '%s' in response", IO_VAPIX_JSON_APIVER);
    goto err_out;
  }

  size = json_array_size(api_versions);
  if (size < 1) {
    SET_ERROR(err, -1, "No supported version in response");
    goto err_out;
  }

  /* Iterate over the supported versions array and check if the version we
   * support is found. */
  while (!found && (i < size)) {
    version = json_array_get(api_versions, i);
    if (!json_is_string(version)) {
      SET_ERROR(err,
                -1,
                "Bad version format in '%s', index: %d",
                IO_VAPIX_JSON_APIVER,
                i);
      goto err_out;
    }

    if (g_strcmp0(IO_VAPIX_VERSION, json_string_value(version)) == 0) {
      /* our supported IO_VAPIX_VERSION was found */
      found = TRUE;
    }

    i++;
  }

  if (!found) {
    SET_ERROR(err,
              -1,
              "%s ver. %s is not supported by the device.",
              IO_VAPIX_CGI_ENDPOINT,
              IO_VAPIX_VERSION);
  }

err_out:
  g_clear_pointer(&response, g_free);
  g_clear_pointer(&json_response, json_decref);

  return found;
}

gboolean
iop_vapix_get_ports(CURL *curl_h,
                    const gchar *credentials,
                    GHashTable **iop_ht,
                    GError **err)
{
  gboolean retv = FALSE;
  gchar *response;
  /* clang-format off */
  const port_json_t port_map[IOP_OBJ_NR_PROPS] = {
  /*  JSON key,              JSON type, Browse name  */
    { IO_VAPIX_JSON_PORT,    J_STRING,  "Index"       },
    { IO_VAPIX_JSON_CFGABLE, J_BOOLEAN, "Configurable"},
    { IO_VAPIX_JSON_USAGE,   J_STRING,  "Usage"       },
    { IO_VAPIX_JSON_NAME,    J_STRING,  "Name"        },
    { IO_VAPIX_JSON_DIR,     J_STRING,  "Direction"   },
    { IO_VAPIX_JSON_STATE,   J_STRING,  "State"       },
    { IO_VAPIX_JSON_NSTATE,  J_STRING,  "NormalState" },
    /* NOTE: 'readonly' property only present in the JSON payload if its value
     * is TRUE. This entry must be _last_ in this table! */
    { IO_VAPIX_JSON_RO,      J_BOOLEAN, "Disabled"    },
  };
  /* clang-format on */

  /* key in iop_ht */
  guint64 *iop_nr;
  /* value in iop_ht */
  ioport_obj_t *iop;

  json_error_t parse_err;
  json_t *json_response = NULL;
  json_t *json_err = NULL;
  json_error_t jerr;
  json_t *json_err_msg = NULL;
  json_t *data;
  json_t *json_nrports = NULL;
  json_t *items = NULL;
  json_t *key;

  guint nr_ports;
  size_t size;
  size_t i, j;
  json_t *port_item;

  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(iop_ht == NULL || *iop_ht == NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  response = vapix_request(curl_h,
                           credentials,
                           IO_VAPIX_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           IO_VAPIX_GET_PORTS_FMT,
                           err);
  if (response == NULL) {
    g_prefix_error(err, "'%s' failed: ", IO_VAPIX_GET_PORTS);
    goto err_out;
  }

  json_response = json_loads(response, 0, &parse_err);
  if (json_response == NULL) {
    SET_ERROR(err,
              -1,
              "Invalid JSON response: L:%d/C:%d: %s",
              parse_err.line,
              parse_err.column,
              parse_err.text);
    goto err_out;
  }

  /* is this an "error" response? */
  json_err = json_object_get(json_response, IO_VAPIX_JSON_ERR);
  if (json_err) {
    json_err_msg = json_object_get(json_err, IO_VAPIX_JSON_ERRMSG);
    if (json_err_msg) {
      SET_ERROR(err,
                -1,
                "'%s' error: %s",
                IO_VAPIX_GET_PORTS,
                json_string_value(json_err_msg));
    } else {
      SET_ERROR(err, -1, "'%s': unknown error", IO_VAPIX_GET_PORTS);
    }
    goto err_out;
  }

  data = json_object_get(json_response, IO_VAPIX_JSON_DATA);
  if (data == NULL) {
    SET_ERROR(err, -1, "No '%s' key in response", IO_VAPIX_JSON_DATA);
    goto err_out;
  }

  if (json_unpack_ex(data,
                     &jerr,
                     JSON_VALIDATE_ONLY,
                     "{s:i,s:[o]}",
                     IO_VAPIX_JSON_NRPORTS,
                     IO_VAPIX_JSON_ITEMS) != 0) {
    SET_ERROR(err,
              -1,
              "Invalid JSON data: L:%d/C:%d: %s",
              jerr.line,
              jerr.column,
              jerr.text);
    goto err_out;
  }

  json_nrports = json_object_get(data, IO_VAPIX_JSON_NRPORTS);
  if (json_nrports == NULL) {
    SET_ERROR(err, -1, "No '%s' key in respose", IO_VAPIX_JSON_NRPORTS);
    goto err_out;
  }

  if (json_typeof(json_nrports) != JSON_INTEGER) {
    SET_ERROR(err, -1, "'%s' not an integer", IO_VAPIX_JSON_NRPORTS);
    goto err_out;
  }

  nr_ports = json_integer_value(json_nrports);

  items = json_object_get(data, IO_VAPIX_JSON_ITEMS);
  if (items == NULL) {
    SET_ERROR(err, -1, "No '%s' key in response", IO_VAPIX_JSON_ITEMS);
    goto err_out;
  }

  if (!json_is_array(items)) {
    SET_ERROR(err, -1, "No valid port items in response");
    goto err_out;
  }

  size = json_array_size(items);
  if (size != nr_ports) {
    SET_ERROR(err,
              -1,
              "Ports array size: %zu mismatches '%s': %d",
              size,
              IO_VAPIX_JSON_NRPORTS,
              nr_ports);
    goto err_out;
  }

  *iop_ht = g_hash_table_new_full(g_int_hash,
                                  g_int_equal,
                                  g_free,
                                  free_ioport_obj);

  /* loop over the array of JSON port objects */
  for (i = 0; i < size; i++) {
    port_item = json_array_get(items, i);

    if (json_is_object(port_item)) {
      key = NULL;
      iop_nr = g_new0(guint64, 1);
      iop = g_new0(ioport_obj_t, 1);

      /* FALSE per default, the JSON key will be present and set to TRUE if
       * the I/O port is in fact configured as disabled/read-only. */
      iop->readonly = FALSE;

      /* loop over all the properties of this JSON port object */
      for (j = 0; j < IOP_OBJ_NR_PROPS; j++) {
        key = json_object_get(port_item, port_map[j].jkey);
        if (key) {
          if (port_map[j].jtype == J_STRING) {
            if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_PORT) == 0) {
              /* JSON: port */
              *iop_nr = g_ascii_strtoll(json_string_value(key), NULL, 10);
            } else if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_USAGE) == 0) {
              /* JSON: usage */
              iop->usage = g_strdup(json_string_value(key));
            } else if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_NAME) == 0) {
              /* JSON: name */
              iop->name = g_strdup(json_string_value(key));
            } else if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_DIR) == 0) {
              /* JSON: direction */
              if (g_strcmp0(json_string_value(key), IO_VAPIX_DIR_INPUT) == 0) {
                iop->direction = UA_IOPORTDIRECTIONTYPE_INPUT;
              } else {
                iop->direction = UA_IOPORTDIRECTIONTYPE_OUTPUT;
              }
            } else if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_STATE) == 0) {
              /* JSON: state */
              if (g_strcmp0(json_string_value(key), IO_VAPIX_STATE_OPEN) == 0) {
                iop->state = UA_IOPORTSTATETYPE_OPEN;
              } else {
                iop->state = UA_IOPORTSTATETYPE_CLOSED;
              }
            } else if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_NSTATE) == 0) {
              /* JSON: normalState */
              if (g_strcmp0(json_string_value(key), IO_VAPIX_STATE_OPEN) == 0) {
                iop->normal_state = UA_IOPORTSTATETYPE_OPEN;
              } else {
                iop->normal_state = UA_IOPORTSTATETYPE_CLOSED;
              }
            }
          }

          if (port_map[j].jtype == J_BOOLEAN) {
            if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_CFGABLE) == 0) {
              /* JSON: configurable */
              iop->configurable = json_boolean_value(key);
            } else if (g_strcmp0(port_map[j].jkey, IO_VAPIX_JSON_RO) == 0) {
              /* JSON: readonly */
              iop->readonly = json_boolean_value(key);
            }
          }
        } else {
          if (j < IOP_OBJ_NR_PROPS - 1) {
            /* We expect all the port properties (JSON port object keys) to be
             * present except the 'readonly' property/key which _is_ allowed to
             * be missing */
            SET_ERROR(err,
                      -1,
                      "port: %zu missing property: '%s'",
                      i,
                      port_map[j].jkey);

            /* free the pre-allocated key/value pair for the hash table */
            g_clear_pointer(&iop_nr, g_free);
            g_clear_pointer(&iop, g_free);

            /* free the potentially allocated resources in the hash table */
            g_hash_table_destroy(*iop_ht);
            *iop_ht = NULL;

            goto err_out;
          }
        }
      } /* for loop through port properties [index, name, usage, dir...] */

      g_hash_table_insert(*iop_ht, iop_nr, iop);

    } else {
      SET_ERROR(err, -1, "Not a JSON object at index: %zu in ports array", i);
      /* free the potentially allocated resources in the hash table */
      g_hash_table_destroy(*iop_ht);
      *iop_ht = NULL;
      goto err_out;
    }
  } /* for loop through port objects in the array of returned ports */

  retv = TRUE;

err_out:
  g_clear_pointer(&response, g_free);
  g_clear_pointer(&json_response, json_decref);

  return retv;
}

gboolean
iop_vapix_set_port(CURL *curl_h,
                   const gchar *credentials,
                   UA_UInt32 portnr,
                   const gchar *iop_key,
                   const gchar *iop_value,
                   GError **err)
{
  /* properties supported by the 'setPorts' API */
  /* clang-format off */
  const gchar *port_props[] = {
    IO_VAPIX_JSON_PORT,
    IO_VAPIX_JSON_USAGE,
    IO_VAPIX_JSON_DIR,
    IO_VAPIX_JSON_NAME,
    IO_VAPIX_JSON_NSTATE,
    IO_VAPIX_JSON_STATE,
    NULL
  };
  /* clang-format on */
  guint i = 0;
  const gchar *property = port_props[i];
  gboolean found = FALSE;
  gboolean retv = FALSE;
  gchar *response;
  gchar *request = NULL;
  json_error_t parse_err;
  json_t *json_response = NULL;
  json_t *json_err = NULL;
  json_t *json_err_msg = NULL;
  json_t *data;

  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(iop_key != NULL, FALSE);
  g_return_val_if_fail(iop_value != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  /* NOTE: the portmanagement.cgi doesn't do a very good job at properly
   * validating the properties to be set via 'setPort'. It can return success
   * even if for example 'name' is misspelled as 'nameee'. At least the property
   * values are properly validated but we better do our own input validation of
   * the keys/properties. */
  while ((property != NULL) && !found) {
    if (g_strcmp0(iop_key, property) == 0) {
      found = TRUE;
    } else {
      property = port_props[++i];
    }
  }

  if (!found) {
    SET_ERROR(err, -1, "Invalid port property: \"%s\"!", iop_key);
    return FALSE;
  }

  request = g_strdup_printf(IO_VAPIX_SET_PORT_FMT, portnr, iop_key, iop_value);
  response = vapix_request(curl_h,
                           credentials,
                           IO_VAPIX_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           request,
                           err);
  g_clear_pointer(&request, g_free);
  if (response == NULL) {
    g_prefix_error(err, "'%s' failed: ", IO_VAPIX_SET_PORTS);
    goto err_out;
  }

  json_response = json_loads(response, 0, &parse_err);
  if (json_response == NULL) {
    SET_ERROR(err,
              -1,
              "Invalid JSON response: L:%d/C:%d: %s",
              parse_err.line,
              parse_err.column,
              parse_err.text);
    goto err_out;
  }

  /* is this an "error" response? */
  json_err = json_object_get(json_response, IO_VAPIX_JSON_ERR);
  if (json_err) {
    json_err_msg = json_object_get(json_err, IO_VAPIX_JSON_ERRMSG);
    if (json_err_msg) {
      SET_ERROR(err,
                -1,
                "'%s' error: %s",
                IO_VAPIX_SET_PORTS,
                json_string_value(json_err_msg));
    } else {
      SET_ERROR(err, -1, "'%s': unknown error", IO_VAPIX_SET_PORTS);
    }
    goto err_out;
  }

  data = json_object_get(json_response, IO_VAPIX_JSON_DATA);
  if (data == NULL) {
    SET_ERROR(err, -1, "No '%s' key in response", IO_VAPIX_JSON_DATA);
    goto err_out;
  }

  retv = TRUE;

err_out:
  g_clear_pointer(&response, g_free);
  g_clear_pointer(&json_response, json_decref);

  return retv;
}
