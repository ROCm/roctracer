/* Copyright (c) 2018-2022 Advanced Micro Devices, Inc.

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
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE. */

#include "roctracer.h"
#include "roctracer_hip.h"
#include "roctracer_ext.h"
#include "roctracer_roctx.h"
#include "roctracer_hsa.h"

#include <assert.h>
#include <dirent.h>
#include <hsa/hsa_api_trace.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <stack>
#include <unordered_map>
#include <vector>

#include "correlation_id.h"
#include "debug.h"
#include "journal.h"
#include "loader.h"
#include "hsa_support.h"
#include "memory_pool.h"
#include "exception.h"
#include "logger.h"

#define API_METHOD_PREFIX                                                                          \
  roctracer_status_t err = ROCTRACER_STATUS_SUCCESS;                                               \
  try {
#define API_METHOD_SUFFIX                                                                          \
  }                                                                                                \
  catch (std::exception & e) {                                                                     \
    ERR_LOGGING(__FUNCTION__ << "(), " << e.what());                                               \
    err = GetExcStatus(e);                                                                         \
  }                                                                                                \
  return err;

#define API_METHOD_CATCH(X)                                                                        \
  }                                                                                                \
  catch (std::exception & e) {                                                                     \
    ERR_LOGGING(__FUNCTION__ << "(), " << e.what());                                               \
  }                                                                                                \
  (void)err;                                                                                       \
  return X;

static inline uint32_t GetPid() {
  static auto pid = syscall(__NR_getpid);
  return pid;
}
static inline uint32_t GetTid() {
  static thread_local auto tid = syscall(__NR_gettid);
  return tid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Internal library methods
//
namespace roctracer {

namespace ext_support {
roctracer_start_cb_t roctracer_start_cb = nullptr;
roctracer_stop_cb_t roctracer_stop_cb = nullptr;
}  // namespace ext_support

struct CallbackJournalData {
  roctracer_rtapi_callback_t callback;
  void* user_data;
};
static Journal<CallbackJournalData> cb_journal;

struct ActivityJournalData {
  roctracer_pool_t* pool;
};
static Journal<ActivityJournalData> act_journal;

roctracer_status_t GetExcStatus(const std::exception& e) {
  const ApiError* roctracer_exc_ptr = dynamic_cast<const ApiError*>(&e);
  return (roctracer_exc_ptr) ? roctracer_exc_ptr->status() : ROCTRACER_STATUS_ERROR;
}

std::mutex hip_activity_mutex;

enum { API_CB_MASK = 0x1, API_ACT_MASK = 0x2 };

class HIPActivityCallbackTracker {
 public:
  uint32_t enable_check(uint32_t op, uint32_t mask) { return data_[op] |= mask; }
  uint32_t disable_check(uint32_t op, uint32_t mask) { return data_[op] &= ~mask; }

 private:
  std::unordered_map<uint32_t, uint32_t> data_;
};

static HIPActivityCallbackTracker hip_act_cb_tracker;

void HIP_ApiCallback(uint32_t op_id, roctracer_record_t* record, void* callback_data, void* arg) {
  hip_api_data_t* data = static_cast<hip_api_data_t*>(callback_data);
  MemoryPool* pool = static_cast<MemoryPool*>(arg);

  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    // Generate a new correlation ID.
    uint64_t correlation_id = CorrelationIdPush();
    data->correlation_id = correlation_id;

    if (pool != nullptr) {
      // Filing record info
      record->domain = ACTIVITY_DOMAIN_HIP_API;
      record->kind = 0;
      record->op = op_id;
      record->process_id = GetPid();
      record->thread_id = GetTid();
      record->begin_ns = hsa_support::timestamp_ns();
      record->correlation_id = correlation_id;
    }
  } else {
    if (pool != nullptr) {
      record->end_ns = hsa_support::timestamp_ns();

      if (auto external_id = ExternalCorrelationId()) {
        roctracer_record_t ext_record{};
        ext_record.domain = ACTIVITY_DOMAIN_EXT_API;
        ext_record.op = ACTIVITY_EXT_OP_EXTERN_ID;
        ext_record.correlation_id = record->correlation_id;
        ext_record.external_id = *external_id;
        // Write the external correlation id record directly followed by the activity record.
        pool->Write(std::array<roctracer_record_t, 2>{ext_record, *record});
      } else {
        // Write record to the buffer.
        pool->Write(*record);
      }
    }
    CorrelationIdPop();
  }
}

void HIP_AsyncActivityCallback(uint32_t op_id, void* record_ptr, void* arg) {
  MemoryPool* pool = reinterpret_cast<MemoryPool*>(arg);
  roctracer_record_t& record = *reinterpret_cast<roctracer_record_t*>(record_ptr);
  record.domain = ACTIVITY_DOMAIN_HIP_OPS;

  // If the record is for a kernel dispatch, write the kernel name in the pool's data,
  // and make the record point to it. Older HIP runtimes do not provide a kernel
  // name, so record.kernel_name might be null.
  if (record.op == HIP_OP_ID_DISPATCH && record.kernel_name != nullptr)
    pool->Write(record, record.kernel_name, strlen(record.kernel_name) + 1,
                [](auto& record, const void* data) {
                  record.kernel_name = static_cast<const char*>(data);
                });
  else
    pool->Write(record);
}

// Logger routines and primitives
util::Logger::mutex_t util::Logger::mutex_;
std::atomic<util::Logger*> util::Logger::instance_{};

// Memory pool routines and primitives
MemoryPool* default_memory_pool = nullptr;
std::recursive_mutex memory_pool_mutex;

// Stop status routines and primitives
unsigned stop_status_value = 0;
std::mutex stop_status_mutex;
unsigned set_stopped(unsigned val) {
  std::lock_guard lock(stop_status_mutex);
  const unsigned ret = (stop_status_value ^ val);
  stop_status_value = val;
  return ret;
}

}  // namespace roctracer

using namespace roctracer;

LOADER_INSTANTIATE();

///////////////////////////////////////////////////////////////////////////////////////////////////
// Public library methods
//

// Returns library version
ROCTRACER_API uint32_t roctracer_version_major() { return ROCTRACER_VERSION_MAJOR; }
ROCTRACER_API uint32_t roctracer_version_minor() { return ROCTRACER_VERSION_MINOR; }

// Returns the last error
ROCTRACER_API const char* roctracer_error_string() {
  return strdup(util::Logger::LastMessage().c_str());
}

// Return Op string by given domain and activity/API codes
// nullptr returned on the error and the library errno is set
ROCTRACER_API const char* roctracer_op_string(uint32_t domain, uint32_t op, uint32_t kind) {
  API_METHOD_PREFIX
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_API:
      return hsa_support::GetApiName(op);
    case ACTIVITY_DOMAIN_HSA_EVT:
      return hsa_support::GetEvtName(op);
    case ACTIVITY_DOMAIN_HSA_OPS:
      return hsa_support::GetOpsName(op);
    case ACTIVITY_DOMAIN_HIP_OPS:
      return HipLoader::Instance().GetOpName(kind);
    case ACTIVITY_DOMAIN_HIP_API:
      return HipLoader::Instance().ApiName(op);
    case ACTIVITY_DOMAIN_EXT_API:
      return "EXT_API";
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
  API_METHOD_CATCH(nullptr)
}

// Return Op code and kind by given string
ROCTRACER_API roctracer_status_t roctracer_op_code(uint32_t domain, const char* str, uint32_t* op,
                                                   uint32_t* kind) {
  API_METHOD_PREFIX
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_API: {
      *op = hsa_support::GetApiCode(str);
      if (*op == HSA_API_ID_NUMBER) {
        EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_ARGUMENT,
                    "Invalid API name \"" << str << "\", domain ID(" << domain << ")");
      }
      if (kind != nullptr) *kind = 0;
      break;
    }
    case ACTIVITY_DOMAIN_HIP_API: {
      *op = hipApiIdByName(str);
      if (*op == HIP_API_ID_NONE) {
        EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_ARGUMENT,
                    "Invalid API name \"" << str << "\", domain ID(" << domain << ")");
      }
      if (kind != nullptr) *kind = 0;
      break;
    }
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "limited domain ID(" << domain << ")");
  }
  API_METHOD_SUFFIX
}

static inline uint32_t get_op_begin(uint32_t domain) {
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
      return 0;
    case ACTIVITY_DOMAIN_HSA_API:
      return 0;
    case ACTIVITY_DOMAIN_HSA_EVT:
      return 0;
    case ACTIVITY_DOMAIN_HIP_OPS:
      return 0;
    case ACTIVITY_DOMAIN_HIP_API:
      return HIP_API_ID_FIRST;
    case ACTIVITY_DOMAIN_EXT_API:
      return 0;
    case ACTIVITY_DOMAIN_ROCTX:
      return 0;
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
  return 0;
}

static inline uint32_t get_op_end(uint32_t domain) {
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
      return HSA_OP_ID_NUMBER;
    case ACTIVITY_DOMAIN_HSA_API:
      return HSA_API_ID_NUMBER;
    case ACTIVITY_DOMAIN_HSA_EVT:
      return HSA_EVT_ID_NUMBER;
    case ACTIVITY_DOMAIN_HIP_OPS:
      return HIP_OP_ID_NUMBER;
    case ACTIVITY_DOMAIN_HIP_API:
      return HIP_API_ID_LAST + 1;
    case ACTIVITY_DOMAIN_EXT_API:
      return 0;
    case ACTIVITY_DOMAIN_ROCTX:
      return ROCTX_API_ID_NUMBER;
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
  return 0;
}

// Enable runtime API callbacks
static void roctracer_enable_callback_fun(roctracer_domain_t domain, uint32_t op,
                                          roctracer_rtapi_callback_t callback, void* user_data) {
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
    case ACTIVITY_DOMAIN_HSA_API:
    case ACTIVITY_DOMAIN_HSA_EVT:
      hsa_support::EnableCallback(domain, op, callback, user_data);
      break;
    case ACTIVITY_DOMAIN_HIP_OPS:
      break;
    case ACTIVITY_DOMAIN_HIP_API: {
      if (!HipLoader::Instance().Enabled()) break;
      std::lock_guard lock(hip_activity_mutex);

      if (hipError_t err =
              HipLoader::Instance().RegisterApiCallback(op, (void*)callback, user_data);
          err != hipSuccess)
        fatal("HIP::RegisterApiCallback(%d) failed (err=%d)", op, err);

      if ((hip_act_cb_tracker.enable_check(op, API_CB_MASK) & API_ACT_MASK) == 0) {
        if (hipError_t err =
                HipLoader::Instance().RegisterActivityCallback(op, (void*)HIP_ApiCallback, nullptr);
            err != hipSuccess)
          fatal("HIP::RegisterActivityCallback(%d) failed (err=%d)", op, err);
      }
      break;
    }
    case ACTIVITY_DOMAIN_ROCTX: {
      if (RocTxLoader::Instance().Enabled() &&
          !RocTxLoader::Instance().RegisterApiCallback(op, (void*)callback, user_data))
        fatal("ROCTX::RegisterApiCallback(%d) failed", op);
      break;
    }
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
}

static void roctracer_enable_callback_impl(roctracer_domain_t domain, uint32_t op,
                                           roctracer_rtapi_callback_t callback, void* user_data) {
  cb_journal.Insert(domain, op, {callback, user_data});
  roctracer_enable_callback_fun(domain, op, callback, user_data);
}

ROCTRACER_API roctracer_status_t roctracer_enable_op_callback(roctracer_domain_t domain,
                                                              uint32_t op,
                                                              roctracer_rtapi_callback_t callback,
                                                              void* user_data) {
  API_METHOD_PREFIX
  roctracer_enable_callback_impl(domain, op, callback, user_data);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_enable_domain_callback(
    roctracer_domain_t domain, roctracer_rtapi_callback_t callback, void* user_data) {
  API_METHOD_PREFIX
  const uint32_t op_end = get_op_end(domain);
  for (uint32_t op = get_op_begin(domain); op < op_end; ++op)
    roctracer_enable_callback_impl(domain, op, callback, user_data);
  API_METHOD_SUFFIX
}

// Disable runtime API callbacks
static void roctracer_disable_callback_fun(roctracer_domain_t domain, uint32_t op) {
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
    case ACTIVITY_DOMAIN_HSA_API:
    case ACTIVITY_DOMAIN_HSA_EVT:
      hsa_support::DisableCallback(domain, op);
      break;
    case ACTIVITY_DOMAIN_HIP_OPS:
      break;
    case ACTIVITY_DOMAIN_HIP_API: {
      if (!HipLoader::Instance().Enabled()) break;
      std::lock_guard lock(hip_activity_mutex);

      if (hipError_t err = HipLoader::Instance().RemoveApiCallback(op); err != hipSuccess)
        fatal("HIP::RemoveApiCallback(%d) failed (err=%d)", op, err);

      if ((hip_act_cb_tracker.disable_check(op, API_CB_MASK) & API_ACT_MASK) == 0) {
        if (hipError_t err = HipLoader::Instance().RemoveActivityCallback(op); err != hipSuccess)
          fatal("HIP::RemoveActivityCallback(%d) failed (err=%d)", op, err);
      }
      break;
    }
    case ACTIVITY_DOMAIN_ROCTX: {
      if (RocTxLoader::Instance().Enabled() && !RocTxLoader::Instance().RemoveApiCallback(op))
        fatal("ROCTX::RemoveApiCallback(%d) failed", op);
      break;
    }
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
}

static void roctracer_disable_callback_impl(roctracer_domain_t domain, uint32_t op) {
  cb_journal.Remove(domain, op);
  roctracer_disable_callback_fun(domain, op);
}

ROCTRACER_API roctracer_status_t roctracer_disable_op_callback(roctracer_domain_t domain,
                                                               uint32_t op) {
  API_METHOD_PREFIX
  roctracer_disable_callback_impl(domain, op);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_disable_domain_callback(roctracer_domain_t domain) {
  API_METHOD_PREFIX
  const uint32_t op_end = get_op_end(domain);
  for (uint32_t op = get_op_begin(domain); op < op_end; ++op)
    roctracer_disable_callback_impl(domain, op);
  API_METHOD_SUFFIX
}

// Return default pool and set new one if parameter pool is not NULL.
ROCTRACER_API roctracer_pool_t* roctracer_default_pool_expl(roctracer_pool_t* pool) {
  std::lock_guard lock(memory_pool_mutex);
  roctracer_pool_t* p = reinterpret_cast<roctracer_pool_t*>(default_memory_pool);
  if (pool != nullptr) default_memory_pool = reinterpret_cast<MemoryPool*>(pool);
  return p;
}

ROCTRACER_API roctracer_pool_t* roctracer_default_pool() {
  std::lock_guard lock(memory_pool_mutex);
  return reinterpret_cast<roctracer_pool_t*>(default_memory_pool);
}

// Open memory pool
static void roctracer_open_pool_impl(const roctracer_properties_t* properties,
                                     roctracer_pool_t** pool) {
  std::lock_guard lock(memory_pool_mutex);
  if ((pool == nullptr) && (default_memory_pool != nullptr)) {
    EXC_RAISING(ROCTRACER_STATUS_ERROR_DEFAULT_POOL_ALREADY_DEFINED, "default pool already set");
  }
  MemoryPool* p = new MemoryPool(*properties);
  if (p == nullptr) EXC_RAISING(ROCTRACER_STATUS_ERROR_MEMORY_ALLOCATION, "MemoryPool() error");
  if (pool != nullptr)
    *pool = p;
  else
    default_memory_pool = p;
}

ROCTRACER_API roctracer_status_t roctracer_open_pool_expl(const roctracer_properties_t* properties,
                                                          roctracer_pool_t** pool) {
  API_METHOD_PREFIX
  roctracer_open_pool_impl(properties, pool);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_open_pool(const roctracer_properties_t* properties) {
  API_METHOD_PREFIX
  roctracer_open_pool_impl(properties, nullptr);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_next_record(const activity_record_t* record,
                                                       const activity_record_t** next) {
  API_METHOD_PREFIX
  *next = record + 1;
  API_METHOD_SUFFIX
}

// Enable activity records logging
static void roctracer_enable_activity_fun(roctracer_domain_t domain, uint32_t op,
                                          roctracer_pool_t* pool) {
  assert(pool != nullptr);
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
    case ACTIVITY_DOMAIN_HSA_API:
    case ACTIVITY_DOMAIN_HSA_EVT:
      hsa_support::EnableActivity(domain, op, pool);
      break;
    case ACTIVITY_DOMAIN_HIP_OPS: {
      if (HipLoader::Instance().Enabled() &&
          HipLoader::Instance().RegisterAsyncActivityCallback(op, (void*)HIP_AsyncActivityCallback,
                                                              pool) != hipSuccess)
        fatal("HIP::EnableActivityCallback error");
      break;
    }
    case ACTIVITY_DOMAIN_HIP_API: {
      if (!HipLoader::Instance().Enabled()) break;
      std::lock_guard lock(hip_activity_mutex);

      hip_act_cb_tracker.enable_check(op, API_ACT_MASK);
      if (hipError_t err =
              HipLoader::Instance().RegisterActivityCallback(op, (void*)HIP_ApiCallback, pool);
          err != hipSuccess)
        fatal("HIP::RegisterActivityCallback(%d) (err=%d)", op, err);
      break;
    }
    case ACTIVITY_DOMAIN_ROCTX:
      break;
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
}

static void roctracer_enable_activity_impl(roctracer_domain_t domain, uint32_t op,
                                           roctracer_pool_t* pool) {
  if (pool == nullptr) pool = default_memory_pool;
  if (pool == nullptr)
    EXC_RAISING(ROCTRACER_STATUS_ERROR_DEFAULT_POOL_UNDEFINED, "no default pool");
  act_journal.Insert(domain, op, {pool});
  roctracer_enable_activity_fun(domain, op, pool);
}

ROCTRACER_API roctracer_status_t roctracer_enable_op_activity_expl(roctracer_domain_t domain,
                                                                   uint32_t op,
                                                                   roctracer_pool_t* pool) {
  API_METHOD_PREFIX
  roctracer_enable_activity_impl(domain, op, pool);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_enable_op_activity(activity_domain_t domain,
                                                              uint32_t op) {
  API_METHOD_PREFIX
  roctracer_enable_activity_impl(domain, op, nullptr);
  API_METHOD_SUFFIX
}

static void roctracer_enable_domain_activity_impl(roctracer_domain_t domain,
                                                  roctracer_pool_t* pool) {
  const uint32_t op_end = get_op_end(domain);
  for (uint32_t op = get_op_begin(domain); op < op_end; ++op) try {
      roctracer_enable_activity_impl(domain, op, pool);
    } catch (const ApiError& err) {
      if (err.status() != ROCTRACER_STATUS_ERROR_NOT_IMPLEMENTED) throw;
    }
}

ROCTRACER_API roctracer_status_t roctracer_enable_domain_activity_expl(roctracer_domain_t domain,
                                                                       roctracer_pool_t* pool) {
  API_METHOD_PREFIX
  roctracer_enable_domain_activity_impl(domain, pool);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_enable_domain_activity(activity_domain_t domain) {
  API_METHOD_PREFIX
  roctracer_enable_domain_activity_impl(domain, nullptr);
  API_METHOD_SUFFIX
}

// Disable activity records logging
static void roctracer_disable_activity_fun(roctracer_domain_t domain, uint32_t op) {
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
    case ACTIVITY_DOMAIN_HSA_API:
    case ACTIVITY_DOMAIN_HSA_EVT:
      hsa_support::DisableActivity(domain, op);
      break;
    case ACTIVITY_DOMAIN_HIP_OPS: {
      if (HipLoader::Instance().Enabled() &&
          HipLoader::Instance().RemoveAsyncActivityCallback(op) != hipSuccess)
        fatal("HIP::EnableActivityCallback(%d)", op);
      break;
    }
    case ACTIVITY_DOMAIN_HIP_API: {
      if (!HipLoader::Instance().Enabled()) break;
      std::lock_guard lock(hip_activity_mutex);

      if ((hip_act_cb_tracker.disable_check(op, API_ACT_MASK) & API_CB_MASK) == 0) {
        if (hipError_t err = HipLoader::Instance().RemoveActivityCallback(op); err != hipSuccess)
          fatal("HIP::RemoveActivityCallback(%d) failed (err=%d)", op, err);
      } else {
        if (hipError_t err =
                HipLoader::Instance().RegisterActivityCallback(op, (void*)HIP_ApiCallback, nullptr);
            err != hipSuccess)
          fatal("HIP::RegisterActivityCallback(%d) failed (err=%d)", op, err);
      }
      break;
    }
    case ACTIVITY_DOMAIN_ROCTX:
      break;
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
}

static void roctracer_disable_activity_impl(roctracer_domain_t domain, uint32_t op) {
  act_journal.Remove(domain, op);
  roctracer_disable_activity_fun(domain, op);
}

ROCTRACER_API roctracer_status_t roctracer_disable_op_activity(roctracer_domain_t domain,
                                                               uint32_t op) {
  API_METHOD_PREFIX
  roctracer_disable_activity_impl(domain, op);
  API_METHOD_SUFFIX
}

static void roctracer_disable_domain_activity_impl(roctracer_domain_t domain) {
  const uint32_t op_end = get_op_end(domain);
  for (uint32_t op = get_op_begin(domain); op < op_end; ++op) try {
      roctracer_disable_activity_impl(domain, op);
    } catch (const ApiError& err) {
      if (err.status() != ROCTRACER_STATUS_ERROR_NOT_IMPLEMENTED) throw;
    }
}

ROCTRACER_API roctracer_status_t roctracer_disable_domain_activity(roctracer_domain_t domain) {
  API_METHOD_PREFIX
  roctracer_disable_domain_activity_impl(domain);
  API_METHOD_SUFFIX
}

// Close memory pool
static void roctracer_close_pool_impl(roctracer_pool_t* pool) {
  std::lock_guard lock(memory_pool_mutex);
  if (pool == nullptr) pool = reinterpret_cast<roctracer_pool_t*>(default_memory_pool);
  if (pool == nullptr) return;
  MemoryPool* p = reinterpret_cast<MemoryPool*>(pool);
  if (p == default_memory_pool) default_memory_pool = nullptr;

  // Disable any activities that specify the pool being deleted.
  std::vector<std::pair<roctracer_domain_t, uint32_t>> ops;
  act_journal.ForEach(
      [&ops, pool](roctracer_domain_t domain, uint32_t op, const ActivityJournalData& data) {
        if (pool == data.pool) ops.emplace_back(domain, op);
        return true;
      });
  for (auto&& [domain, op] : ops) roctracer_disable_activity_impl(domain, op);

  delete (p);
}

ROCTRACER_API roctracer_status_t roctracer_close_pool_expl(roctracer_pool_t* pool) {
  API_METHOD_PREFIX
  roctracer_close_pool_impl(pool);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_close_pool() {
  API_METHOD_PREFIX
  roctracer_close_pool_impl(NULL);
  API_METHOD_SUFFIX
}

// Flush available activity records
static void roctracer_flush_activity_impl(roctracer_pool_t* pool) {
  if (pool == nullptr) pool = roctracer_default_pool();
  MemoryPool* default_memory_pool = reinterpret_cast<MemoryPool*>(pool);
  if (default_memory_pool != nullptr) default_memory_pool->Flush();
}

ROCTRACER_API roctracer_status_t roctracer_flush_activity_expl(roctracer_pool_t* pool) {
  API_METHOD_PREFIX
  roctracer_flush_activity_impl(pool);
  API_METHOD_SUFFIX
}

ROCTRACER_API roctracer_status_t roctracer_flush_activity() {
  API_METHOD_PREFIX
  roctracer_flush_activity_impl(nullptr);
  API_METHOD_SUFFIX
}

// Notifies that the calling thread is entering an external API region.
// Push an external correlation id for the calling thread.
ROCTRACER_API roctracer_status_t
roctracer_activity_push_external_correlation_id(activity_correlation_id_t id) {
  API_METHOD_PREFIX
  ExternalCorrelationIdPush(id);
  API_METHOD_SUFFIX
}

// Notifies that the calling thread is leaving an external API region.
// Pop an external correlation id for the calling thread, and return it in 'last_id' if not null.
ROCTRACER_API roctracer_status_t
roctracer_activity_pop_external_correlation_id(activity_correlation_id_t* last_id) {
  API_METHOD_PREFIX

  auto external_id = ExternalCorrelationIdPop();
  if (!external_id) {
    if (last_id != nullptr) *last_id = 0;
    EXC_RAISING(ROCTRACER_STATUS_ERROR_MISMATCHED_EXTERNAL_CORRELATION_ID,
                "unbalanced external correlation id pop");
  }

  if (last_id != nullptr) *last_id = *external_id;
  API_METHOD_SUFFIX
}

// Start API
ROCTRACER_API void roctracer_start() {
  if (set_stopped(0)) {
    if (ext_support::roctracer_start_cb) ext_support::roctracer_start_cb();
    cb_journal.ForEach([](roctracer_domain_t domain, uint32_t op, const CallbackJournalData& data) {
      roctracer_enable_callback_fun(domain, op, data.callback, data.user_data);
      return true;
    });
    act_journal.ForEach(
        [](roctracer_domain_t domain, uint32_t op, const ActivityJournalData& data) {
          roctracer_enable_activity_fun(domain, op, data.pool);
          return true;
        });
  }
}

// Stop API
ROCTRACER_API void roctracer_stop() {
  if (set_stopped(1)) {
    // Must disable the activity first as the spawner checks for the activity being NULL
    // to indicate that there is no callback.
    act_journal.ForEach([](roctracer_domain_t domain, uint32_t op, const ActivityJournalData&) {
      roctracer_disable_activity_fun(domain, op);
      return true;
    });
    cb_journal.ForEach([](roctracer_domain_t domain, uint32_t op, const CallbackJournalData&) {
      roctracer_disable_callback_fun(domain, op);
      return true;
    });
    if (ext_support::roctracer_stop_cb) ext_support::roctracer_stop_cb();
  }
}

ROCTRACER_API roctracer_status_t roctracer_get_timestamp(roctracer_timestamp_t* timestamp) {
  API_METHOD_PREFIX
  *timestamp = hsa_support::timestamp_ns();
  API_METHOD_SUFFIX
}

// Set properties
ROCTRACER_API roctracer_status_t roctracer_set_properties(roctracer_domain_t domain,
                                                          void* properties) {
  API_METHOD_PREFIX
  switch (domain) {
    case ACTIVITY_DOMAIN_HSA_OPS:
    case ACTIVITY_DOMAIN_HSA_EVT:
    case ACTIVITY_DOMAIN_HSA_API:
    case ACTIVITY_DOMAIN_HIP_OPS:
    case ACTIVITY_DOMAIN_HIP_API: {
      break;
    }
    case ACTIVITY_DOMAIN_EXT_API: {
      roctracer_ext_properties_t* ops_properties =
          reinterpret_cast<roctracer_ext_properties_t*>(properties);
      ext_support::roctracer_start_cb = ops_properties->start_cb;
      ext_support::roctracer_stop_cb = ops_properties->stop_cb;
      break;
    }
    default:
      EXC_RAISING(ROCTRACER_STATUS_ERROR_INVALID_DOMAIN_ID, "invalid domain ID(" << domain << ")");
  }
  API_METHOD_SUFFIX
}

__attribute__((constructor)) void constructor() { util::Logger::Create(); }
__attribute__((destructor)) void destructor() { util::Logger::Destroy(); }

extern "C" {

// The HSA_AMD_TOOL_PRIORITY variable must be a constant value type initialized by the loader
// itself, not by code during _init. 'extern const' seems to do that although that is not a
// guarantee.
ROCTRACER_EXPORT extern const uint32_t HSA_AMD_TOOL_PRIORITY = 50;

// HSA-runtime tool on-load method
ROCTRACER_EXPORT bool OnLoad(HsaApiTable* table, uint64_t runtime_version,
                             uint64_t failed_tool_count, const char* const* failed_tool_names) {
  [](auto&&...) {}(runtime_version, failed_tool_count, failed_tool_names);
  hsa_support::Initialize(table);
  return true;
}

ROCTRACER_EXPORT void OnUnload() { hsa_support::Finalize(); }

}  // extern "C"