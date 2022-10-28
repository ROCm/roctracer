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

#include <cassert>
#include <sstream>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <utility>
#include <variant>

#include <cxxabi.h> /* kernel name demangling */
#include <dirent.h>
#include <hsa/hsa_api_trace.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h> /* SYS_xxx definitions */
#include <sys/types.h>
#include <unistd.h> /* usleep */


#include <roctracer_ext.h>
#include <roctracer_roctx.h>
#include <roctracer_hsa.h>
#include <roctracer_hip.h>

#include "util/xml.h"
#include "loader.h"
#include "trace_buffer.h"

// Macro to check ROC-tracer calls status
#define CHECK_ROCTRACER(call)                                                                      \
  do {                                                                                             \
    int err = call;                                                                                \
    if (err != 0) {                                                                                \
      std::cerr << roctracer_error_string() << std::endl << std::flush;                            \
      abort();                                                                                     \
    }                                                                                              \
  } while (0)

#define ONLOAD_TRACE(str)                                                                          \
  if (getenv("ROCP_ONLOAD_TRACE")) do {                                                            \
      std::cout << "PID(" << GetPid() << "): TRACER_TOOL::" << __FUNCTION__ << " " << str          \
                << std::endl                                                                       \
                << std::flush;                                                                     \
    } while (0);
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
  printf("%u:%u %s", GetPid(), GetTid(), buf);
  fflush(stdout);
  va_end(valist);
}
#else
#define DEBUG_TRACE(...)
#endif

thread_local roctracer_timestamp_t hsa_begin_timestamp = 0;
thread_local roctracer_timestamp_t hip_begin_timestamp = 0;

namespace util {

inline roctracer_timestamp_t timestamp_ns() {
  roctracer_timestamp_t timestamp;
  CHECK_ROCTRACER(roctracer_get_timestamp(&timestamp));
  return timestamp;
}

}  // namespace util

bool trace_roctx = false;
bool trace_hsa_api = false;
bool trace_hsa_activity = false;
bool trace_hip_api = false;
bool trace_hip_activity = false;
bool trace_pcs = false;

std::vector<std::string> hsa_api_vec;
std::vector<std::string> hip_api_vec;

LOADER_INSTANTIATE();
TRACE_BUFFER_INSTANTIATE();

// Global output file handle
FILE* begin_ts_file_handle = NULL;
FILE* roctx_file_handle = NULL;
FILE* hsa_api_file_handle = NULL;
FILE* hsa_async_copy_file_handle = NULL;
FILE* hip_api_file_handle = NULL;
FILE* hip_activity_file_handle = NULL;
FILE* pc_sample_file_handle = NULL;

void close_output_file(FILE* file_handle);
void close_file_handles() {
  if (begin_ts_file_handle) close_output_file(begin_ts_file_handle);
  if (roctx_file_handle) close_output_file(roctx_file_handle);
  if (hsa_api_file_handle) close_output_file(hsa_api_file_handle);
  if (hsa_async_copy_file_handle) close_output_file(hsa_async_copy_file_handle);
  if (hip_api_file_handle) close_output_file(hip_api_file_handle);
  if (hip_activity_file_handle) close_output_file(hip_activity_file_handle);
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
/* The function extracts the kernel name from
input string. By using the iterators it finds the
window in the string which contains only the kernel name.
For example 'Foo<int, float>::foo(a[], int (int))' -> 'foo'*/
std::string truncate_name(const std::string& name) {
  auto rit = name.rbegin();
  auto rend = name.rend();
  uint32_t counter = 0;
  char open_token = 0;
  char close_token = 0;
  while (rit != rend) {
    if (counter == 0) {
      switch (*rit) {
        case ')':
          counter = 1;
          open_token = ')';
          close_token = '(';
          break;
        case '>':
          counter = 1;
          open_token = '>';
          close_token = '<';
          break;
        case ']':
          counter = 1;
          open_token = ']';
          close_token = '[';
          break;
        case ' ':
          ++rit;
          continue;
      }
      if (counter == 0) break;
    } else {
      if (*rit == open_token) counter++;
      if (*rit == close_token) counter--;
    }
    ++rit;
  }
  auto rbeg = rit;
  while ((rit != rend) && (*rit != ' ') && (*rit != ':')) rit++;
  return name.substr(rend - rit, rit - rbeg);
}
// C++ symbol demangle
static inline std::string cxx_demangle(const std::string& symbol) {
  int status;
  char* demangled = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
  if (status != 0) return symbol;
  std::string ret(demangled);
  free(demangled);
  return ret;
}

// Tracing control thread
uint32_t control_delay_us = 0;
uint32_t control_len_us = 0;
uint32_t control_dist_us = 0;
std::thread* trace_period_thread = nullptr;
std::atomic_bool trace_period_stop = false;
void trace_period_fun() {
  std::this_thread::sleep_for(std::chrono::microseconds(control_delay_us));
  do {
    roctracer_start();
    if (trace_period_stop) {
      roctracer_stop();
      break;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(control_len_us));
    roctracer_stop();
    if (trace_period_stop) break;
    std::this_thread::sleep_for(std::chrono::microseconds(control_dist_us));
  } while (!trace_period_stop);
}

// Flushing control thread
uint32_t control_flush_us = 0;
std::thread* flush_thread = nullptr;
std::atomic_bool stop_flush_thread = false;


void flush_thr_fun() {
  while (!stop_flush_thread) {
    CHECK_ROCTRACER(roctracer_flush_activity());
    roctracer::TraceBufferBase::FlushAll();
    std::this_thread::sleep_until(std::chrono::steady_clock::now() +
                                  std::chrono::microseconds(control_flush_us));
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// rocTX annotation tracing

struct roctx_trace_entry_t {
  std::atomic<roctracer::TraceEntryState> valid;
  uint32_t cid;
  roctracer_timestamp_t time;
  uint32_t pid;
  uint32_t tid;
  roctx_range_id_t rid;
  const char* message;

  roctx_trace_entry_t(uint32_t cid_, roctracer_timestamp_t time_, uint32_t pid_, uint32_t tid_,
                      roctx_range_id_t rid_, const char* message_)
      : valid(roctracer::TRACE_ENTRY_INIT),
        cid(cid_),
        time(time_),
        pid(pid_),
        tid(tid_),
        rid(rid_),
        message(message_ != nullptr ? strdup(message_) : nullptr) {}
  ~roctx_trace_entry_t() {
    if (message != nullptr) free(const_cast<char*>(message));
  }
};

// rocTX buffer flush function
void roctx_flush_cb(roctx_trace_entry_t* entry) {
  std::ostringstream os;
  os << entry->time << " " << entry->pid << ":" << entry->tid << " " << entry->cid << ":"
     << entry->rid;
  if (entry->message != NULL)
    os << ":\"" << entry->message << "\"";
  else
    os << ":\"\"";
  fprintf(roctx_file_handle, "%s\n", os.str().c_str());
  fflush(roctx_file_handle);
}

roctracer::TraceBuffer<roctx_trace_entry_t> roctx_trace_buffer("rocTX API", 0x200000,
                                                               roctx_flush_cb);

// rocTX callback function
void roctx_api_callback(uint32_t domain, uint32_t cid, const void* callback_data,
                        void* /* user_arg */) {
  const roctx_api_data_t* data = reinterpret_cast<const roctx_api_data_t*>(callback_data);

  roctx_trace_entry_t& entry = roctx_trace_buffer.Emplace(
      cid, util::timestamp_ns(), GetPid(), GetTid(), data->args.id, data->args.message);
  entry.valid.store(roctracer::TRACE_ENTRY_COMPLETE, std::memory_order_release);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// HSA API tracing

struct hsa_api_trace_entry_t {
  std::atomic<uint32_t> valid;
  uint32_t cid;
  roctracer_timestamp_t begin;
  roctracer_timestamp_t end;
  uint32_t pid;
  uint32_t tid;
  hsa_api_data_t data;

  hsa_api_trace_entry_t(uint32_t cid_, roctracer_timestamp_t begin_, roctracer_timestamp_t end_,
                        uint32_t pid_, uint32_t tid_, const hsa_api_data_t& data_)
      : valid(roctracer::TRACE_ENTRY_INIT),
        cid(cid_),
        begin(begin_),
        end(end_),
        pid(pid_),
        tid(tid_),
        data(data_) {}
  ~hsa_api_trace_entry_t() {}
};

void hsa_api_flush_cb(hsa_api_trace_entry_t* entry) {
  std::ostringstream os;
  os << entry->begin << ":" << entry->end << " " << entry->pid << ":" << entry->tid << " "
     << hsa_api_data_pair_t(entry->cid, entry->data) << " :" << entry->data.correlation_id;
  fprintf(hsa_api_file_handle, "%s\n", os.str().c_str());
  fflush(hsa_api_file_handle);
}

roctracer::TraceBuffer<hsa_api_trace_entry_t> hsa_api_trace_buffer("HSA API", 0x200000,
                                                                   hsa_api_flush_cb);

// HSA API callback function

void hsa_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
  (void)arg;
  const hsa_api_data_t* data = reinterpret_cast<const hsa_api_data_t*>(callback_data);
  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    hsa_begin_timestamp = util::timestamp_ns();
  } else {
    const roctracer_timestamp_t end_timestamp =
        (cid == HSA_API_ID_hsa_shut_down) ? hsa_begin_timestamp : util::timestamp_ns();
    hsa_api_trace_entry_t& entry = hsa_api_trace_buffer.Emplace(
        cid, hsa_begin_timestamp, end_timestamp, GetPid(), GetTid(), *data);
    entry.valid.store(roctracer::TRACE_ENTRY_COMPLETE, std::memory_order_release);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// HIP API tracing

struct hip_api_trace_entry_t {
  std::atomic<uint32_t> valid;
  activity_domain_t domain;
  uint32_t cid;
  roctracer_timestamp_t begin;
  roctracer_timestamp_t end;
  uint32_t pid;
  uint32_t tid;
  hip_api_data_t data;
  const char* name;
  void* ptr;

  hip_api_trace_entry_t(activity_domain_t domain_, uint32_t cid_, roctracer_timestamp_t begin_,
                        roctracer_timestamp_t end_, uint32_t pid_, uint32_t tid_,
                        const hip_api_data_t& data_, const char* name_, void* ptr_)
      : valid(roctracer::TRACE_ENTRY_INIT),
        domain(domain_),
        cid(cid_),
        begin(begin_),
        end(end_),
        pid(pid_),
        tid(tid_),
        data(data_),
        name(name_ != nullptr ? strdup(name_) : nullptr),
        ptr(ptr_) {}

  ~hip_api_trace_entry_t() {
    if (name != nullptr) free(const_cast<char*>(name));
  }
};

static std::string getKernelNameMultiKernelMultiDevice(hipLaunchParams* launchParamsList,
                                                       int numDevices) {
  std::stringstream name_str;
  for (int i = 0; i < numDevices; ++i) {
    if (launchParamsList[i].func != nullptr) {
      name_str << roctracer::HipLoader::Instance().KernelNameRefByPtr(launchParamsList[i].func)
               << ":"
               << roctracer::HipLoader::Instance().GetStreamDeviceId(launchParamsList[i].stream)
               << ";";
    }
  }
  return name_str.str();
}

template <typename... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;


static std::optional<std::string> getKernelName(uint32_t cid, const hip_api_data_t* data) {
  std::variant<const void*, hipFunction_t> function;
  switch (cid) {
    case HIP_API_ID_hipExtLaunchMultiKernelMultiDevice: {
      return getKernelNameMultiKernelMultiDevice(
          data->args.hipExtLaunchMultiKernelMultiDevice.launchParamsList,
          data->args.hipExtLaunchMultiKernelMultiDevice.numDevices);
    }
    case HIP_API_ID_hipLaunchCooperativeKernelMultiDevice: {
      return getKernelNameMultiKernelMultiDevice(
          data->args.hipLaunchCooperativeKernelMultiDevice.launchParamsList,
          data->args.hipLaunchCooperativeKernelMultiDevice.numDevices);
    }
    case HIP_API_ID_hipLaunchKernel: {
      function = data->args.hipLaunchKernel.function_address;
      break;
    }
    case HIP_API_ID_hipExtLaunchKernel: {
      function = data->args.hipExtLaunchKernel.function_address;
      break;
    }
    case HIP_API_ID_hipLaunchCooperativeKernel: {
      function = data->args.hipLaunchCooperativeKernel.f;
      break;
    }
    case HIP_API_ID_hipLaunchByPtr: {
      function = data->args.hipLaunchByPtr.hostFunction;
      break;
    }
    case HIP_API_ID_hipGraphAddKernelNode: {
      function = data->args.hipGraphAddKernelNode.pNodeParams->func;
      break;
    }
    case HIP_API_ID_hipGraphExecKernelNodeSetParams: {
      function = data->args.hipGraphExecKernelNodeSetParams.pNodeParams->func;
      break;
    }
    case HIP_API_ID_hipGraphKernelNodeSetParams: {
      function = data->args.hipGraphKernelNodeSetParams.pNodeParams->func;
      break;
    }
    case HIP_API_ID_hipModuleLaunchKernel: {
      function = data->args.hipModuleLaunchKernel.f;
      break;
    }
    case HIP_API_ID_hipExtModuleLaunchKernel: {
      function = data->args.hipExtModuleLaunchKernel.f;
      break;
    }
    case HIP_API_ID_hipHccModuleLaunchKernel: {
      function = data->args.hipHccModuleLaunchKernel.f;
      break;
    }
    default:
      return {};
  }
  return std::visit(
      Overloaded{
          [](const void* func) {
            return roctracer::HipLoader::Instance().KernelNameRefByPtr(func);
          },
          [](hipFunction_t func) { return roctracer::HipLoader::Instance().KernelNameRef(func); },
      },
      function);
}

void hip_api_flush_cb(hip_api_trace_entry_t* entry) {
  const uint32_t domain = entry->domain;
  const uint32_t cid = entry->cid;
  const hip_api_data_t* data = &(entry->data);
  const uint64_t correlation_id = data->correlation_id;
  const roctracer_timestamp_t begin_timestamp = entry->begin;
  const roctracer_timestamp_t end_timestamp = entry->end;
  std::ostringstream rec_ss;
  std::ostringstream oss;

  const char* str =
      (domain != ACTIVITY_DOMAIN_EXT_API) ? roctracer_op_string(domain, cid, 0) : strdup("MARK");
  rec_ss << std::dec << begin_timestamp << ":" << end_timestamp << " " << entry->pid << ":"
         << entry->tid;
  oss << std::dec << rec_ss.str() << " " << str;

  DEBUG_TRACE(
      "hip_api_flush_cb(\"%s\"): domain(%u) cid(%u) entry(%p) name(\"%s\" correlation_id(%lu) "
      "beg(%lu) end(%lu))\n",
      roctracer_op_string(entry->domain, entry->cid, 0), entry->domain, entry->cid, entry,
      entry->name, correlation_id, begin_timestamp, end_timestamp);

  if (domain == ACTIVITY_DOMAIN_HIP_API) {
    const char* str = hipApiString((hip_api_id_t)cid, data);
    rec_ss << " " << str;
    if (entry->name) {
      static bool truncate = []() {
        const char* env_var = getenv("ROCP_TRUNCATE_NAMES");
        return env_var && std::atoi(env_var) != 0;
      }();

      std::string kernel_name(cxx_demangle(entry->name));
      if (truncate) kernel_name = truncate_name(kernel_name);
      rec_ss << " kernel=" << kernel_name;
    }
    rec_ss << " :" << correlation_id;
    fprintf(hip_api_file_handle, "%s\n", rec_ss.str().c_str());
  } else {
    fprintf(hip_api_file_handle, "%s(name(%s))\n", oss.str().c_str(), entry->name);
  }
  fflush(hip_api_file_handle);
}

roctracer::TraceBuffer<hip_api_trace_entry_t> hip_api_trace_buffer("HIP API", 0x200000,
                                                                   hip_api_flush_cb);

void hip_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
  (void)arg;
  const hip_api_data_t* data = reinterpret_cast<const hip_api_data_t*>(callback_data);
  const roctracer_timestamp_t timestamp = util::timestamp_ns();

  if (data->phase == ACTIVITY_API_PHASE_ENTER) {
    hip_begin_timestamp = timestamp;
  } else {
    // Post init of HIP APU args
    hipApiArgsInit((hip_api_id_t)cid, const_cast<hip_api_data_t*>(data));
    auto kernel_name = getKernelName(cid, data);
    hip_api_trace_entry_t& entry = hip_api_trace_buffer.Emplace(
        static_cast<activity_domain_t>(domain), cid, hip_begin_timestamp, timestamp, GetPid(),
        GetTid(), *data, kernel_name ? kernel_name->c_str() : nullptr,
        cid == HIP_API_ID_hipMalloc ? data->args.hipMalloc.ptr : nullptr);
    entry.valid.store(roctracer::TRACE_ENTRY_COMPLETE, std::memory_order_release);
  }

  DEBUG_TRACE(
      "hip_api_callback(\"%s\") phase(%d): cid(%u) data(%p) entry(%p) name(\"%s\") "
      "correlation_id(%lu) timestamp(%lu)\n",
      roctracer_op_string(domain, cid, 0), data->phase, cid, data, entry,
      (entry.name != nullptr) ? entry.name : "", data->correlation_id, timestamp);
}

void mark_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
  (void)arg;
  const char* name = reinterpret_cast<const char*>(callback_data);

  const roctracer_timestamp_t timestamp = util::timestamp_ns();
  hip_api_trace_entry_t& entry = hip_api_trace_buffer.Emplace(
      static_cast<activity_domain_t>(domain), cid, timestamp, timestamp + 1, GetPid(), GetTid(),
      hip_api_data_t{}, name, nullptr);
  entry.valid.store(roctracer::TRACE_ENTRY_COMPLETE, std::memory_order_release);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Activity tracing callback
//   hipMalloc id(3) correlation_id(1): begin_ns(1525888652762640464) end_ns(1525888652762877067)
void pool_activity_callback(const char* begin, const char* end, void* arg) {
  const roctracer_record_t* record = reinterpret_cast<const roctracer_record_t*>(begin);
  const roctracer_record_t* end_record = reinterpret_cast<const roctracer_record_t*>(end);

  while (record < end_record) {
    const char* name = roctracer_op_string(record->domain, record->op, record->kind);
    DEBUG_TRACE(
        "pool_activity_callback(\"%s\"): domain(%u) op(%u) kind(%u) record(%p) correlation_id(%lu) "
        "beg(%lu) end(%lu)\n",
        name, record->domain, record->op, record->kind, record, record->correlation_id,
        record->begin_ns, record->end_ns);

    switch (record->domain) {
      case ACTIVITY_DOMAIN_HIP_OPS:
        fprintf(hip_activity_file_handle, "%lu:%lu %d:%lu %s:%lu:%u\n", record->begin_ns,
                record->end_ns, record->device_id, record->queue_id, name, record->correlation_id,
                my_pid);
        fflush(hip_activity_file_handle);
        break;
      case ACTIVITY_DOMAIN_HSA_OPS:
        if (record->op == HSA_OP_ID_COPY) {
          fprintf(hsa_async_copy_file_handle, "%lu:%lu async-copy:%lu:%u\n", record->begin_ns,
                  record->end_ns, record->correlation_id, my_pid);
          fflush(hsa_async_copy_file_handle);
        } else if (record->op == HSA_OP_ID_RESERVED1) {
          fprintf(pc_sample_file_handle, "%u %lu 0x%lx %s\n", record->pc_sample.se,
                  record->pc_sample.cycle, record->pc_sample.pc, name);
          fflush(pc_sample_file_handle);
        }
        break;
    }
    CHECK_ROCTRACER(roctracer_next_record(record, &record));
  }
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
    if (last_pos == std::string::npos)
      error_str = "token string error: \"" + token + "\"";
    else {
      const size_t end_pos = last_pos + 1;
      if (end_pos <= first_pos)
        error_str = "token string error: \"" + token + "\"";
      else
        norm_len = end_pos - first_pos;
    }
  }
  if (((first_pos != std::string::npos) && (norm_len == 0)) ||
      ((first_pos == std::string::npos) && not_empty)) {
    fatal("normalize_token error, " + label + ": '" + token + "'," + error_str);
  }
  return (norm_len != 0) ? token.substr(first_pos, norm_len) : std::string("");
}

int get_xml_array(const xml::Xml::level_t* node, const std::string& field, const std::string& delim,
                  std::vector<std::string>* vec, const char* label = NULL) {
  int parse_iter = 0;
  const auto& opts = node->opts;
  auto it = opts.find(field);
  if (it != opts.end()) {
    const std::string array_string = it->second;
    if (label != NULL) printf("%s%s = %s\n", label, field.c_str(), array_string.c_str());
    size_t pos1 = 0;
    const size_t string_len = array_string.length();
    while (pos1 < string_len) {
      // set pos2 such that it also handles case of multiple delimiter options.
      // For example-  "hipLaunchKernel, hipExtModuleLaunchKernel, hipMemsetAsync"
      // in this example delimiters are ' ' and also ','
      const size_t pos2 = array_string.find_first_of(delim, pos1);
      const bool found = (pos2 != std::string::npos);
      const size_t token_len = (pos2 != std::string::npos) ? pos2 - pos1 : string_len - pos1;
      const std::string token = array_string.substr(pos1, token_len);
      const std::string norm_str = normalize_token(token, found, "get_xml_array");
      if (norm_str.length() != 0) vec->push_back(norm_str);
      if (!found) break;
      // update pos2 such that it represents the first non-delimiter character
      // in case multiple delimiters are specified in variable 'delim'
      pos1 = array_string.find_first_not_of(delim, pos2);
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
  } else
    file_handle = stdout;
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
    CHECK_ROCTRACER(roctracer_open_pool(&properties));
  }
}

// Flush tracing pool
void close_tracing_pool() {
  if (roctracer_pool_t* pool = roctracer_default_pool(); pool != nullptr) {
    CHECK_ROCTRACER(roctracer_flush_activity_expl(pool));
    CHECK_ROCTRACER(roctracer_close_pool_expl(pool));
  }
}

// tool library is loaded
static bool is_loaded = false;

// tool unload method
void tool_unload() {
  ONLOAD_TRACE("begin, loaded(" << is_loaded << ")");

  if (is_loaded == false) return;
  is_loaded = false;

  if (flush_thread) {
    stop_flush_thread = true;
    flush_thread->join();
    delete flush_thread;
    flush_thread = nullptr;
  }

  if (trace_period_thread) {
    trace_period_stop = true;
    trace_period_thread->join();
    delete trace_period_thread;
    trace_period_thread = nullptr;
  }

  if (trace_roctx) {
    CHECK_ROCTRACER(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_ROCTX));
  }
  if (trace_hsa_api) {
    CHECK_ROCTRACER(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HSA_API));
  }
  if (trace_hsa_activity || trace_pcs) {
    CHECK_ROCTRACER(roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HSA_OPS));
  }
  if (trace_hip_api || trace_hip_activity) {
    CHECK_ROCTRACER(roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HIP_API));
    CHECK_ROCTRACER(roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HIP_API));
    CHECK_ROCTRACER(roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HIP_OPS));
  }

  // Flush tracing pool
  close_tracing_pool();
  roctracer::TraceBufferBase::FlushAll();

  close_file_handles();

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

    // PC sampling enabling
    if (std::string(trace_domain).find("pcs") != std::string::npos) {
      trace_pcs = true;
    }
  }

  printf("ROCTracer (pid=%d): ", (int)GetPid());
  fflush(stdout);

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
        if (node->tag != "parameters")
          fatal("ROCTracer: trace node is not supported '" + name + ":" + node->tag + "'");
        get_xml_array(node, "api", ", ",
                      &api_vec);  // delimiter options given as both spaces and commas (' ' and ',')
        break;
      }

      if (name == "rocTX") {
        found = true;
        trace_roctx = true;
      }
      if (name == "HSA") {
        found = true;
        trace_hsa_api = true;
        hsa_api_vec = api_vec;
      }
      if (name == "GPU") {
        found = true;
        trace_hsa_activity = true;
      }
      if (name == "HIP") {
        found = true;
        trace_hip_api = true;
        trace_hip_activity = true;
        hip_api_vec = api_vec;
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
    fprintf(stdout, "    rocTX-trace()\n");
    fflush(stdout);
    CHECK_ROCTRACER(
        roctracer_enable_domain_callback(ACTIVITY_DOMAIN_ROCTX, roctx_api_callback, NULL));
  }

  const char* ctrl_str = getenv("ROCP_CTRL_RATE");
  if (ctrl_str != NULL) {
    uint32_t ctrl_delay = 0;
    uint32_t ctrl_len = 0;
    uint32_t ctrl_rate = 0;

    if (sscanf(ctrl_str, "%d:%d:%d", &ctrl_delay, &ctrl_len, &ctrl_rate) != 3 ||
        ctrl_len > ctrl_rate)
      fatal("Invalid ROCP_CTRL_RATE variable (ctrl_delay:ctrl_len:ctrl_rate)");

    control_dist_us = ctrl_rate - ctrl_len;
    control_len_us = ctrl_len;
    control_delay_us = ctrl_delay;

    roctracer_stop();

    if (ctrl_delay != UINT32_MAX) {
      fprintf(stdout, "ROCTracer: trace control: delay(%uus), length(%uus), rate(%uus)\n",
              ctrl_delay, ctrl_len, ctrl_rate);
      fflush(stdout);
      trace_period_thread = new std::thread(trace_period_fun);
    } else {
      fprintf(stdout, "ROCTracer: trace start disabled\n");
      fflush(stdout);
    }
  }

  const char* flush_str = getenv("ROCP_FLUSH_RATE");
  if (flush_str != NULL) {
    sscanf(flush_str, "%d", &control_flush_us);
    if (control_flush_us == 0) {
      fprintf(stderr, "ROCTracer: control flush rate bad value\n");
      abort();
    }

    fprintf(stdout, "ROCTracer: trace control flush rate(%uus)\n", control_flush_us);
    fflush(stdout);
    flush_thread = new std::thread(flush_thr_fun);
  }

  ONLOAD_TRACE_END();
}

extern "C" {

// The HSA_AMD_TOOL_PRIORITY variable must be a constant value type initialized by the loader
// itself, not by code during _init. 'extern const' seems do that although that is not a guarantee.
ROCTRACER_EXPORT extern const uint32_t HSA_AMD_TOOL_PRIORITY = 1050;

// HSA-runtime tool on-load method
ROCTRACER_EXPORT bool OnLoad(HsaApiTable* table, uint64_t runtime_version,
                             uint64_t failed_tool_count, const char* const* failed_tool_names) {
  ONLOAD_TRACE_BEG();

  tool_load();

  // OnUnload may not be called if the ROC runtime is not shutdown by the client
  // application before exiting, so register an atexit handler to unload the tool.
  std::atexit(tool_unload);

  const char* output_prefix = getenv("ROCP_OUTPUT_DIR");

  // Dumping HSA handles for agents
  FILE* handles_file_handle = open_output_file(output_prefix, "hsa_handles.txt");
  auto iterate_agent_data = std::make_pair(table, handles_file_handle);

  [[maybe_unused]] hsa_status_t status = table->core_->hsa_iterate_agents_fn(
      [](hsa_agent_t agent, void* user_data) {
        auto [hsa_api_table, hsa_handles_file] =
            *reinterpret_cast<decltype(iterate_agent_data)*>(user_data);
        hsa_device_type_t type;

        if (hsa_api_table->core_->hsa_agent_get_info_fn(agent, HSA_AGENT_INFO_DEVICE, &type) !=
            HSA_STATUS_SUCCESS)
          return HSA_STATUS_ERROR;

        fprintf(hsa_handles_file, "0x%lx agent %s\n", agent.handle,
                (type == HSA_DEVICE_TYPE_CPU) ? "cpu" : "gpu");
        return HSA_STATUS_SUCCESS;
      },
      &iterate_agent_data);
  assert(status == HSA_STATUS_SUCCESS && "failed to iterate HSA agents");

  close_output_file(handles_file_handle);

  // App begin timestamp begin_ts_file.txt
  begin_ts_file_handle = open_output_file(output_prefix, "begin_ts_file.txt");
  const roctracer_timestamp_t app_start_time = util::timestamp_ns();
  fprintf(begin_ts_file_handle, "%lu\n", app_start_time);

  // Enable HSA API callbacks/activity
  if (trace_hsa_api) {
    hsa_api_file_handle = open_output_file(output_prefix, "hsa_api_trace.txt");

    fprintf(stdout, "    HSA-trace(");
    fflush(stdout);
    if (hsa_api_vec.size() != 0) {
      for (unsigned i = 0; i < hsa_api_vec.size(); ++i) {
        uint32_t cid = HSA_API_ID_NUMBER;
        const char* api = hsa_api_vec[i].c_str();
        CHECK_ROCTRACER(roctracer_op_code(ACTIVITY_DOMAIN_HSA_API, api, &cid, NULL));
        CHECK_ROCTRACER(
            roctracer_enable_op_callback(ACTIVITY_DOMAIN_HSA_API, cid, hsa_api_callback, NULL));
        printf(" %s", api);
      }
    } else {
      CHECK_ROCTRACER(
          roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HSA_API, hsa_api_callback, NULL));
    }
    printf(")\n");
  }

  // Enable HSA GPU activity
  if (trace_hsa_activity) {
    hsa_async_copy_file_handle = open_output_file(output_prefix, "async_copy_trace.txt");

    // Allocating tracing pool
    open_tracing_pool();

    fprintf(stdout, "    HSA-activity-trace()\n");
    fflush(stdout);
    CHECK_ROCTRACER(roctracer_enable_op_activity(ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_COPY));
  }

  // Enable HIP API callbacks/activity
  if (trace_hip_api || trace_hip_activity) {
    fprintf(stdout, "    HIP-trace()\n");
    fflush(stdout);
    // roctracer properties
    roctracer_set_properties(ACTIVITY_DOMAIN_HIP_API, (void*)mark_api_callback);
    // Allocating tracing pool
    open_tracing_pool();

    // Enable tracing
    if (trace_hip_api) {
      hip_api_file_handle = open_output_file(output_prefix, "hip_api_trace.txt");
      if (hip_api_vec.size() != 0) {
        for (unsigned i = 0; i < hip_api_vec.size(); ++i) {
          uint32_t cid = HIP_API_ID_NONE;
          const char* api = hip_api_vec[i].c_str();
          CHECK_ROCTRACER(roctracer_op_code(ACTIVITY_DOMAIN_HIP_API, api, &cid, NULL));
          CHECK_ROCTRACER(
              roctracer_enable_op_callback(ACTIVITY_DOMAIN_HIP_API, cid, hip_api_callback, NULL));
          printf(" %s", api);
        }
      } else {
        CHECK_ROCTRACER(
            roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HIP_API, hip_api_callback, NULL));
      }
    }

    if (trace_hip_activity) {
      hip_activity_file_handle = open_output_file(output_prefix, "hcc_ops_trace.txt");
      CHECK_ROCTRACER(roctracer_enable_domain_activity(ACTIVITY_DOMAIN_HIP_OPS));
    }
  }

  // Enable PC sampling
  if (trace_pcs) {
    fprintf(stdout, "    PCS-trace()\n");
    fflush(stdout);
    open_tracing_pool();
    pc_sample_file_handle = open_output_file(output_prefix, "pcs_trace.txt");
    CHECK_ROCTRACER(roctracer_enable_op_activity(ACTIVITY_DOMAIN_HSA_OPS, HSA_OP_ID_RESERVED1));
  }

  ONLOAD_TRACE_END();
  return true;
}

// HSA-runtime on-unload method
ROCTRACER_EXPORT void OnUnload() {
  ONLOAD_TRACE_BEG();
  tool_unload();
  ONLOAD_TRACE_END();
}

}  // extern "C"
