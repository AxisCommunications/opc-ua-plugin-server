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

#ifndef __VINPUT_VAPIX_H__
#define __VINPUT_VAPIX_H__

#define VINPUT_ACTIVATE_CGI_ENDPOINT   "virtualinput/activate.cgi"
#define VINPUT_DEACTIVATE_CGI_ENDPOINT "virtualinput/deactivate.cgi"
#define VINPUT_SCHEMA_CGI_ENDPOINT     "virtualinput/getschemaversions.cgi"
#define VINPUT_VAPIX_CGI_PARAMS        "schemaversion=%s&port=%d"
#define VINPUT_VAPIX_CGI_PARAMS_FULL   "schemaversion=%s&port=%d&duration=%d"

/* helper structure to interpret the result of a VAPIX call */
typedef struct parser_status {
  /* bitmask with status bits */
  guint vapix_mask;
  /* the value of <StateChanged> */
  gboolean state_changed;
  /* the value of <MajorVersion> */
  gchar *schema_version;
  /* the value of <ErrorDescription>, if any */
  gchar *error_descr;

  /* Points to the current XML tag of interest while parsing to help
   * interpreting the text. Set to NULL after parsing is finished. */
  const gchar *element;
} parser_status_t;

gchar *
vin_get_schema_version(CURL *curl_h, const gchar *credentials, GError **err);

/* NOTE: "duration" (in seconds) is an optional parameter of the "activate.cgi"
 * request. OPC UA methods cannot take optional parameters. We use the
 * convention that a negative "duration" value will be ignored.
 * See:
 * https://developer.axis.com/vapix/network-video/input-and-outputs/#activate-a-virtual-input:
 * */
UA_StatusCode
vin_set_port_state(CURL *curl_h,
                   const gchar *credentials,
                   const gchar *schema_version,
                   UA_UInt32 portnr,
                   UA_Boolean state,
                   UA_Int32 duration,
                   gboolean *vin_states,
                   gboolean *state_changed,
                   GError **err);

#endif /* __VINPUT_VAPIX_H__ */
