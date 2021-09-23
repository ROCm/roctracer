/*
Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.

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

#include <sstream>
#include <string>

#include <cxxabi.h>  /* names denangle */
#include <dirent.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>  /* SYS_xxx definitions */
#include <sys/types.h>
#include <unistd.h>  /* usleep */

#include <roctracer_ext.h>
#include <roctracer_roctx.h>
#include <roctracer_hsa.h>
#include <roctracer_hip.h>
#include <roctracer_hcc.h>
#include <roctracer_kfd.h>
#include <ext/hsa_rt_utils.hpp>
#include <roctracer_trace_entries.h>

#include "src/core/loader.h"
#include "src/core/trace_buffer.h"
#include "util/evt_stats.h"
#include "util/hsa_rsrc_factory.h"
#include "util/xml.h"

#define PUBLIC_API __attribute__((visibility("default")))
#define CONSTRUCTOR_API __attribute__((constructor))
#define DESTRUCTOR_API __attribute__((destructor))

// Linux sys call
#define PTHREAD_CALL(call)                                                                         \
  do {                                                                                             \
    int err = call;                                                                                \
    if (err != 0) {                                                                                \
      errno = err;                                                                                 \
      perror(#call);                                                                               \
      abort();                                                                                     \
    }                                                                                              \
  } while (0)

// Macro to check ROC-tracer calls status
#define ROCTRACER_CALL(call)                                                                       \
  do {                                                                                             \
    int err = call;                                                                                \
    if (err != 0) {                                                                                \
      std::cerr << roctracer_error_string() << std::endl << std::flush;                            \
      abort();                                                                                     \
    }                                                                                              \
  } while (0)

#define ONLOAD_TRACE(str) \
  if (getenv("ROCP_ONLOAD_TRACE")) do { \
    std::cout << "PID(" << GetPid() << "): TRACER_TOOL::" << __FUNCTION__ << " " << str << std::endl << std::flush; \
  } while(0);
#define ONLOAD_TRACE_BEG() ONLOAD_TRACE("begin")
#define ONLOAD_TRACE_END() ONLOAD_TRACE("end")

static inline uint32_t GetPid() { return syscall(__NR_getpid); }
static inline uint32_t GetTid() { return syscall(__NR_gettid); }

#if DEBUG_TRACE_ON
inline static void DEBUG_TRACE(const char* fmt, ...) {
  constexpr int size = 256;
  char buf[size];

  va_list valist;
  va_start(valist, fmt);
  vsnprintf(buf, size, fmt, valist);
  printf("%u:%u %s", GetPid(), GetTid(), buf); fflush(stdout);
  va_end(valist);
}
#else
inline static void DEBUG_TRACE(const char* fmt, ...) {}
#endif

hsa_rt_utils::Timer* timer = NULL;
thread_local timestamp_t hsa_begin_timestamp = 0;
thread_local timestamp_t hip_begin_timestamp = 0;
thread_local timestamp_t kfd_begin_timestamp = 0;
bool trace_roctx = false;
bool trace_hsa_api = false;
bool trace_hsa_activity = false;
bool trace_hip_api = false;
bool trace_hip_activity = false;
bool trace_kfd = false;
bool trace_pcs = false;
// API trace arrays and sizes
uint32_t hsa_api_array_size=0;
char** hsa_api_array;
uint32_t kfd_api_array_size=0;
char** kfd_api_array;
uint32_t hip_api_array_size=0;
char** hip_api_array;

LOADER_INSTANTIATE();
TRACE_BUFFER_INSTANTIATE();

typedef EvtStatsT<const std::string, uint32_t> EvtStatsA;
// HIP stats
EvtStats* hip_api_stats = NULL;
EvtStatsA* hip_kernel_stats = NULL;
EvtStatsA* hip_memcpy_stats = NULL;

// Global output file handle
FILE* begin_ts_file_handle = NULL;
FILE* roctx_file_handle = NULL;
FILE* hsa_api_file_handle = NULL;
FILE* hsa_async_copy_file_handle = NULL;
FILE* hip_api_file_handle = NULL;
FILE* hcc_activity_file_handle = NULL;
FILE* kfd_api_file_handle = NULL;
FILE* pc_sample_file_handle = NULL;

void close_output_file(FILE* file_handle);
void close_file_handles() {
  if (begin_ts_file_handle) close_output_file(pc_sample_file_handle);
  if (roctx_file_handle) close_output_file(roctx_file_handle);
  if (hsa_api_file_handle) close_output_file(hsa_api_file_handle);
  if (hsa_async_copy_file_handle) close_output_file(hsa_async_copy_file_handle);
  if (hip_api_file_handle) close_output_file(hip_api_file_handle);
  if (hcc_activity_file_handle) close_output_file(hcc_activity_file_handle);
  if (kfd_api_file_handle) close_output_file(kfd_api_file_handle);
  if (pc_sample_file_handle) close_output_file(pc_sample_file_handle);
}

static const uint32_t my_pid = GetPid();

// Error handler
void fatal(const std::string msg) {
  close_file_handles();
  fflush(stdout);
  fprintf(stderr, "%s\n\n", msg.c_str());
  fflush(stderr);
  abort();
}

// C++ symbol demangle
static inline const char* cxx_demangle(const char* symbol) {
  size_t funcnamesize;
  int status;
  const char* ret = (symbol != NULL) ? abi::__cxa_demangle(symbol, NULL, &funcnamesize, &status) : symbol;
  return (ret != NULL) ? ret : strdup(symbol);
}

// Tracing control thread
uint32_t control_delay_us = 0;
uint32_t control_len_us = 0;
uint32_t control_dist_us = 0;
void* control_thr_fun(void*) {
  const uint32_t delay_sec = control_delay_us / 1000000;
  const uint32_t delay_us = control_delay_us % 1000000;
  const uint32_t len_sec = control_len_us / 1000000;
  const uint32_t len_us = control_len_us % 1000000;
  const uint32_t dist_sec = control_dist_us / 1000000;
  const uint32_t dist_us = control_dist_us % 1000000;
  bool to_start = true;

  sleep(delay_sec);
  usleep(delay_us);

  while (1) {
    if (to_start) {
      to_start = false;
      roctracer_start();
      sleep(len_sec);
      usleep(len_us);
    } else {
      to_start = true;
      roctracer_stop();
      sleep(dist_sec);
      usleep(dist_us);
    }
  }

  return NULL;
}

// Flushing control thread
uint32_t control_flush_us = 0;
pthread_t flush_thread;
bool flush_thread_started = false;
std::mutex flush_thread_mutex;;

void* flush_thr_fun(void*) {
  const uint32_t dist_sec = control_flush_us / 1000000;
  const uint32_t dist_us = control_flush_us % 1000000;

  while (1) {
    sleep(dist_sec);
    usleep(dist_us);
    std::lock_guard<std::mutex> lock(flush_thread_mutex);
    if (!flush_thread_started) while(1) sleep(1);
    ROCTRACER_CALL(roctracer_flush_activity());
    roctracer::TraceBufferBase::FlushAll();
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// rocTX annotation tracing

void roctx_flush_cb_wrapper(roctx_trace_entry_t* entry);
constexpr roctracer::TraceBuffer<roctx_trace_entry_t>::flush_prm_t roctx_flush_prm = {roctracer::DFLT_ENTRY_TYPE, roctx_flush_cb_wrapper};
roctracer::TraceBuffer<roctx_trace_entry_t>* roctx_trace_buffer = NULL;

// rocTX callback function
static inline void roctx_callback_fun(
    uint32_t domain,
    uint32_t cid,
    uint32_t tid,
    roctx_range_id_t rid,
    const char* message)
{
#if ROCTX_CLOCK_TIME
  const timestamp_t time = HsaTimer::clocktime_ns(HsaTimer::TIME_ID_CLOCK_MONOTONIC);
#else
  const timestamp_t time = timer->timestamp_fn_ns();
#endif
  roctx_trace_entry_t* entry = roctx_trace_buffer->GetEntry();
  entry->cid = cid;
  entry->time = time;
  entry->pid = GetPid();
  entry->tid = tid;
  entry->rid = rid;
  entry->message = (message != NULL) ? strdup(message) : NULL;
  entry->valid.store(roctracer::TRACE_ENTRY_COMPL, std::memory_order_release);
}

void roctx_api_callback(
    uint32_t domain,
    uint32_t cid,
    const void* callback_data,
    void* arg)
{
  (void)arg;
  const roctx_api_data_t* data = reinterpret_cast<const roctx_api_data_t*>(callback_data);
  roctx_callback_fun(domain, cid, GetTid(), data->args.id, data->args.message);
}

// rocTX Start/Stop callbacks
void roctx_range_start_callback(const roctx_range_data_t* data, void* arg) {
  roctx_callback_fun(ACTIVITY_DOMAIN_ROCTX, ROCTX_API_ID_roctxRangePushA, data->tid, 0, data->message);
}
void roctx_range_stop_callback(const roctx_range_data_t* data, void* arg) {
  roctx_callback_fun(ACTIVITY_DOMAIN_ROCTX, ROCTX_API_ID_roctxRangePop, data->tid, 0, NULL);
}
void start_callback() { roctracer::RocTxLoader::Instance().RangeStackIterate(roctx_range_start_callback, NULL); }
void stop_callback() { roctracer::RocTxLoader::Instance().RangeStackIterate(roctx_range_stop_callback, NULL); }

// rocTX buffer flush function
void roctx_flush_cb(roctx_trace_entry_t* entry) {
  std::ostringstream os;
  os << entry->time << " " << entry->pid << ":" << entry->tid << " " << entry->cid << ":" << entry->rid;
  if (entry->message != NULL) os << ":\"" << entry->message << "\"";
  else os << ":\"\"";
  fprintf(roctx_file_handle, "%s\n", os.str().c_str()); fflush(roctx_file_handle);
}

void roctx_flush_cb_wrapper(roctx_trace_entry_t* entry){
#if ROCTX_CLOCK_TIME
  timestamp_t timestamp = 0;
  HsaRsrcFactory::Instance().GetTimestamp(HsaTimer::TIME_ID_CLOCK_MONOTONIC, entry->time, &timestamp);
  entry->time = timestamp
#endif
  roctx_flush_cb(entry);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// HSA API tracing

void hsa_api_flush_cb(hsa_api_trace_entry_t* entry);
constexpr roctracer::TraceBuffer<hsa_api_trace_entry_t>::flush_prm_t hsa_flush_prm = {roctracer::DFLT_ENTRY_TYPE, hsa_api_flush_cb};
roctracer::TraceBuffer<hsa_api_trace_entry_t>* hsa_api_trace_buffer = NULL;

// HSA API callback function

void hsa_api_callback(
    uint32_t domain,
    uint32_t cid,
    const void* callback_data,
    void* arg)
{
  (void)arg;
  const hsa_api_data_t* data = reinterpret_cast<const hsa_api_data_t*>(callback_data);
  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    hsa_begin_timestamp = timer->timestamp_fn_ns();
  } else {

    const timestamp_t end_timestamp = (cid == HSA_API_ID_hsa_shut_down) ? hsa_begin_timestamp : timer->timestamp_fn_ns();
    hsa_api_trace_entry_t* entry = hsa_api_trace_buffer->GetEntry();
    entry->cid = cid;
    entry->begin = hsa_begin_timestamp;
    entry->end = end_timestamp;
    entry->pid = GetPid();
    entry->tid = GetTid();
    entry->data = *data;
    entry->valid.store(roctracer::TRACE_ENTRY_COMPL, std::memory_order_release);
  }
}

void hsa_api_flush_cb(hsa_api_trace_entry_t* entry) {
  std::ostringstream os;
  os << entry->begin << ":" << entry->end << " " << entry->pid << ":" << entry->tid << " " << hsa_api_data_pair_t(entry->cid, entry->data);
  fprintf(hsa_api_file_handle, "%s\n", os.str().c_str()); fflush(hsa_api_file_handle);
}

void hsa_activity_flush_cb(
  hsa_activity_trace_entry_t *entry)
{
  fprintf(hsa_async_copy_file_handle, "%lu:%lu async-copy:%lu:%u\n", entry->record->begin_ns, entry->record->end_ns, entry->index, entry->pid); fflush(hsa_async_copy_file_handle);
}

void hsa_activity_callback_wrapper( uint32_t op,
  activity_record_t* record,
  void* arg){
  static uint64_t index = 0;
  hsa_activity_trace_entry_t hsa_activity_trace_entry = {index, op, my_pid, record, arg};
  hsa_activity_flush_cb(&hsa_activity_trace_entry);
  index++;
  }

///////////////////////////////////////////////////////////////////////////////////////////////////////
// HIP API tracing

void hip_api_flush_cb(hip_api_trace_entry_t* entry);
constexpr roctracer::TraceBuffer<hip_api_trace_entry_t>::flush_prm_t hip_api_flush_prm = {roctracer::DFLT_ENTRY_TYPE, hip_api_flush_cb};
roctracer::TraceBuffer<hip_api_trace_entry_t>* hip_api_trace_buffer = NULL;

static inline bool is_hip_kernel_launch_api(const uint32_t& cid) {
  bool ret =
    (cid == HIP_API_ID_hipLaunchKernel) ||
    (cid == HIP_API_ID_hipLaunchCooperativeKernel) ||
    (cid == HIP_API_ID_hipLaunchCooperativeKernelMultiDevice) ||
    (cid == HIP_API_ID_hipExtLaunchMultiKernelMultiDevice) ||
    (cid == HIP_API_ID_hipModuleLaunchKernel) ||
    (cid == HIP_API_ID_hipExtModuleLaunchKernel) ||
    (cid == HIP_API_ID_hipHccModuleLaunchKernel);
  return ret;
}

void hip_api_callback(
    uint32_t domain,
    uint32_t cid,
    const void* callback_data,
    void* arg)
{
  (void)arg;
  const hip_api_data_t* data = reinterpret_cast<const hip_api_data_t*>(callback_data);
  const timestamp_t timestamp = timer->timestamp_fn_ns();
  hip_api_trace_entry_t* entry = NULL;

  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    hip_begin_timestamp = timestamp;
  } else {
    // Post onit of HIP APU args
    hipApiArgsInit((hip_api_id_t)cid, const_cast<hip_api_data_t*>(data));

    entry = hip_api_trace_buffer->GetEntry();
    entry->cid = cid;
    entry->domain = domain;
    entry->begin = hip_begin_timestamp;
    entry->end = timestamp;
    entry->pid = GetPid();
    entry->tid = GetTid();
    entry->data = *data;
    entry->name = NULL;
    entry->ptr = NULL;

    if (cid == HIP_API_ID_hipMalloc) {
      entry->ptr = *(data->args.hipMalloc.ptr);
    } else if (is_hip_kernel_launch_api(cid)) {
      switch(cid) {
        case HIP_API_ID_hipExtLaunchMultiKernelMultiDevice:
        case HIP_API_ID_hipLaunchCooperativeKernelMultiDevice:
        {
          const hipLaunchParams* listKernels = data->args.hipLaunchCooperativeKernelMultiDevice.launchParamsList;
          std::string name_str = "";
          for (int i = 0; i < data->args.hipLaunchCooperativeKernelMultiDevice.numDevices; ++i) {
            const hipLaunchParams& lp = listKernels[i];
            if (lp.func != NULL) {
              const char* kernel_name = roctracer::HipLoader::Instance().KernelNameRefByPtr(lp.func, lp.stream);
              const int device_id = roctracer::HipLoader::Instance().GetStreamDeviceId(lp.stream);
              name_str += std::string(kernel_name) + ":" + std::to_string(device_id) + ";";
            }
          }
          entry->name = strdup(name_str.c_str());
          break;
        }
        case HIP_API_ID_hipLaunchKernel:
        case HIP_API_ID_hipLaunchCooperativeKernel:
        {
          const void* f = data->args.hipLaunchKernel.function_address;
          hipStream_t stream = data->args.hipLaunchKernel.stream;
          if (f != NULL) entry->name = strdup(roctracer::HipLoader::Instance().KernelNameRefByPtr(f, stream));
          break;
        }
        default:
        {
          const hipFunction_t f = data->args.hipModuleLaunchKernel.f;
          if (f != NULL) entry->name = strdup(roctracer::HipLoader::Instance().KernelNameRef(f));
        }
      }
    }

    entry->valid.store(roctracer::TRACE_ENTRY_COMPL, std::memory_order_release);
  }

  const char * name = roctracer_op_string(domain, cid, 0);
  DEBUG_TRACE("hip_api_callback(\"%s\") phase(%d): cid(%u) data(%p) entry(%p) name(\"%s\") correlation_id(%lu) timestamp(%lu)\n",
    name, data->phase, cid, data, entry, (entry) ? entry->name : NULL, data->correlation_id, timestamp);
}

void mark_api_callback(
    uint32_t domain,
    uint32_t cid,
    const void* callback_data,
    void* arg)
{
  (void)arg;
  const char* name = reinterpret_cast<const char*>(callback_data);

  const timestamp_t timestamp = timer->timestamp_fn_ns();
  hip_api_trace_entry_t* entry = hip_api_trace_buffer->GetEntry();
  entry->cid = 0;
  entry->domain = domain;
  entry->begin = timestamp;
  entry->end = timestamp + 1;
  entry->pid = GetPid();
  entry->tid = GetTid();
  entry->data = {};
  entry->name = strdup(name);
  entry->ptr = NULL;
  entry->valid.store(roctracer::TRACE_ENTRY_COMPL, std::memory_order_release);
}

typedef std::map<uint64_t, const char*> hip_kernel_map_t;
hip_kernel_map_t* hip_kernel_map = NULL;
std::mutex hip_kernel_mutex;

void hip_api_flush_cb(hip_api_trace_entry_t *entry){
  const uint32_t domain = entry->domain;
  const uint32_t cid = entry->cid;
  const hip_api_data_t* data = &(entry->data);
  const uint64_t correlation_id = data->correlation_id;
  const timestamp_t begin_timestamp = entry->begin;
  const timestamp_t end_timestamp = entry->end;
  std::ostringstream rec_ss;
  std::ostringstream oss;

  const char* str = (domain != ACTIVITY_DOMAIN_EXT_API) ? roctracer_op_string(domain, cid, 0) : strdup("MARK");
  rec_ss << std::dec << begin_timestamp << ":" << end_timestamp << " " << entry->pid << ":" << entry->tid;
  oss << std::dec << rec_ss.str() << " " << str;

  const char * name = roctracer_op_string(entry->domain, entry->cid, 0);
  DEBUG_TRACE("hip_api_flush_cb(\"%s\"): domain(%u) cid(%u) entry(%p) name(\"%s\" correlation_id(%lu) beg(%lu) end(%lu))\n",
    name, entry->domain, entry->cid, entry, entry->name, correlation_id, begin_timestamp, end_timestamp);

  if (domain == ACTIVITY_DOMAIN_HIP_API) {
#if HIP_PROF_HIP_API_STRING
    if (hip_api_stats == NULL) {
      const char* str = hipApiString((hip_api_id_t)cid, data);
      rec_ss << " " << str;
      if (is_hip_kernel_launch_api(cid) && entry->name) {
        const char* kernel_name = cxx_demangle(entry->name);
        rec_ss << " kernel=" << kernel_name;
      }
      rec_ss<< " :" << correlation_id;
      fprintf(hip_api_file_handle, "%s\n", rec_ss.str().c_str());
    }
#else  // !HIP_PROF_HIP_API_STRING
    switch (cid) {
      case HIP_API_ID_hipMemcpy:
        fprintf(hip_api_file_handle, "%s(dst(%p) src(%p) size(0x%x) kind(%u))\n",
          oss.str().c_str(),
          data->args.hipMemcpy.dst,
          data->args.hipMemcpy.src,
          (uint32_t)(data->args.hipMemcpy.sizeBytes),
          (uint32_t)(data->args.hipMemcpy.kind));
        break;
      case HIP_API_ID_hipMemcpyAsync:
        fprintf(hip_api_file_handle, "%s(dst(%p) src(%p) size(0x%x) kind(%u) stream(%p))\n",
          oss.str().c_str(),
          data->args.hipMemcpyAsync.dst,
          data->args.hipMemcpyAsync.src,
          (uint32_t)(data->args.hipMemcpyAsync.sizeBytes),
          (uint32_t)(data->args.hipMemcpyAsync.kind),
          data->args.hipMemcpyAsync.stream);
        break;
      case HIP_API_ID_hipMalloc:
        fprintf(hip_api_file_handle, "%s(ptr(%p) size(0x%x))\n",
          oss.str().c_str(),
          entry->ptr,
          (uint32_t)(data->args.hipMalloc.size));
        break;
      case HIP_API_ID_hipFree:
        fprintf(hip_api_file_handle, "%s(ptr(%p))\n",
          oss.str().c_str(),
          data->args.hipFree.ptr);
        break;
      case HIP_API_ID_hipModuleLaunchKernel:
        fprintf(hip_api_file_handle, "%s(kernel(%s) stream(%p))\n",
          oss.str().c_str(),
          cxx_demangle(entry->name),
          data->args.hipModuleLaunchKernel.stream);
        break;
      case HIP_API_ID_hipExtModuleLaunchKernel:
        fprintf(hip_api_file_handle, "%s(kernel(%s) stream(%p))\n",
          oss.str().c_str(),
          cxx_demangle(entry->name),
          data->args.hipExtModuleLaunchKernel.hStream);
        break;
#if !HIP_VDI
      case HIP_API_ID_hipHccModuleLaunchKernel:
        fprintf(hip_api_file_handle, "%s(kernel(%s) stream(%p))\n",
          oss.str().c_str(),
          cxx_demangle(entry->name),
          data->args.hipHccModuleLaunchKernel.hStream);
        break;
#endif
      default:
        fprintf(hip_api_file_handle, "%s()\n", oss.str().c_str());
    }
#endif  // !HIP_PROF_HIP_API_STRING
  } else {
    fprintf(hip_api_file_handle, "%s(name(%s))\n", oss.str().c_str(), entry->name);
  }

  fflush(hip_api_file_handle);
}

void hip_api_flush_cb_wrapper(hip_api_trace_entry_t *entry){
  const uint32_t domain = entry->domain;
  const uint32_t cid = entry->cid;
  const hip_api_data_t* data = &(entry->data);
  const uint64_t correlation_id = data->correlation_id;
  const timestamp_t begin_timestamp = entry->begin;
  const timestamp_t end_timestamp = entry->end;

#if HIP_PROF_HIP_API_STRING
  if (domain == ACTIVITY_DOMAIN_HIP_API && hip_api_stats != NULL) {
    hip_api_stats->add_event(cid, end_timestamp - begin_timestamp);
    if (is_hip_kernel_launch_api(cid)) {
      hip_kernel_mutex.lock();
      (*hip_kernel_map)[correlation_id] = entry->name;
      hip_kernel_mutex.unlock();
    }
  }
#endif
  hip_api_flush_cb(entry);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
// HSA API tracing

struct hip_act_trace_entry_t {
  std::atomic<uint32_t> valid;
  roctracer::entry_type_t type;
  uint32_t kind;
  timestamp_t dur;
  uint64_t correlation_id;
};

void hip_act_flush_cb(hip_act_trace_entry_t* entry);
constexpr roctracer::TraceBuffer<hip_act_trace_entry_t>::flush_prm_t hip_act_flush_prm = {roctracer::DFLT_ENTRY_TYPE, hip_act_flush_cb};
roctracer::TraceBuffer<hip_act_trace_entry_t>* hip_act_trace_buffer = NULL;

// HIP ACT trace buffer flush callback
void hip_act_flush_cb(hip_act_trace_entry_t* entry) {
  const uint32_t domain = ACTIVITY_DOMAIN_HCC_OPS;
  const uint32_t op = 0;
  const char * name = roctracer_op_string(domain, op, entry->kind);
  if (name == NULL) {
    printf("hip_act_flush_cb name is NULL\n"); fflush(stdout);
    abort();
  }

  if (strncmp("Kernel", name, 6) == 0) {
    hip_kernel_mutex.lock();
    if (hip_kernel_stats == NULL) {
      printf("hip_act_flush_cb hip_kernel_stats is NULL\n"); fflush(stdout);
      abort();
    }
    name = (*hip_kernel_map)[entry->correlation_id];
    hip_kernel_mutex.unlock();
    const char* kernel_name = cxx_demangle(name);
    hip_kernel_stats->add_event(kernel_name, entry->dur);
  } else {
    hip_memcpy_stats->add_event(name, entry->dur);
  }
}

// Activity tracing callback
//   hipMalloc id(3) correlation_id(1): begin_ns(1525888652762640464) end_ns(1525888652762877067)

void hip_activity_flush_cb(hip_activity_trace_entry_t *entry){
  fprintf(hcc_activity_file_handle, "%lu:%lu %d:%lu %s:%lu:%u\n",
    entry->record->begin_ns, entry->record->end_ns,
    entry->record->device_id, entry->record->queue_id,
    entry->name, entry->record->correlation_id, entry->pid);
  fflush(hcc_activity_file_handle);
}

void pool_activity_callback(const char* begin, const char* end, void* arg) {
  const roctracer_record_t* record = reinterpret_cast<const roctracer_record_t*>(begin);
  const roctracer_record_t* end_record = reinterpret_cast<const roctracer_record_t*>(end);

  while (record < end_record) {
    const char * name = roctracer_op_string(record->domain, record->op, record->kind);
    DEBUG_TRACE("pool_activity_callback(\"%s\"): domain(%u) op(%u) kind(%u) record(%p) correlation_id(%lu) beg(%lu) end(%lu)\n",
      name, record->domain, record->op, record->kind, record, record->correlation_id, record->begin_ns, record->end_ns);

    switch(record->domain) {
      case ACTIVITY_DOMAIN_HCC_OPS:
        if (hip_memcpy_stats != NULL) {
          hip_act_trace_entry_t* entry = hip_act_trace_buffer->GetEntry();
          entry->kind = record->kind;
          entry->dur = record->end_ns - record->begin_ns;
          entry->correlation_id = record->correlation_id;
          entry->valid.store(roctracer::TRACE_ENTRY_COMPL, std::memory_order_release);
        } else {
          hip_activity_trace_entry_t hip_activity_trace_entry = {record, name, my_pid};
          hip_activity_flush_cb(&hip_activity_trace_entry);
        }
        break;
      case ACTIVITY_DOMAIN_HSA_OPS:
        if (record->op == HSA_OP_ID_RESERVED1) {
          fprintf(pc_sample_file_handle, "%u %lu 0x%lx %s\n",
            record->pc_sample.se, record->pc_sample.cycle, record->pc_sample.pc, name);
          fflush(pc_sample_file_handle);
        }
        break;
    }
    ROCTRACER_CALL(roctracer_next_record(record, &record));
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// KFD API tracing

void kfd_api_flush_cb(kfd_api_trace_entry_t* entry);
constexpr roctracer::TraceBuffer<kfd_api_trace_entry_t>::flush_prm_t kfd_api_flush_prm = {roctracer::DFLT_ENTRY_TYPE, kfd_api_flush_cb};
roctracer::TraceBuffer<kfd_api_trace_entry_t>* kfd_api_trace_buffer = NULL;

// KFD API callback function

static thread_local bool in_kfd_api_callback = false;
void kfd_api_callback(
    uint32_t domain,
    uint32_t cid,
    const void* callback_data,
    void* arg)
{
  (void)arg;
  if (in_kfd_api_callback) return;
  in_kfd_api_callback = true;
  const kfd_api_data_t* data = reinterpret_cast<const kfd_api_data_t*>(callback_data);
  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    kfd_begin_timestamp = timer->timestamp_fn_ns();
  } else {
    const timestamp_t end_timestamp = timer->timestamp_fn_ns();
    kfd_api_trace_entry_t* entry = kfd_api_trace_buffer->GetEntry();
    entry->cid = cid;
    entry->begin = kfd_begin_timestamp;
    entry->end = end_timestamp;
    entry->pid = GetPid();
    entry->tid = GetTid();
    entry->data = *data;
    entry->valid.store(roctracer::TRACE_ENTRY_COMPL, std::memory_order_release);
  }
  in_kfd_api_callback = false;
}

void kfd_api_flush_cb(kfd_api_trace_entry_t* entry) {
  std::ostringstream os;
  os << entry->begin << ":" << entry->end << " " << entry->pid << ":" << entry->tid << " " << kfd_api_data_pair_t(entry->cid, entry->data);
  fprintf(kfd_api_file_handle, "%s\n", os.str().c_str());
}
///////////////////////////////////////////////////////////////////////////////////////////////////////

// Input parser
std::string normalize_token(const std::string& token, bool not_empty, const std::string& label) {
  const std::string space_chars_set = " \t";
  const size_t first_pos = token.find_first_not_of(space_chars_set);
  size_t norm_len = 0;
  std::string error_str = "none";
  if (first_pos != std::string::npos) {
    const size_t last_pos = token.find_last_not_of(space_chars_set);
    if (last_pos == std::string::npos) error_str = "token string error: \"" + token + "\"";
    else {
      const size_t end_pos = last_pos + 1;
      if (end_pos <= first_pos) error_str = "token string error: \"" + token + "\"";
      else norm_len = end_pos - first_pos;
    }
  }
  if (((first_pos != std::string::npos) && (norm_len == 0)) ||
      ((first_pos == std::string::npos) && not_empty)) {
    fatal("normalize_token error, " + label + ": '" + token + "'," + error_str);
  }
  return (norm_len != 0) ? token.substr(first_pos, norm_len) : std::string("");
}

int get_xml_array(const xml::Xml::level_t* node, const std::string& field, const std::string& delim, std::vector<std::string>* vec, const char* label = NULL) {
  int parse_iter = 0;
  const auto& opts = node->opts;
  auto it = opts.find(field);
  if (it != opts.end()) {
    const std::string array_string = it->second;
    if (label != NULL) printf("%s%s = %s\n", label, field.c_str(), array_string.c_str());
    size_t pos1 = 0;
    const size_t string_len = array_string.length();
    while (pos1 < string_len) {
      const size_t pos2 = array_string.find(delim, pos1);
      const bool found = (pos2 != std::string::npos);
      const size_t token_len = (pos2 != std::string::npos) ? pos2 - pos1 : string_len - pos1;
      const std::string token = array_string.substr(pos1, token_len);
      const std::string norm_str = normalize_token(token, found, "get_xml_array");
      if (norm_str.length() != 0) vec->push_back(norm_str);
      if (!found) break;
      pos1 = pos2 + 1;
      ++parse_iter;
    }
  }
  return parse_iter;
}

// Open output file
FILE* open_output_file(const char* prefix, const char* name, const char** path = NULL) {
  FILE* file_handle = NULL;
  if (path != NULL) *path = NULL;

  if (prefix != NULL) {
    std::ostringstream oss;
    oss << prefix << "/" << GetPid() << "_" << name;
    file_handle = fopen(oss.str().c_str(), "w");
    if (file_handle == NULL) {
      std::ostringstream errmsg;
      errmsg << "ROCTracer: fopen error, file '" << oss.str().c_str() << "'";
      perror(errmsg.str().c_str());
      abort();
    }

    if (path != NULL) *path = strdup(oss.str().c_str());
  } else file_handle = stdout;
  return file_handle;
}

void close_output_file(FILE* file_handle) {
  if (file_handle != NULL) {
    fflush(file_handle);
    if (file_handle != stdout) fclose(file_handle);
  }
}

// Allocating tracing pool
void open_tracing_pool() {
  if (roctracer_default_pool() == NULL) {
    roctracer_properties_t properties{};
    properties.buffer_size = 0x80000;
    properties.buffer_callback_fun = pool_activity_callback;
    ROCTRACER_CALL(roctracer_open_pool(&properties));
  }
}

// Flush tracing pool
void close_tracing_pool() {
  if (roctracer_default_pool() != NULL) {
    ROCTRACER_CALL(roctracer_flush_activity());
  }
}

// tool library is loaded
static bool is_loaded = false;

// tool unload method
void tool_unload() {
  ONLOAD_TRACE("begin, loaded(" << is_loaded << ")");

  if (is_loaded == false) return;
  is_loaded = false;

  if (flush_thread_started) {
    flush_thread_mutex.lock();
    flush_thread_started = false;
    flush_thread_mutex.unlock();
    PTHREAD_CALL(pthread_cancel(flush_thread));
    void *res;
    PTHREAD_CALL(pthread_join(flush_thread, &res));
    if (res != PTHREAD_CANCELED) FATAL("flush thread wasn't stopped correctly");
  }

  if (trace_roctx) {
    ROCTRACER_CALL(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_ROCTX));
  }
  if (trace_hsa_api) {
    ROCTRACER_CALL(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HSA_API));
  }
  if (trace_hsa_activity || trace_pcs) {
    ROCTRACER_CALL(roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HSA_OPS));
  }
  if (trace_hip_api || trace_hip_activity) {
    ROCTRACER_CALL(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HIP_API));
    ROCTRACER_CALL(roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HIP_API));
    ROCTRACER_CALL(roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HCC_OPS));
  }
  if (trace_kfd) {
    ROCTRACER_CALL(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_KFD_API));
  }

  // Flush tracing pool
  close_tracing_pool();
  roctracer::TraceBufferBase::FlushAll();

  ONLOAD_TRACE_END();
}

// tool load method
void tool_load() {
  ONLOAD_TRACE("begin, loaded(" << is_loaded << ")");

  if (is_loaded == true) return;
  is_loaded = true;

  // Output file
  const char* output_prefix = getenv("ROCP_OUTPUT_DIR");
  if (output_prefix != NULL) {
    DIR* dir = opendir(output_prefix);
    if (dir == NULL) {
      std::ostringstream errmsg;
      errmsg << "ROCTracer: Cannot open output directory '" << output_prefix << "'";
      perror(errmsg.str().c_str());
      abort();
    }
  }

  // API traces switches
  const char* trace_domain = getenv("ROCTRACER_DOMAIN");
  if (trace_domain != NULL) {
    // ROCTX domain
    if (std::string(trace_domain).find("roctx") != std::string::npos) {
      trace_roctx = true;
    }

    // HSA/HIP domains enabling
    if (std::string(trace_domain).find("hsa") != std::string::npos) {
      trace_hsa_api = true;
      trace_hsa_activity = true;
    }
    if (std::string(trace_domain).find("hip") != std::string::npos) {
      trace_hip_api = true;
      trace_hip_activity = true;
    }
    if (std::string(trace_domain).find("sys") != std::string::npos) {
      trace_hsa_api = true;
      trace_hip_api = true;
      trace_hip_activity = true;
    }

    // KFD domain enabling
    if (std::string(trace_domain).find("kfd") != std::string::npos) {
      trace_kfd = true;
    }

    // PC sampling enabling
    if (std::string(trace_domain).find("pcs") != std::string::npos) {
      trace_pcs = true;
    }
  }

  printf("ROCTracer (pid=%d): ", (int)GetPid()); fflush(stdout);

  // XML input
  const char* xml_name = getenv("ROCP_INPUT");
  if (xml_name != NULL) {
    xml::Xml* xml = xml::Xml::Create(xml_name);
    if (xml == NULL) {
      fprintf(stderr, "ROCTracer: Input file not found '%s'\n", xml_name);
      abort();
    }

    bool found = false;
    for (const auto* entry : xml->GetNodes("top.trace")) {
      auto it = entry->opts.find("name");
      if (it == entry->opts.end()) fatal("ROCTracer: trace name is missing");
      const std::string& name = it->second;

      std::vector<std::string> api_vec;
      for (const auto* node : entry->nodes) {
        if (node->tag != "parameters") fatal("ROCTracer: trace node is not supported '" + name + ":" + node->tag + "'");
        get_xml_array(node, "api", ",", &api_vec);
        break;
      }

      if (name == "rocTX") {
        found = true;
        trace_roctx = true;
      }
      if (name == "HSA") {
        found = true;
        trace_hsa_api = true;
        hsa_api_array_size = api_vec.size();
        if(hsa_api_array_size > 0){
          hsa_api_array = (char**)malloc(api_vec.size() * sizeof(char*));
          for(uint64_t i = 0 ; i < api_vec.size(); i++){
            hsa_api_array[i] = strdup(api_vec[i].c_str());
          }
        }      
      }
      if (name == "GPU") {
        found = true;
        trace_hsa_activity = true;
      }
      if (name == "HIP") {
        found = true;
        trace_hip_api = true;
        trace_hip_activity = true;
        hip_api_array_size = api_vec.size();
        if(hip_api_array_size > 0){
          hip_api_array = (char**)malloc(api_vec.size() * sizeof(char*));
          for(uint64_t i = 0 ; i < api_vec.size(); i++){
            hip_api_array[i] = strdup(api_vec[i].c_str());
          }
        }
      }
      if (name == "KFD") {
        found = true;
        trace_kfd = true;
        kfd_api_array_size = api_vec.size();
        if(kfd_api_array_size > 0){
          kfd_api_array = (char**)malloc(api_vec.size() * sizeof(char*));
          for(uint64_t i = 0 ; i < api_vec.size(); i++){
            kfd_api_array[i] = strdup(api_vec[i].c_str());
          }
        }
      }
    }

    if (found) printf("input from \"%s\"", xml_name);
  }
  printf("\n");

  // Disable HIP activity if HSA activity was set
  if (trace_hsa_activity == true) trace_hip_activity = false;

  // Enable rpcTX callbacks
  if (trace_roctx) {
    roctx_file_handle = open_output_file(output_prefix, "roctx_trace.txt");

    // initialize HSA tracing
    roctracer_ext_properties_t properties {
      start_callback,
      stop_callback
    };
    roctracer_set_properties(ACTIVITY_DOMAIN_EXT_API, &properties);

    fprintf(stdout, "    rocTX-trace()\n"); fflush(stdout);
    ROCTRACER_CALL(roctracer_enable_domain_callback(ACTIVITY_DOMAIN_ROCTX, roctx_api_callback, NULL));
  }

  const char* ctrl_str = getenv("ROCP_CTRL_RATE");
  if (ctrl_str != NULL) {
    uint32_t ctrl_delay = 0;
    uint32_t ctrl_len = 0;
    uint32_t ctrl_rate = 0;

    sscanf(ctrl_str, "%d:%d:%d", &ctrl_delay, &ctrl_len, &ctrl_rate);

    if (ctrl_len > ctrl_rate) {
      fprintf(stderr, "ROCTracer: control length value (%u) > rate value (%u)\n", ctrl_len, ctrl_rate);
      abort();
    }
    control_dist_us = ctrl_rate - ctrl_len;
    control_len_us = ctrl_len;
    control_delay_us = ctrl_delay;

    roctracer_stop();

    if (ctrl_delay != UINT32_MAX) {
      fprintf(stdout, "ROCTracer: trace control: delay(%uus), length(%uus), rate(%uus)\n", ctrl_delay, ctrl_len, ctrl_rate); fflush(stdout);
      pthread_t thread;
      pthread_attr_t attr;
      int err = pthread_attr_init(&attr);
      if (err) { errno = err; perror("pthread_attr_init"); abort(); }
      err = pthread_create(&thread, &attr, control_thr_fun, NULL);
    } else {
      fprintf(stdout, "ROCTracer: trace start disabled\n"); fflush(stdout);
    }
  }

  const char* flush_str = getenv("ROCP_FLUSH_RATE");
  if (flush_str != NULL) {
    sscanf(flush_str, "%d", &control_flush_us);
    if (control_flush_us == 0) {
      fprintf(stderr, "ROCTracer: control flush rate bad value\n");
      abort();
    }

    fprintf(stdout, "ROCTracer: trace control flush rate(%uus)\n", control_flush_us); fflush(stdout);
    pthread_attr_t attr;
    int err = pthread_attr_init(&attr);
    if (err) { errno = err; perror("pthread_attr_init"); abort(); }
    std::lock_guard<std::mutex> lock(flush_thread_mutex);
    PTHREAD_CALL(pthread_create(&flush_thread, &attr, flush_thr_fun, NULL));
    flush_thread_started = true;
  }

  // Enable KFD API callbacks/activity
  if (trace_kfd) {
    kfd_api_file_handle = open_output_file(output_prefix, "kfd_api_trace.txt");
    // initialize KFD tracing
    roctracer_set_properties(ACTIVITY_DOMAIN_KFD_API, NULL);

    printf("    KFD-trace(");
    if (kfd_api_array_size != 0) {
      for (unsigned i = 0; i < kfd_api_array_size; ++i) {
        uint32_t cid = KFD_API_ID_NUMBER;
        const char* api = kfd_api_array[i];
        ROCTRACER_CALL(roctracer_op_code(ACTIVITY_DOMAIN_KFD_API, api, &cid, NULL));
        ROCTRACER_CALL(roctracer_enable_op_callback(ACTIVITY_DOMAIN_KFD_API, cid, kfd_api_callback, NULL));
        printf(" %s", api);
        free((char*)api);
      }
      free(kfd_api_array);
    } else {
      ROCTRACER_CALL(roctracer_enable_domain_callback(ACTIVITY_DOMAIN_KFD_API, kfd_api_callback, NULL));
    }
    printf(")\n");
  }

  ONLOAD_TRACE_END();
}

void exit_handler(int status, void* arg) {
  ONLOAD_TRACE("status(" << status << ") arg(" << arg << ")");
}

// HSA-runtime tool on-load method
extern "C" PUBLIC_API bool OnLoad(HsaApiTable* table, uint64_t runtime_version, uint64_t failed_tool_count,
                                  const char* const* failed_tool_names) {
  ONLOAD_TRACE_BEG();
  on_exit(exit_handler, NULL);

  timer = new hsa_rt_utils::Timer(table->core_->hsa_system_get_info_fn);

  const char* output_prefix = getenv("ROCP_OUTPUT_DIR");

  // App begin timestamp begin_ts_file.txt
  begin_ts_file_handle = open_output_file(output_prefix, "begin_ts_file.txt");
  const timestamp_t app_start_time = timer->timestamp_fn_ns();
  fprintf(begin_ts_file_handle, "%lu\n", app_start_time);

  // Enable HSA API callbacks/activity
  if (trace_hsa_api) {
    hsa_api_file_handle = open_output_file(output_prefix, "hsa_api_trace.txt");

    // initialize HSA tracing
    roctracer_set_properties(ACTIVITY_DOMAIN_HSA_API, (void*)table);

    fprintf(stdout, "    HSA-trace("); fflush(stdout);
    if (hsa_api_array_size != 0) {
      for (unsigned i = 0; i < hsa_api_array_size; ++i) {
        uint32_t cid = HSA_API_ID_NUMBER;
        const char* api = hsa_api_array[i];
        ROCTRACER_CALL(roctracer_op_code(ACTIVITY_DOMAIN_HSA_API, api, &cid, NULL));
        ROCTRACER_CALL(roctracer_enable_op_callback(ACTIVITY_DOMAIN_HSA_API, cid, hsa_api_callback, NULL));
        printf(" %s", api);
        free((char*)api);
      }
      free(hsa_api_array);
    } else {
      ROCTRACER_CALL(roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HSA_API, hsa_api_callback, NULL));
    }
    printf(")\n");
  }

  // Enable HSA GPU activity
  if (trace_hsa_activity) {
    hsa_async_copy_file_handle = open_output_file(output_prefix, "async_copy_trace.txt");

    // initialize HSA tracing
    roctracer::hsa_ops_properties_t ops_properties {
      table,
      reinterpret_cast<activity_async_callback_t>(hsa_activity_callback_wrapper),
      NULL,
      output_prefix
    };
    roctracer_set_properties(ACTIVITY_DOMAIN_HSA_OPS, &ops_properties);

    fprintf(stdout, "    HSA-activity-trace()\n"); fflush(stdout);
    ROCTRACER_CALL(roctracer_enable_op_activity(ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_COPY));
  }

  // Enable HIP API callbacks/activity
  if (trace_hip_api || trace_hip_activity) {
    fprintf(stdout, "    HIP-trace()\n"); fflush(stdout);
    // roctracer properties
    roctracer_set_properties(ACTIVITY_DOMAIN_HIP_API, (void*)mark_api_callback);
    // Allocating tracing pool
    open_tracing_pool();

    // Check for optimized stats
    const bool is_stats_opt = (getenv("ROCP_STATS_OPT") != NULL);

    // HIP kernel ma pinstantiation
    if (is_stats_opt) hip_kernel_map = new hip_kernel_map_t;

    // Enable tracing
    if (trace_hip_api) {
      hip_api_file_handle = open_output_file(output_prefix, "hip_api_trace.txt");
      if (hip_api_array_size != 0) {
        for (unsigned i = 0; i < hip_api_array_size; ++i) {
          uint32_t cid = HIP_API_ID_NUMBER;
          const char* api = hip_api_array[i];
          ROCTRACER_CALL(roctracer_op_code(ACTIVITY_DOMAIN_HIP_API, api, &cid, NULL));
          ROCTRACER_CALL(roctracer_enable_op_callback(ACTIVITY_DOMAIN_HIP_API, cid, hip_api_callback, NULL));
          printf(" %s", api);
          free((char*)api);
        }
        free(hip_api_array);
      } else {
        ROCTRACER_CALL(roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HIP_API, hip_api_callback, NULL));
      }

      if (is_stats_opt) {
  const char* path = NULL;
  FILE* f = open_output_file(output_prefix, "hip_api_stats.csv", &path);
        hip_api_stats = new EvtStats(f, path);
  for (uint32_t id = 0; id < HIP_API_ID_NUMBER; id += 1) {
          const char* label = roctracer_op_string(ACTIVITY_DOMAIN_HIP_API, id, 0);
          hip_api_stats->set_label(id, label);
  }
      }
    }

    if (trace_hip_activity) {
      hcc_activity_file_handle = open_output_file(output_prefix, "hcc_ops_trace.txt");
      ROCTRACER_CALL(roctracer_enable_domain_activity(ACTIVITY_DOMAIN_HCC_OPS));

      if (is_stats_opt) {
  FILE* f = NULL;
  const char* path = NULL;
  f = open_output_file(output_prefix, "hip_kernel_stats.csv", &path);
        hip_kernel_stats = new EvtStatsA(f, path);
  f = open_output_file(output_prefix, "hip_memcpy_stats.csv", &path);
        hip_memcpy_stats = new EvtStatsA(f, path);
      }
    }
  }

  // Enable PC sampling
  if (trace_pcs) {
    fprintf(stdout, "    PCS-trace()\n"); fflush(stdout);
    open_tracing_pool();
    pc_sample_file_handle = open_output_file(output_prefix, "pcs_trace.txt");
    ROCTRACER_CALL(roctracer_enable_op_activity(ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_RESERVED1));
  }

  // Dumping HSA handles for agents and pools
  FILE* handles_file_handle = open_output_file(output_prefix, "hsa_handles.txt");
  HsaRsrcFactory::Instance().DumpHandles(handles_file_handle);
  close_output_file(handles_file_handle);

  ONLOAD_TRACE_END();
  return true;
}

// HSA-runtime on-unload method
extern "C" PUBLIC_API void OnUnload() {
  ONLOAD_TRACE("");
}

extern "C" CONSTRUCTOR_API void constructor() {
  ONLOAD_TRACE_BEG();
  roctracer::hip_support::HIP_depth_max = 0;
  roctx_trace_buffer = new roctracer::TraceBuffer<roctx_trace_entry_t>("rocTX API", 0x200000, &roctx_flush_prm, 1);
  hip_api_trace_buffer = new roctracer::TraceBuffer<hip_api_trace_entry_t>("HIP API", 0x200000, &hip_api_flush_prm, 1);
  hip_act_trace_buffer = new roctracer::TraceBuffer<hip_act_trace_entry_t>("HIP ACT", 0x200000, &hip_act_flush_prm, 1, 1);
  hsa_api_trace_buffer = new roctracer::TraceBuffer<hsa_api_trace_entry_t>("HSA API", 0x200000, &hsa_flush_prm, 1);
  kfd_api_trace_buffer = new roctracer::TraceBuffer<kfd_api_trace_entry_t>("KFD API", 0x200000, &kfd_api_flush_prm, 1);
  roctracer_load();
  tool_load();
  ONLOAD_TRACE_END();
}
extern "C" DESTRUCTOR_API void destructor() {
  ONLOAD_TRACE_BEG();
  tool_unload();
  roctracer_flush_buf();
  close_file_handles();


  if (hip_api_stats) hip_api_stats->dump();
  if (hip_kernel_stats) hip_kernel_stats->dump();
  if (hip_memcpy_stats) hip_memcpy_stats->dump();

  roctracer_unload();
  ONLOAD_TRACE_END();
}
