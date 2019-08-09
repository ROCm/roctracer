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

#ifndef INC_ROCTOOLSEXT_H_
#define INC_ROCTOOLSEXT_H_

#include <dlfcn.h>
#include <iostream>
#include <string.h>
#include "roctracer_tx.h"

#define PUBLIC_API __attribute__((visibility("default")))
#if 0
#define roctxMark            roctxMarkA
#define roctxRangeStart      roctxRangeStartA
#define roctxRangePush       roctxRangePushA
#endif
typedef uint64_t roctxRangeId_t;
typedef const char* roctxStringHandle_t;
typedef enum roctxMessageType_t
{
    ROCTX_MESSAGE_UNKNOWN          = 0,    
    ROCTX_MESSAGE_TYPE_ASCII       = 1
} roctxMessageType_t;
extern "C" {

PUBLIC_API void hsaOpenROCTX();
PUBLIC_API void hsaCloseROCTX();
PUBLIC_API void roctxMarkA (roctxStringHandle_t message);
PUBLIC_API roctxRangeId_t roctxRangePushA(roctxStringHandle_t message);
PUBLIC_API roctxRangeId_t roctxRangePop() ;
PUBLIC_API roctxRangeId_t roctxRangeStart(roctxStringHandle_t message);
PUBLIC_API roctxRangeId_t roctxRangeEnd(roctxRangeId_t rocTxRangeID);
}
#if 0
#define ROCTX_RESOURCE_MAKE_TYPE(CLASS, INDEX) ((((uint32_t)(ROCTX_RESOURCE_CLASS_ ## CLASS))<<16)|((uint32_t)(INDEX)))

typedef enum roctxResourceGenericType_t
{
    ROCTX_RESOURCE_TYPE_UNKNOWN = 0,
    ROCTX_RESOURCE_TYPE_GENERIC_POINTER = ROCTX_RESOURCE_MAKE_TYPE(GENERIC, 1), /**< Generic pointer assumed to have no collisions with other pointers. */
    ROCTX_RESOURCE_TYPE_GENERIC_THREAD_NATIVE = ROCTX_RESOURCE_MAKE_TYPE(GENERIC, 2), /**< OS native thread identifier. */
    ROCTX_RESOURCE_TYPE_GENERIC_THREAD_POSIX = ROCTX_RESOURCE_MAKE_TYPE(GENERIC, 3) /**< POSIX pthread identifier. */
} roctxResourceGenericType_t;
#endif

enum roctx_api_id_t {
  // block: HSAROCTXAPI API
  ROCTX_API_ID_hsaOpenROCTX = 0,
  ROCTX_API_ID_hsaCloseROCTX = 1,
  ROCTX_API_ID_roctxMarkA = 2,
  ROCTX_API_ID_roctxRangePushA = 3,
  ROCTX_API_ID_roctxRangePop = 4,
  ROCTX_API_ID_roctxRangeStart = 5,
  ROCTX_API_ID_roctxRangeEnd = 6,
  ROCTX_API_ID_NUMBER = 7,
  ROCTX_API_ID_ANY = 8,
};

struct roctx_api_data_t {
  uint64_t correlation_id;
  uint32_t phase;
  union {
  };
  union {
    // block: HSAROCTXAPI API
    struct {
    } hsaOpenROCTX;
    struct {
    } hsaCloseROCTX;
    struct {
      roctxStringHandle_t message;
    } roctxMarkA;
    struct {
      roctxStringHandle_t message;
    } roctxRangePushA;
    struct {
    } roctxRangePop;
    struct {
      roctxStringHandle_t message;
    } roctxRangeStart;
    struct {
      roctxRangeId_t rocTxRangeID;
    } roctxRangeEnd;
  } args;
};
//#if PROF_API_IMPL
namespace roctracer {
namespace roctx {

const char* GetApiName(const uint32_t& id) {
  switch (id) {
    // block: HSAROCTXAPI API
    case ROCTX_API_ID_hsaOpenROCTX: return "hsaOpenROCTX";
    case ROCTX_API_ID_hsaCloseROCTX: return "hsaCloseROCTX";
    case ROCTX_API_ID_roctxMarkA: return "roctxMarkA";
    case ROCTX_API_ID_roctxRangePushA: return "roctxRangePushA";
    case ROCTX_API_ID_roctxRangePop: return "roctxRangePop";
    case ROCTX_API_ID_roctxRangeStart: return "roctxRangeStart";
    case ROCTX_API_ID_roctxRangeEnd: return "roctxRangeEnd";
  }
  return "unknown";
}

uint32_t GetApiCode(const char* str) {
  // block: HSAROCTXAPI API
  if (strcmp("hsaOpenROCTX", str) == 0) return ROCTX_API_ID_hsaOpenROCTX;
  if (strcmp("hsaCloseROCTX", str) == 0) return ROCTX_API_ID_hsaCloseROCTX;
  if (strcmp("roctxMarkA", str) == 0) return ROCTX_API_ID_roctxMarkA;
  if (strcmp("roctxRangePushA", str) == 0) return ROCTX_API_ID_roctxRangePushA;
  if (strcmp("roctxRangePop", str) == 0) return ROCTX_API_ID_roctxRangePop;
  if (strcmp("roctxRangeStart", str) == 0) return ROCTX_API_ID_roctxRangeStart;
  if (strcmp("roctxRangeEnd", str) == 0) return ROCTX_API_ID_roctxRangeEnd;
  return ROCTX_API_ID_NUMBER;
}

typedef struct {
  decltype(hsaOpenROCTX)* hsaOpenROCTX_fn;
  decltype(hsaCloseROCTX)* hsaCloseROCTX_fn;
  decltype(roctxMarkA)* roctxMarkA_fn;
  decltype(roctxRangePushA)* roctxRangePushA_fn;
  decltype(roctxRangePop)* roctxRangePop_fn;
  decltype(roctxRangeStart)* roctxRangeStart_fn;
  decltype(roctxRangeEnd)* roctxRangeEnd_fn;
} HSAROCTXAPI_saved_t;

HSAROCTXAPI_saved_t* HSAROCTXAPI_saved = NULL;
void intercept_ROCTXApiTable(void) {
  HSAROCTXAPI_saved = new HSAROCTXAPI_saved_t{};
  typedef decltype(HSAROCTXAPI_saved_t::hsaOpenROCTX_fn) hsaOpenROCTX_t;
  HSAROCTXAPI_saved->hsaOpenROCTX_fn = (hsaOpenROCTX_t)dlsym(RTLD_NEXT,"hsaOpenROCTX");
  typedef decltype(HSAROCTXAPI_saved_t::hsaCloseROCTX_fn) hsaCloseROCTX_t;
  HSAROCTXAPI_saved->hsaCloseROCTX_fn = (hsaCloseROCTX_t)dlsym(RTLD_NEXT,"hsaCloseROCTX");
  typedef decltype(HSAROCTXAPI_saved_t::roctxMarkA_fn) roctxMarkA_t;
  HSAROCTXAPI_saved->roctxMarkA_fn = (roctxMarkA_t)dlsym(RTLD_NEXT,"roctxMarkA");
  typedef decltype(HSAROCTXAPI_saved_t::roctxRangePushA_fn) roctxRangePushA_t;
  HSAROCTXAPI_saved->roctxRangePushA_fn = (roctxRangePushA_t)dlsym(RTLD_NEXT,"roctxRangePushA");
  typedef decltype(HSAROCTXAPI_saved_t::roctxRangePop_fn) roctxRangePop_t;
  HSAROCTXAPI_saved->roctxRangePop_fn = (roctxRangePop_t)dlsym(RTLD_NEXT,"roctxRangePop");
  typedef decltype(HSAROCTXAPI_saved_t::roctxRangeStart_fn) roctxRangeStart_t;
  HSAROCTXAPI_saved->roctxRangeStart_fn = (roctxRangeStart_t)dlsym(RTLD_NEXT,"roctxRangeStart");
  typedef decltype(HSAROCTXAPI_saved_t::roctxRangeEnd_fn) roctxRangeEnd_t;
  HSAROCTXAPI_saved->roctxRangeEnd_fn = (roctxRangeEnd_t)dlsym(RTLD_NEXT,"roctxRangeEnd");
};

typedef CbTable<ROCTX_API_ID_NUMBER> cb_table_t;
cb_table_t cb_table;

void hsaOpenROCTX_callback(void) {
  if (HSAROCTXAPI_saved == NULL) intercept_ROCTXApiTable();
  roctx_api_data_t api_data{};
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_hsaOpenROCTX, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_hsaOpenROCTX, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->hsaOpenROCTX_fn();
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_hsaOpenROCTX, &api_data, api_callback_arg);
}
void hsaCloseROCTX_callback(void) {
  roctx_api_data_t api_data{};
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_hsaCloseROCTX, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_hsaCloseROCTX, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->hsaCloseROCTX_fn();
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_hsaCloseROCTX, &api_data, api_callback_arg);
}
void roctxMarkA_callback(roctxStringHandle_t message) {
  roctx_api_data_t api_data{};
  api_data.args.roctxMarkA.message = message;
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_roctxMarkA, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxMarkA, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->roctxMarkA_fn(message);
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxMarkA, &api_data, api_callback_arg);
}
void roctxRangePushA_callback(roctxStringHandle_t message) {
  roctx_api_data_t api_data{};
  api_data.args.roctxRangePushA.message = message;
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_roctxRangePushA, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangePushA, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->roctxRangePushA_fn(message);
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangePushA, &api_data, api_callback_arg);
}
void roctxRangePop_callback(void) {
  roctx_api_data_t api_data{};
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_roctxRangePop, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangePop, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->roctxRangePop_fn();
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangePop, &api_data, api_callback_arg);
}
void roctxRangeStart_callback(roctxStringHandle_t message) {
  roctx_api_data_t api_data{};
  api_data.args.roctxRangeStart.message = message;
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_roctxRangeStart, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangeStart, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->roctxRangeStart_fn(message);
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangeStart, &api_data, api_callback_arg);
}
void roctxRangeEnd_callback(roctxRangeId_t rocTxRangeID) {
  roctx_api_data_t api_data{};
  api_data.args.roctxRangeEnd.rocTxRangeID = rocTxRangeID;
  activity_rtapi_callback_t api_callback_fun = NULL;
  void* api_callback_arg = NULL;
  cb_table.get(ROCTX_API_ID_roctxRangeEnd, &api_callback_fun, &api_callback_arg);
  api_data.phase = 0;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangeEnd, &api_data, api_callback_arg);
  HSAROCTXAPI_saved->roctxRangeEnd_fn(rocTxRangeID);
  api_data.phase = 1;
  if (api_callback_fun) api_callback_fun(ACTIVITY_DOMAIN_ROCTX_API, ROCTX_API_ID_roctxRangeEnd, &api_data, api_callback_arg);
}
}; };
//#endif // PROF_API_IMPL

#endif // end of define INC_ROCTOOLSEXT_H_
