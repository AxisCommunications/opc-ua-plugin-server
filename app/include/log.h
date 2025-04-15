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

#ifndef __LOG_H__
#define __LOG_H__

#include <open62541/plugin/log_syslog.h>
/* clang-format off */
#define LOG_D(logger, fmt, args...)                                            \
  {                                                                            \
    UA_LOG_DEBUG(logger, UA_LOGCATEGORY_USERLAND, "%s:%d/%s: " fmt,            \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __FUNCTION__,                                                  \
                ##args);                                                       \
  }

#define LOG_I(logger, fmt, args...)                                            \
  {                                                                            \
    UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "%s:%d/%s: " fmt,             \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __FUNCTION__,                                                  \
                ##args);                                                       \
  }

#define LOG_W(logger, fmt, args...)                                            \
  {                                                                            \
    UA_LOG_WARNING(logger, UA_LOGCATEGORY_USERLAND, "%s:%d/%s: " fmt,          \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __FUNCTION__,                                                  \
                ##args);                                                       \
  }

#define LOG_E(logger, fmt, args...)                                            \
  {                                                                            \
    UA_LOG_ERROR(logger, UA_LOGCATEGORY_USERLAND, "%s:%d/%s: " fmt,            \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __FUNCTION__,                                                  \
                ##args);                                                       \
  }

#define LOG_C(logger, fmt, args...)                                            \
  {                                                                            \
    UA_LOG_FATAL(logger, UA_LOGCATEGORY_USERLAND, "%s:%d/%s: " fmt,            \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __FUNCTION__,                                                  \
                ##args);                                                       \
  }
/* clang-format on */

#endif /* __LOG_H__ */
