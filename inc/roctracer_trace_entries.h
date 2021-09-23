#ifndef INC_ROCTRACER_TRACE_ENTRIES_H_
#define INC_ROCTRACER_TRACE_ENTRIES_H_

#include <atomic>
#include <cstdint>

#include <roctracer_roctx.h>
#include <roctracer_hsa.h>
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

#endif