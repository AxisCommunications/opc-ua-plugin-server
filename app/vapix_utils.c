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
#include <glib.h>

#include "error.h"
#include "vapix_utils.h"

DEFINE_GQUARK("vapix-utils")

#define VAPIX_URL "http://127.0.0.12/axis-cgi/%s"

#define CONF1_DBUS_SERVICE     "com.axis.HTTPConf1"
#define CONF1_DBUS_OBJECT_PATH "/com/axis/HTTPConf1/VAPIXServiceAccounts1"
#define CONF1_DBUS_INTERFACE   "com.axis.HTTPConf1.VAPIXServiceAccounts1"

#define HTTP_HDR_CONTENT "Content-Type"
#define HTTP_HDR_ACCEPT  "Accept"
#define MIME_XML         "application/xml"
#define MIME_JSON        "application/json"

/* Local functions */
static size_t
write_cb(gchar *ptr, size_t size, size_t nmemb, void *userdata)
{
  size_t processed_bytes;

  g_assert(ptr != NULL);
  g_assert(userdata != NULL);

  processed_bytes = size * nmemb;

  g_string_append_len((GString *) userdata, ptr, processed_bytes);

  return processed_bytes;
}

static void
set_curl_setopt_error(CURLcode res, GError **err)
{
  g_assert(err == NULL || *err == NULL);

  SET_ERROR(err,
            -1,
            "curl_easy_setopt error %d: '%s'",
            res,
            curl_easy_strerror(res));
}

static gchar *
parse_credentials(GVariant *result, GError **err)
{
  gchar *v_creds = NULL;
  gchar *credentials = NULL;
  gchar **split = NULL;
  guint len;

  g_assert(result != NULL);
  g_assert(err == NULL || *err == NULL);

  g_variant_get(result, "(&s)", &v_creds);

  split = g_strsplit(v_creds, ":", -1);
  if (split == NULL) {
    SET_ERROR(err, -1, "Error parsing credential string: '%s'", v_creds);
    goto out;
  }

  len = g_strv_length(split);
  if (len != 2) {
    SET_ERROR(err,
              -1,
              "Invalid credential string length (%u): '%s'",
              len,
              v_creds);
    goto out;
  }

  credentials = g_strdup_printf("%s:%s", split[0], split[1]);

out:
  if (split != NULL) {
    g_strfreev(split);
  }

  return credentials;
}

/* Exported functions */
gchar *
vapix_request(CURL *handle,
              const gchar *credentials,
              const gchar *endpoint,
              HTTP_req_method_t req_type,
              HTTP_media_t media_type,
              const gchar *post_req,
              GError **err)
{
  glong code;
  GString *response;
  gchar *vapix_resp = NULL;
  gchar *url = NULL;
  gchar *content_hdr = NULL;
  gchar *accept_hdr = NULL;
  CURLcode res;
  struct curl_slist *headers = NULL;

  g_return_val_if_fail(handle != NULL, NULL);
  g_return_val_if_fail(credentials != NULL, NULL);
  g_return_val_if_fail(endpoint != NULL, NULL);
  g_return_val_if_fail(err == NULL || *err == NULL, NULL);
  g_return_val_if_fail(((req_type == HTTP_GET) && (post_req == NULL)) ||
                               ((req_type == HTTP_POST) && (post_req != NULL)),
                       NULL);

  /* start clean */
  curl_easy_reset(handle);

  url = g_strdup_printf(VAPIX_URL, endpoint);
  response = g_string_new(NULL);

  res = curl_easy_setopt(handle, CURLOPT_URL, url);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_USERPWD, credentials);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  if (req_type == HTTP_POST) {
    /* prepare corresponding HTTP headers */
    if (media_type == XML_data) {
      content_hdr = g_strdup_printf("%s: %s", HTTP_HDR_CONTENT, MIME_XML);
      accept_hdr = g_strdup_printf("%s: %s", HTTP_HDR_ACCEPT, MIME_XML);
    } else if (media_type == JSON_data) {
      content_hdr = g_strdup_printf("%s: %s", HTTP_HDR_CONTENT, MIME_JSON);
      accept_hdr = g_strdup_printf("%s: %s", HTTP_HDR_ACCEPT, MIME_JSON);
    }

    if (content_hdr && accept_hdr) {
      headers = curl_slist_append(headers, content_hdr);
      if (!headers) {
        SET_ERROR(err,
                  -1,
                  "curl_slist_append(): failed adding 'Content-Type:'");
        goto err_out;
      }

      headers = curl_slist_append(headers, accept_hdr);
      if (!headers) {
        SET_ERROR(err, -1, "curl_slist_append(): failed adding 'Accept:'");
        goto err_out;
      }

      res = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
      if (res != CURLE_OK) {
        set_curl_setopt_error(res, err);
        goto err_out;
      }
    }

    res = curl_easy_setopt(handle, CURLOPT_POSTFIELDS, post_req);

    if (res != CURLE_OK) {
      set_curl_setopt_error(res, err);
      goto err_out;
    }
  }

  res = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_cb);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_perform(handle);

  if (res != CURLE_OK) {
    SET_ERROR(err,
              -1,
              "curl_easy_perform error %d: '%s'",
              res,
              curl_easy_strerror(res));

    goto err_out;
  }

  res = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &code);

  if (res != CURLE_OK) {
    SET_ERROR(err,
              -1,
              "curl_easy_getinfo error %d: '%s'",
              res,
              curl_easy_strerror(res));

    goto err_out;
  }

  if (code != 200) {
    SET_ERROR(err,
              -1,
              "Got response code %ld from request to %s with response '%s'",
              code,
              endpoint,
              response->str);
    g_string_free(response, TRUE);
    goto err_out;
  }

  vapix_resp = g_string_free(response, FALSE);

err_out:
  g_clear_pointer(&url, g_free);
  g_clear_pointer(&content_hdr, g_free);
  g_clear_pointer(&accept_hdr, g_free);

  /* handles NULL-case too */
  curl_slist_free_all(headers);

  return vapix_resp;
}

gchar *
vapix_get_credentials(const gchar *username, GError **err)
{
  GDBusConnection *con = NULL;
  GVariant *result = NULL;
  gchar *credentials = NULL;

  g_return_val_if_fail(username != NULL, NULL);
  g_return_val_if_fail(err == NULL || *err == NULL, NULL);

  con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, err);
  if (con == NULL) {
    g_prefix_error(err, "Error connecting to D-Bus: ");
    return NULL;
  }

  result = g_dbus_connection_call_sync(con,
                                       CONF1_DBUS_SERVICE,
                                       CONF1_DBUS_OBJECT_PATH,
                                       CONF1_DBUS_INTERFACE,
                                       "GetCredentials",
                                       g_variant_new("(s)", username),
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       err);
  if (result == NULL) {
    g_prefix_error(err, "Failed to get credentials: ");
    goto out;
  }

  credentials = parse_credentials(result, err);

  if (credentials == NULL) {
    g_prefix_error(err, "parse_credentials() failed: ");
  }

  g_variant_unref(result);

out:
  g_object_unref(con);

  return credentials;
}
