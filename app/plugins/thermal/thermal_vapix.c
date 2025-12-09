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

#include "error.h"
#include "log.h"
#include "thermal_vapix.h"
#include "vapix_utils.h"

#define THERMOMETRY_API_VERSION  "1.2"
#define THERMOMETRY_CGI_ENDPOINT "thermometry.cgi"

#define GET_SUPPORTED_VERSIONS_REQUEST                                         \
  "{ \"method\" : \"getSupportedVersions\" }"
#define LIST_AREAS_REQUEST                                                     \
  "{"                                                                          \
  " \"apiVersion\" : \"" THERMOMETRY_API_VERSION "\","                         \
  " \"method\": \"listAreas\","                                                \
  " \"params\":{\"presetNbr\":0}"                                              \
  "}"
#define GET_AREA_STATUS_REQUEST                                                \
  "{"                                                                          \
  " \"apiVersion\" : \"" THERMOMETRY_API_VERSION "\","                         \
  " \"method\": \"getAreaStatus\","                                            \
  " \"params\":{}"                                                             \
  "}"
#define SET_SCALE_REQUEST                                                      \
  "{"                                                                          \
  " \"apiVersion\" : \"" THERMOMETRY_API_VERSION "\","                         \
  " \"method\": \"setTemperatureScale\","                                      \
  " \"params\":{ "                                                             \
  " \"unit\": \"%s\" }"                                                        \
  "}"

DEFINE_GQUARK("opc-thermal-vapix-plugin");

gboolean
vapix_get_supported_versions(gchar *credentials, CURL *curl_h, GError **err)
{
  gchar *response;
  json_t *json_response;
  json_error_t json_error;
  gboolean retval = FALSE;
  const gchar *fmt_str = "{s:{s:o}}";
  json_t *api_versions;
  json_t *version;
  gsize index;

  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  response = vapix_request(curl_h,
                           credentials,
                           THERMOMETRY_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           GET_SUPPORTED_VERSIONS_REQUEST,
                           err);

  if (response == NULL) {
    g_prefix_error(err, "vapix call: 'getSupportedVersions' failed: ");
    return FALSE;
  }

  json_response = json_loads(response, 0, &json_error);

  if (json_response == NULL) {
    SET_ERROR(err,
              -1,
              "json_loads() failed: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
    retval = FALSE;
    goto out;
  }

  if (json_unpack_ex(json_response,
                     &json_error,
                     0,
                     fmt_str,
                     "data",
                     "apiVersions",
                     &api_versions) != 0) {
    SET_ERROR(err,
              -1,
              "json_unpack_ex() failed: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
    retval = FALSE;
    goto out;
  }

  json_array_foreach(api_versions, index, version)
  {
    gboolean exit = FALSE;
    gint major1;
    gint major2;
    gint minor1;
    gint minor2;
    gchar **version_in_split = g_strsplit(json_string_value(version), ".", 2);
    gchar **version_split = g_strsplit(THERMOMETRY_API_VERSION, ".", 2);

    if (version_in_split == NULL || version_split == NULL ||
        g_strv_length(version_in_split) < 2 ||
        g_strv_length(version_split) < 2) {
      SET_ERROR(err, -1, "Invalid api version format");
      exit = TRUE;
      goto cleanup;
    }

    major1 = g_ascii_strtoll(version_split[0], NULL, 10);
    major2 = g_ascii_strtoll(version_in_split[0], NULL, 10);
    minor1 = g_ascii_strtoll(version_split[1], NULL, 10);
    minor2 = g_ascii_strtoll(version_in_split[1], NULL, 10);

    if (major1 == major2 && minor1 <= minor2) {
      retval = TRUE;
      exit = TRUE;
    }

cleanup:
    g_clear_pointer(&version_in_split, g_strfreev);
    g_clear_pointer(&version_split, g_strfreev);
    if (exit) {
      break;
    }
  }

  if (!retval && *err == NULL) {
    SET_ERROR(err,
              -1,
              "Api version - %s is not supported on this device.",
              THERMOMETRY_API_VERSION);
  }

out:
  g_clear_pointer(&response, g_free);
  g_clear_pointer(&json_response, json_decref);

  return retval;
}

gboolean
vapix_get_thermal_areas(gchar *credentials,
                        CURL *curl_h,
                        GList **areas,
                        GError **err)
{
  json_t *area;
  json_t *area_list;
  gsize index;
  json_error_t json_error;
  json_t *json_response;
  gchar *response;
  gboolean retval = TRUE;

  const gchar *area_fmt = "{s:i, s:b, s:s, s:s, s:s, s:i, s:i}";
  const gchar *fmt_string = "{s:{s:o}}";

  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(areas != NULL, FALSE);
  g_return_val_if_fail(*areas == NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  response = vapix_request(curl_h,
                           credentials,
                           THERMOMETRY_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           LIST_AREAS_REQUEST,
                           err);

  if (response == NULL) {
    g_prefix_error(err, "Failed to list thermal areas: ");
    retval = FALSE;
    goto out;
  }

  json_response = json_loads(response, 0, &json_error);

  if (json_response == NULL) {
    SET_ERROR(err,
              -1,
              "Invalid json response: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
    retval = FALSE;
    goto out;
  }

  /* clang-format off */
  if (json_unpack_ex(json_response,
                     &json_error,
                     0,
                     fmt_string,
                     "data",
                     "arealist", &area_list) != 0) {
    /* clang-format on */
    SET_ERROR(err,
              -1,
              "json_unpack_ex() failed: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
    retval = FALSE;
    goto out;
  }

  json_array_foreach(area_list, index, area)
  {
    gchar *name;
    gchar *detectionType;
    gchar *measurement;
    thermal_area_t *values = g_new(thermal_area_t, 1);

    /* clang-format off */
    if (json_unpack_ex(area,
                       &json_error,
                       0,
                       area_fmt,
                       "id", &values->id,
                       "enabled", &values->enabled,
                       "name", &name,
                       "detectionType", &detectionType,
                       "measurement", &measurement,
                       "threshold", &values->threshold,
                       "presetNbr" , &values->presetNbr) != 0) {
      /* clang-format on */
      SET_ERROR(err,
                -1,
                "json_unpack_ex() failed: %d/%d/%d - %s",
                json_error.line,
                json_error.column,
                json_error.position,
                json_error.text);
      retval = FALSE;
      goto out;
    }

    values->name = g_strdup(name);
    values->detectionType = g_strdup(detectionType);
    values->measurement = g_strdup(measurement);

    *areas = g_list_prepend(*areas, values);
  }

out:
  g_clear_pointer(&json_response, json_decref);
  g_clear_pointer(&response, g_free);

  return retval;
}

gboolean
vapix_get_thermal_area_status(gchar *credentials,
                              CURL *curl_h,
                              GList **areas,
                              GError **err)
{
  json_t *area;
  json_t *area_list;
  gsize index;
  json_error_t json_error;
  json_t *json_response;
  gchar *response;
  gboolean ret = TRUE;

  const gchar *area_fmt = "{s:i, s:f, s:f, s:f, s:b}";
  const gchar *fmt_string = "{s:{s:o}}";

  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(areas != NULL, FALSE);
  g_return_val_if_fail(*areas == NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  response = vapix_request(curl_h,
                           credentials,
                           THERMOMETRY_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           GET_AREA_STATUS_REQUEST,
                           err);

  if (response == NULL) {
    g_prefix_error(err, "vapix call: 'getAreaStatus' failed: ");
    ret = FALSE;
    goto err_out;
  }

  json_response = json_loads(response, 0, &json_error);

  if (json_response == NULL) {
    SET_ERROR(err,
              -1,
              "Invalid json response: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
    ret = FALSE;
    goto err_out;
  }

  /* clang-format off */
  if (json_unpack_ex(json_response,
                     &json_error,
                     0,
                     fmt_string,
                     "data",
                     "arealist", &area_list) != 0) {
    SET_ERROR(err, -1, "json_unpack_ex() failed: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
    ret = FALSE;
    goto err_out;
  }
  /* clang-format on */

  json_array_foreach(area_list, index, area)
  {
    thermal_area_values_t *values = g_new(thermal_area_values_t, 1);

    /* clang-format off */
    if (json_unpack_ex(area,
                       &json_error,
                       0,
                       area_fmt,
                       "id", &values->id,
                       "avg", &values->avg,
                       "min", &values->min,
                       "max", &values->max,
                       "triggered", &values->triggered) != 0) {
      SET_ERROR(err, -1, "json_unpack_ex() failed: %d/%d/%d - %s",
              json_error.line,
              json_error.column,
              json_error.position,
              json_error.text);
      ret = FALSE;
      goto err_out;
    }
    /* clang-format on */
    *areas = g_list_prepend(*areas, values);
  }

err_out:
  g_clear_pointer(&json_response, json_decref);
  g_clear_pointer(&response, g_free);

  return ret;
}

gboolean
vapix_set_temperature_scale(gchar *credentials,
                            CURL *curl_h,
                            gchar *scale,
                            GError **err)
{
  gchar *request;
  gchar *response;
  gboolean retval = TRUE;

  g_return_val_if_fail(credentials != NULL, FALSE);
  g_return_val_if_fail(curl_h != NULL, FALSE);
  g_return_val_if_fail(scale != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  request = g_strdup_printf(SET_SCALE_REQUEST, scale);

  response = vapix_request(curl_h,
                           credentials,
                           THERMOMETRY_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           request,
                           err);

  if (response == NULL) {
    g_prefix_error(err, "vapix call: 'setTemperatureScale' failed: ");
    retval = FALSE;
    goto err_out;
  }

err_out:
  g_clear_pointer(&request, g_free);
  g_clear_pointer(&response, g_free);

  return retval;
}
