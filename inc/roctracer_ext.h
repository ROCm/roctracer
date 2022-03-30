/*
Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

////////////////////////////////////////////////////////////////////////////////
//
// ROC Tracer Extension API
//
// The API provides functionality for application annotation with event and
// external ranges correlation
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INC_ROCTRACER_EXT_H_
#define INC_ROCTRACER_EXT_H_

#include <roctracer.h>

typedef void (*roctracer_start_cb_t)(void);
typedef void (*roctracer_stop_cb_t)(void);
typedef struct {
  roctracer_start_cb_t start_cb;
  roctracer_stop_cb_t stop_cb;
} roctracer_ext_properties_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

////////////////////////////////////////////////////////////////////////////////
// Application annotation API

// Tracing start API
void roctracer_start(void);

// Tracing stop API
void roctracer_stop(void);

////////////////////////////////////////////////////////////////////////////////
// External correlation id API

// Notifies that the calling thread is entering an external API region.
// Push an external correlation id for the calling thread.
roctracer_status_t roctracer_activity_push_external_correlation_id(activity_correlation_id_t id);

// Notifies that the calling thread is leaving an external API region.
// Pop an external correlation id for the calling thread.
// 'lastId' returns the last external correlation if not NULL
roctracer_status_t roctracer_activity_pop_external_correlation_id(activity_correlation_id_t* last_id);

#ifdef __cplusplus
}  // extern "C" block
#endif  // __cplusplus

#endif  // INC_ROCTRACER_EXT_H_
