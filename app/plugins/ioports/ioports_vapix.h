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

#ifndef __IOPORTS_VAPIX_H__
#define __IOPORTS_VAPIX_H__

#define IO_VAPIX_JSON_ERR     "error"
#define IO_VAPIX_JSON_ERRMSG  "message"
#define IO_VAPIX_JSON_DATA    "data"
#define IO_VAPIX_JSON_NRPORTS "numberOfPorts"
#define IO_VAPIX_JSON_ITEMS   "items"
#define IO_VAPIX_JSON_PORT    "port"
#define IO_VAPIX_JSON_STATE   "state"
#define IO_VAPIX_JSON_CFGABLE "configurable"
#define IO_VAPIX_JSON_RO      "readonly"
#define IO_VAPIX_JSON_USAGE   "usage"
#define IO_VAPIX_JSON_DIR     "direction"
#define IO_VAPIX_JSON_NAME    "name"
#define IO_VAPIX_JSON_NSTATE  "normalState"
#define IO_VAPIX_JSON_APIVER  "apiVersions"

#define IO_VAPIX_DIR_INPUT    "input"
#define IO_VAPIX_DIR_OUTPUT   "output"
#define IO_VAPIX_STATE_OPEN   "open"
#define IO_VAPIX_STATE_CLOSED "closed"

/* structure used to hold a local cache (hash table) with I/O ports state and
 * config information */
typedef struct ioport_obj {
  gboolean configurable;
  gboolean readonly;
  gchar *name;
  gchar *usage;
  UA_IOPortStateType normal_state;
  UA_IOPortStateType state;
  UA_IOPortDirectionType direction;
} ioport_obj_t;

/* Checks if the device supports version `IO_VAPIX_VERSION` of the
 * "portmanagement.cgi" API. */
gboolean
iop_vapix_check_api_ver(CURL *curl_h, const gchar *credentials, GError **err);

/**
 * Calls the 'getPorts' method of the "portmanagement.cgi" API and returns a
 * hash table (iop_ht) with information about the I/O ports found on the device.
 */
gboolean
iop_vapix_get_ports(CURL *curl_h,
                    const gchar *credentials,
                    GHashTable **iop_ht,
                    GError **err);

/**
 * Calls the 'setPorts' method of the "portmanagement.cgi" API. Only one
 * property ('iop_key') of an I/O port can be set ('iop_value') at a time. */
gboolean
iop_vapix_set_port(CURL *curl_h,
                   const gchar *credentials,
                   UA_UInt32 portnr,
                   const gchar *iop_key,
                   const gchar *iop_value,
                   GError **err);

#endif /* __IOPORTS_VAPIX_H__ */
