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

#ifndef __VAPIX_UTILS_H__
#define __VAPIX_UTILS_H__

#include <curl/curl.h>
#include <glib.h>

/* HTTP request methods we use for VAPIX APIs */
typedef enum {
  HTTP_GET,
  HTTP_POST,
} HTTP_req_method_t;

typedef enum { NONE_data, XML_data, JSON_data } HTTP_media_t;

/**
 * vapix_get_credentials:
 * @username: a user name which will be used to get credentials for to perform
 *    VAPIX calls
 * @err: return location for a #GError
 *
 * Returns the required credentials for @username to perform VAPIX calls.
 *
 * Returns: a newly-allocated string holding the credentials on success, NULL
 *    if @err is set.
 */
gchar *
vapix_get_credentials(const gchar *username, GError **err);

/**
 * vapix_request:
 * @handle: a cURL handle obtained with curl_easy_init()
 * @credentials: credentials string obtained via vapix_get_credentials()
 * @endpoint: the endpoint part of the VAPIX API
 * @req_type: HTTP_GET or HTTP_POST
 * @post_req: NULL if @req_type is HTTP_GET or a string holding the POST data
 *    if @req_type is HTTP_POST
 * @err: return location for a #GError
 *
 * Performs a VAPIX API request.
 *
 * Returns: a newly-allocated string holding the VAPIX response on success, NULL
 *    if @err is set.
 */
gchar *
vapix_request(CURL *handle,
              const gchar *credentials,
              const gchar *endpoint,
              HTTP_req_method_t req_type,
              HTTP_media_t media_type,
              const gchar *post_req,
              GError **err);

#endif /* __VAPIX_UTILS_H__ */
