/**
 * Copyright (C), Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ERROR_H__
#define __ERROR_H__

#define DEFINE_GQUARK(catalog)                                                 \
  static GQuark error_quark(void)                                              \
  {                                                                            \
    static GQuark quark;                                                       \
    if (!quark)                                                                \
      quark = g_quark_from_static_string(catalog);                             \
    return quark;                                                              \
  }

#define SET_ERROR(error, code, ...)                                            \
  G_STMT_START { g_set_error(error, error_quark(), code, __VA_ARGS__); }       \
  G_STMT_END

#define GERROR_MSG(err) (err ? err->message : "Unknown error")

#endif /* __ERROR_H__*/
