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
