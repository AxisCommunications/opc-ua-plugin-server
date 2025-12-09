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

#include <glib.h>
#include <open62541/server.h>

#include "error.h"
#include "vapix_utils.h"
#include "vinput_vapix.h"

/* XML tags */
#define VINPUT_XML_TAG_RESP            "VirtualInputResponse"
#define VINPUT_XML_TAG_SUCCESS         "Success"
#define VINPUT_XML_TAG_ERROR           "Error"
#define VINPUT_XML_TAG_ERROR_DESC      "ErrorDescription"
#define VINPUT_XML_TAG_SCHVER          "SchemaVersion"
#define VINPUT_XML_TAG_MAJVER          "MajorVersion"
#define VINPUT_XML_TAG_ACTIVATE_SUCC   "ActivateSuccess"
#define VINPUT_XML_TAG_DEACTIVATE_SUCC "DeactivateSuccess"
#define VINPUT_XML_TAG_STATE_CHNG      "StateChanged"
#define VINPUT_XML_TXT_TRUE            "true"
#define VINPUT_XML_TXT_FALSE           "false"

/* bitmask positions */
#define VAPIX_VIN_RESP   (1 << 0)
#define VAPIX_SUCCESS    (1 << 1)
#define VAPIX_SCHEMA     (1 << 2)
#define VAPIX_ACTIVATE   (1 << 3)
#define VAPIX_DEACTIVATE (1 << 4)
#define VAPIX_ERR        (1 << 5)

DEFINE_GQUARK("vinput-vapix")

static void
vin_xml_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                      const gchar *element_name,
                      G_GNUC_UNUSED const gchar **attribute_names,
                      G_GNUC_UNUSED const gchar **attribute_values,
                      gpointer user_data,
                      G_GNUC_UNUSED GError **err)
{
  parser_status_t *pst;

  g_assert(element_name != NULL);
  g_assert(user_data != NULL);

  pst = user_data;

  if (g_strcmp0(element_name, VINPUT_XML_TAG_RESP) == 0) {
    pst->element = VINPUT_XML_TAG_RESP;
    pst->vapix_mask |= VAPIX_VIN_RESP;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_ERROR) == 0) {
    pst->element = VINPUT_XML_TAG_ERROR;
    pst->vapix_mask |= VAPIX_ERR;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_ERROR_DESC) == 0) {
    pst->element = VINPUT_XML_TAG_ERROR_DESC;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_SUCCESS) == 0) {
    pst->element = VINPUT_XML_TAG_SUCCESS;
    pst->vapix_mask |= VAPIX_SUCCESS;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_SCHVER) == 0) {
    pst->element = VINPUT_XML_TAG_SCHVER;
    pst->vapix_mask |= VAPIX_SCHEMA;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_MAJVER) == 0) {
    pst->element = VINPUT_XML_TAG_MAJVER;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_ACTIVATE_SUCC) == 0) {
    pst->element = VINPUT_XML_TAG_ACTIVATE_SUCC;
    pst->vapix_mask |= VAPIX_ACTIVATE;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_DEACTIVATE_SUCC) == 0) {
    pst->element = VINPUT_XML_TAG_DEACTIVATE_SUCC;
    pst->vapix_mask |= VAPIX_DEACTIVATE;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_STATE_CHNG) == 0) {
    pst->element = VINPUT_XML_TAG_STATE_CHNG;
  }
}

static void
vin_xml_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                    const gchar *element_name,
                    gpointer user_data,
                    G_GNUC_UNUSED GError **err)
{
  parser_status_t *pst;

  g_assert(element_name != NULL);
  g_assert(user_data != NULL);

  pst = user_data;

  if (g_strcmp0(element_name, VINPUT_XML_TAG_RESP) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_SUCCESS) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_ERROR) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_ERROR_DESC) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_SCHVER) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_MAJVER) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_ACTIVATE_SUCC) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_DEACTIVATE_SUCC) == 0) {
    pst->element = NULL;
  } else if (g_strcmp0(element_name, VINPUT_XML_TAG_STATE_CHNG) == 0) {
    pst->element = NULL;
  }
}

static void
vin_xml_text(G_GNUC_UNUSED GMarkupParseContext *context,
             const gchar *text,
             gsize text_len,
             gpointer user_data,
             GError **err)
{
  gchar *buf = NULL;
  parser_status_t *pst;

  g_assert(text != NULL);
  g_assert(user_data != NULL);
  g_assert(err == NULL || *err == NULL);

  /* strip any leading or trailing whitespace of data/value: we need a
   * modifiable copy of 'text' allocated dynamically */
  buf = g_strndup(text, text_len);
  if (buf) {
    buf = g_strstrip(buf);
  }

  pst = user_data;

  if ((g_strcmp0(pst->element, VINPUT_XML_TAG_ERROR_DESC) == 0) &&
      (pst->vapix_mask & VAPIX_VIN_RESP) && (pst->vapix_mask & VAPIX_ERR)) {
    /* the VAPIX request returned an <Error> response,
     * save the <ErrorDescription> */
    if (strlen(buf) > 0) {
      /* NOTE: needs to be freed with vin_xml_parse_clear() */
      pst->error_descr = g_strdup(buf);
    } else {
      /* this is a parsing error */
      SET_ERROR(err, -1, "<%s>: missing value", VINPUT_XML_TAG_ERROR_DESC);
    }
  } else if ((g_strcmp0(pst->element, VINPUT_XML_TAG_MAJVER) == 0) &&
             (pst->vapix_mask & VAPIX_VIN_RESP) &&
             (pst->vapix_mask & VAPIX_SUCCESS) &&
             (pst->vapix_mask & VAPIX_SCHEMA)) {
    /* current element is <MajorVersion> and prior expected tags found: save
     * the <MajorVersion> of <SchemaVersion> */

    if (strlen(buf) > 0) {
      /* NOTE: needs to be freed with vin_xml_parse_clear() */
      pst->schema_version = g_strdup(buf);
    } else {
      /* this is a parsing error */
      SET_ERROR(err, -1, "<%s>: missing value", VINPUT_XML_TAG_MAJVER);
    }

  } else if ((g_strcmp0(pst->element, VINPUT_XML_TAG_STATE_CHNG) == 0) &&
             (pst->vapix_mask & VAPIX_VIN_RESP) &&
             (pst->vapix_mask & VAPIX_SUCCESS)) {
    /* check if the current element is <StateChanged> */

    if ((pst->vapix_mask & VAPIX_ACTIVATE) ||
        (pst->vapix_mask & VAPIX_DEACTIVATE)) {
      /* if <ActivateSuccess> or <DeactivateSuccess> */

      /* save the value of <StateChanged> */
      if (g_strcmp0(buf, VINPUT_XML_TXT_TRUE) == 0) {
        pst->state_changed = TRUE;
      } else if (g_strcmp0(buf, VINPUT_XML_TXT_FALSE) == 0) {
        pst->state_changed = FALSE;
      } else {
        /* this is a parsing error */
        /* unexpected: the value was neither 'true' nor 'false' */
        SET_ERROR(err, -1, "<%s>: unexpected value", VINPUT_XML_TAG_STATE_CHNG);
      }
    } /* <ActivateSuccess> || <DeactivateSuccess>*/
  }   /* <StateChanged> */

  g_clear_pointer(&buf, g_free);

  return;
}

static gboolean
vin_xml_parse(const gchar *xml_txt, parser_status_t *result, GError **err)
{
  GMarkupParseContext *parse_ctx = NULL;
  /* clang-format off */
  GMarkupParser vinput_xml_parser = {
    .start_element = vin_xml_start_element,
    .end_element = vin_xml_end_element,
    .text = vin_xml_text,
    .passthrough = NULL,
    .error = NULL,
  };
  /* clang-format on */
  gboolean res = FALSE;

  g_return_val_if_fail(xml_txt != NULL, FALSE);
  g_return_val_if_fail(result != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  parse_ctx = g_markup_parse_context_new(&vinput_xml_parser, 0, result, NULL);
  if (parse_ctx == NULL) {
    SET_ERROR(err, -1, "g_markup_parse_context_new() failed");
    return res;
  }

  res = g_markup_parse_context_parse(parse_ctx, xml_txt, strlen(xml_txt), err);
  if (!res) {
    g_prefix_error(err, "g_markup_parse_context_parse(): ");
    goto err_out;
  }

  res = g_markup_parse_context_end_parse(parse_ctx, err);
  if (!res) {
    g_prefix_error(err, "g_markup_parse_context_end_parse(): ");
    goto err_out;
  }

err_out:
  g_markup_parse_context_free(parse_ctx);

  return res;
}

/* Must be called after each call to vin_xml_parse() to free parsing result */
static void
vin_xml_parse_clear(parser_status_t *parser_status)
{
  g_return_if_fail(parser_status != NULL);

  g_clear_pointer(&parser_status->schema_version, g_free);
  g_clear_pointer(&parser_status->error_descr, g_free);
}

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
                   GError **err)
{
  UA_StatusCode ua_status = UA_STATUSCODE_BAD;
  gchar *vapix_req = NULL;
  gchar *vapix_params = NULL;
  gchar *response = NULL;
  parser_status_t parse_res = { 0 };

  g_return_val_if_fail(curl_h != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(credentials != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(schema_version != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(vin_states != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(state_changed != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(err == NULL || *err == NULL, UA_STATUSCODE_BAD);

  if (state && (duration >= 0)) {
    /* NOTE: only "activate.cgi" (i.e. "state == true") has an
     * optional "duration" parameter */
    vapix_params = g_strdup_printf(VINPUT_VAPIX_CGI_PARAMS_FULL,
                                   schema_version,
                                   portnr,
                                   duration);
  } else {
    vapix_params =
            g_strdup_printf(VINPUT_VAPIX_CGI_PARAMS, schema_version, portnr);
  }

  if (state) {
    /* activate request */
    vapix_req = g_strdup_printf("%s?%s",
                                VINPUT_ACTIVATE_CGI_ENDPOINT,
                                vapix_params);
  } else {
    /* deactivate request */
    vapix_req = g_strdup_printf("%s?%s",
                                VINPUT_DEACTIVATE_CGI_ENDPOINT,
                                vapix_params);
  }

  response = vapix_request(curl_h,
                           credentials,
                           vapix_req,
                           HTTP_GET,
                           NONE_data,
                           NULL,
                           err);
  if (response != NULL) {
    if (vin_xml_parse(response, &parse_res, err)) {
      if (parse_res.vapix_mask & VAPIX_ERR) {
        /* VAPIX response: error */
        if (parse_res.error_descr) {
          SET_ERROR(err,
                    -1,
                    "%s: error response: %s",
                    vapix_req,
                    parse_res.error_descr);
        }
      } else {
        /* VAPIX response: success */
        ua_status = UA_STATUSCODE_GOOD;
        *state_changed = parse_res.state_changed;

        if (parse_res.state_changed) {
          /* request successfull with a state change, update our
           * 'vin_states' cache */
          if (state) {
            vin_states[portnr - 1] = TRUE;
          } else {
            vin_states[portnr - 1] = FALSE;
          }
        }
      }
    } else {
      g_prefix_error(err, "vin_xml_parse() failed: ");
    } /* vin_xml_parse(): FALSE */

    vin_xml_parse_clear(&parse_res);
    g_clear_pointer(&response, g_free);

  } else {
    /* failed request, response == NULL */
    g_prefix_error(err, "vapix_request() failed: ");
  }

  g_clear_pointer(&vapix_params, g_free);
  g_clear_pointer(&vapix_req, g_free);

  return ua_status;
}

gchar *
vin_get_schema_version(CURL *curl_h, const gchar *credentials, GError **err)
{
  parser_status_t parse_res = { 0 };
  gchar *schema_version = NULL;
  gchar *response = NULL;

  g_return_val_if_fail(curl_h != NULL, NULL);
  g_return_val_if_fail(credentials != NULL, NULL);
  g_return_val_if_fail(err == NULL || *err == NULL, NULL);

  /* fetch the schema version of the Virtual Input VAPIX */
  response = vapix_request(curl_h,
                           credentials,
                           VINPUT_SCHEMA_CGI_ENDPOINT,
                           HTTP_GET,
                           NONE_data,
                           NULL,
                           err);
  if (response != NULL) {
    if (vin_xml_parse(response, &parse_res, err)) {
      /* <Success> response: save the <MajorVersion> of <SchemaVersion> as we
       * need it for subsequent activate/deactivate VAPIX calls */
      schema_version = g_strdup(parse_res.schema_version);
    } else {
      /* <Error> response */
      g_prefix_error(err, "vin_xml_parse() failed: ");

      vin_xml_parse_clear(&parse_res);
      g_clear_pointer(&response, g_free);
      goto err_out;
    }
    vin_xml_parse_clear(&parse_res);
    g_clear_pointer(&response, g_free);
  } else {
    g_prefix_error(err, "vapix_request() failed: ");
  } /* response == NULL*/

err_out:

  return schema_version;
}
