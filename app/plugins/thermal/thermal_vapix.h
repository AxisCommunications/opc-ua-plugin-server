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

#ifndef __THERMAL_VAPIX_H__
#define __THERMAL_VAPIX_H__

#include <curl/curl.h>
#include <glib.h>

typedef struct thermal_area {
  gchar *detectionType;
  gboolean enabled;
  guint32 id;
  gchar *measurement;
  gint32 presetNbr;
  gint32 threshold;
  gchar *name;
} thermal_area_t;

typedef struct thermal_area_values {
  guint32 id;
  gdouble avg;
  gdouble max;
  gdouble min;
  gboolean triggered;
} thermal_area_values_t;

gboolean
vapix_get_supported_versions(gchar *credentials, CURL *curl_h, GError **err);

gboolean
vapix_get_thermal_areas(gchar *credentials,
                        CURL *curl_h,
                        GList **areas,
                        GError **err);

gboolean
vapix_get_thermal_area_status(gchar *credentials,
                              CURL *curl_h,
                              GList **areas,
                              GError **err);

gboolean
vapix_set_temperature_scale(gchar *credentials,
                            CURL *curl_h,
                            gchar *scale,
                            GError **err);

#endif /* __THERMAL_VAPIX_H__ */
