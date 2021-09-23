#ifndef INC_ROCTRACER_TRACE_ENTRIES_H_
#define INC_ROCTRACER_TRACE_ENTRIES_H_

#include <atomic>
#include <cstdint>

#include <roctracer.h>
#include <roctracer_roctx.h>
#include <roctracer_hsa.h>
#include <roctracer_hip.h>
#include <ext/prof_protocol.h>
#include <ext/hsa_rt_utils.hpp>

typedef hsa_rt_utils::Timer::timestamp_t timestamp_t;

namespace roctracer {
enum entry_type_t {
  DFLT_ENTRY_TYPE = 0,
  API_ENTRY_TYPE = 1,
  COPY_ENTRY_TYPE = 2,
  KERNEL_ENTRY_TYPE = 3,
  NUM_ENTRY_TYPE = 4
};
}

struct roctx_trace_entry_t {
  std::atomic<uint32_t> valid;
  roctracer::entry_type_t type;
  uint32_t cid;
  timestamp_t time;
  uint32_t pid;
  uint32_t tid;
  roctx_range_id_t rid;
  const char* message;
};

struct hsa_api_trace_entry_t {
  std::atomic<uint32_t> valid;
  roctracer::entry_type_t type;
  uint32_t cid;
  timestamp_t begin;
  timestamp_t end;
  uint32_t pid;
  uint32_t tid;
  hsa_api_data_t data;
};

struct hsa_activity_trace_entry_t {
  uint64_t index;
  uint32_t op;
  uint32_t pid;
  activity_record_t *record;
  void *arg;
};

struct hip_api_trace_entry_t {
  std::atomic<uint32_t> valid;
  roctracer::entry_type_t type;
  uint32_t domain;
  uint32_t cid;
  timestamp_t begin;
  timestamp_t end;
  uint32_t pid;
  uint32_t tid;
  hip_api_data_t data;
  const char* name;
  void* ptr;
};

struct hip_activity_trace_entry_t {
  const roctracer_record_t *record;
  const char *name;
  uint32_t pid;
};

#endif