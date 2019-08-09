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

#ifndef INC_ROCTRACER_TX_H_
#define INC_ROCTRACER_TX_H_
#include <iostream>
#include <mutex>

#include <hsa.h>

#include "roctracer.h"
#include "roctx.h"
#define PUBLIC_API __attribute__((visibility("default")))

extern "C" {
typedef uint64_t roctxRangeId_t;
typedef const char* roctxStringHandle_t;

PUBLIC_API void hsaOpenROCTX(void);
PUBLIC_API void hsaCloseROCTX(void);
PUBLIC_API void roctxMarkA (roctxStringHandle_t message);
PUBLIC_API roctxRangeId_t roctxRangePushA(roctxStringHandle_t message);
PUBLIC_API roctxRangeId_t roctxRangePop(void) ;
PUBLIC_API roctxRangeId_t roctxRangeStart(roctxStringHandle_t message);
PUBLIC_API roctxRangeId_t roctxRangeEnd(roctxRangeId_t rocTxRangeID);
}
namespace roctracer {
namespace roctx {
enum {
  HSA_OP_ID_async_copy = 0
};

template <int N>
class CbTable {
  public:
  typedef std::mutex mutex_t;

  CbTable() {
    std::lock_guard<mutex_t> lck(mutex_);
    for (int i = 0; i < N; i++) {
      callback_[i] = NULL;
      arg_[i] = NULL;
    }
  }

  void set(uint32_t id, activity_rtapi_callback_t callback, void* arg) {
    std::lock_guard<mutex_t> lck(mutex_);
    callback_[id] = callback;
    arg_[id] = arg;
  }

  void get(uint32_t id, activity_rtapi_callback_t* callback, void** arg) {
    std::lock_guard<mutex_t> lck(mutex_);
    *callback = callback_[id];
    *arg = arg_[id];
  }

  private:
  activity_rtapi_callback_t callback_[N];
  void* arg_[N];
  mutex_t mutex_;
};

struct ops_properties_t {
  activity_async_callback_t async_copy_callback_fun;
  void* async_copy_callback_arg;
};

}; // namespace roctx 

typedef roctx::ops_properties_t roctx_ops_properties_t;
}; 

namespace roctracer {
namespace roctx {
template <typename T>
struct output_streamer {
  inline static std::ostream& put(std::ostream& out, const T& v) { out << v; return out; }
};
template<>
struct output_streamer<bool> {
  inline static std::ostream& put(std::ostream& out, bool v) { out << std::hex << "<bool " << "0x" << v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint8_t> {
  inline static std::ostream& put(std::ostream& out, uint8_t v) { out << std::hex << "<uint8_t " << "0x" << v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint16_t> {
  inline static std::ostream& put(std::ostream& out, uint16_t v) { out << std::hex << "<uint16_t " << "0x" << v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint32_t> {
  inline static std::ostream& put(std::ostream& out, uint32_t v) { out << std::hex << "<uint32_t " << "0x" << v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint64_t> {
  inline static std::ostream& put(std::ostream& out, uint64_t v) { out << std::hex << "<uint64_t " << "0x" << v << std::dec << ">"; return out; }
};

template<>
struct output_streamer<bool*> {
  inline static std::ostream& put(std::ostream& out, bool* v) { out << std::hex << "<bool " << "0x" << *v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint8_t*> {
  inline static std::ostream& put(std::ostream& out, uint8_t* v) { out << std::hex << "<uint8_t " << "0x" << *v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint16_t*> {
  inline static std::ostream& put(std::ostream& out, uint16_t* v) { out << std::hex << "<uint16_t " << "0x" << *v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint32_t*> {
  inline static std::ostream& put(std::ostream& out, uint32_t* v) { out << std::hex << "<uint32_t " << "0x" << *v << std::dec << ">"; return out; }
};
template<>
struct output_streamer<uint64_t*> {
  inline static std::ostream& put(std::ostream& out, uint64_t* v) { out << std::hex << "<uint64_t " << "0x" << *v << std::dec << ">"; return out; }
};

template<>
struct output_streamer<hsa_queue_t*> {
  inline static std::ostream& put(std::ostream& out, hsa_queue_t* v) { out << "<queue " << v << ">"; return out; }
};
template<>
struct output_streamer<hsa_queue_t**> {
  inline static std::ostream& put(std::ostream& out, hsa_queue_t** v) { out << "<queue " << *v << ">"; return out; }
};
// begin ostream ops for ROCTX 
#if 0
template<>
struct output_streamer<roctxMarkA&> {
  inline static std::ostream& put(std::ostream& out, roctxMarkA& v) 
{
    roctracer::roctx::output_streamer<uint32_t>::put(out,v.message);    
    return out;
} 
};
template<>
struct output_streamer<roctxRangePushA&> {
  inline static std::ostream& put(std::ostream& out, roctxRangePushA& v) {
    roctracer::roctx::output_streamer<uint32_t>::put(out,v.message);         
    return out;
} 
};
template<>
struct output_streamer<roctxRangeStart&> {
  inline static std::ostream& put(std::ostream& out, roctxRangeStart& v) {
    roctracer::roctx::output_streamer<uint32_t>::put(out,v.message);             return out;
}
};
template<>
struct output_streamer<roctxRangeEnd&> {
  inline static std::ostream& put(std::ostream& out, roctxRangeEnd& v) {
    roctracer::roctx::output_streamer<uint32_t>::put(out,v.message);             return out;
}
};
#endif
// end ostream ops for ROCTX 
};};

#endif // INC_ROCTRACER_TX_H_
