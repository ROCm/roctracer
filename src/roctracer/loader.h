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

#ifndef SRC_CORE_LOADER_H_
#define SRC_CORE_LOADER_H_

#include <atomic>
#include <mutex>
#include <dlfcn.h>

#define ONLD_TRACE(str)                                                                            \
  if (getenv("ROCP_ONLOAD_TRACE")) do {                                                            \
      std::cout << "PID(" << GetPid() << "): TRACER_LOADER::" << __FUNCTION__ << " " << str        \
                << std::endl                                                                       \
                << std::flush;                                                                     \
    } while (0);

namespace roctracer {

// Base runtime loader class
template <class T> class BaseLoader : public T {
  static uint32_t GetPid() { return syscall(__NR_getpid); }

 public:
  typedef std::mutex mutex_t;
  typedef BaseLoader<T> loader_t;

  bool Enabled() const { return (handle_ != NULL); }

  template <class fun_t> fun_t* GetFun(const char* fun_name) {
    if (handle_ == NULL) return NULL;

    fun_t* f = (fun_t*)dlsym(handle_, fun_name);
    if ((to_check_symb_ == true) && (f == NULL)) {
      fprintf(stderr, "roctracer: symbol lookup '%s' failed: \"%s\"\n", fun_name, dlerror());
      abort();
    }
    return f;
  }

  static inline loader_t& Instance() {
    loader_t* obj = instance_.load(std::memory_order_acquire);
    if (obj == NULL) {
      std::lock_guard<mutex_t> lck(mutex_);
      if (instance_.load(std::memory_order_relaxed) == NULL) {
        obj = new loader_t();
        instance_.store(obj, std::memory_order_release);
      }
    }
    return *instance_;
  }

  static loader_t* GetRef() { return instance_; }
  static void SetLibName(const char* name) { lib_name_ = name; }

 private:
  BaseLoader() {
    const int flags = (to_load_ == true) ? RTLD_LAZY : RTLD_LAZY | RTLD_NOLOAD;
    handle_ = dlopen(lib_name_, flags);
    ONLD_TRACE("(" << lib_name_ << " = " << handle_ << ")");
    if ((to_check_open_ == true) && (handle_ == NULL)) {
      fprintf(stderr, "roctracer: Loading '%s' failed, %s\n", lib_name_, dlerror());
      abort();
    }

    T::init(this);
  }

  ~BaseLoader() {
    if (handle_ != NULL) dlclose(handle_);
  }

  static bool to_load_;
  static bool to_check_open_;
  static bool to_check_symb_;

  static mutex_t mutex_;
  static const char* lib_name_;
  static std::atomic<loader_t*> instance_;
  void* handle_;
};

// ROCprofiler library loader class
class RocpApi {
 public:
  typedef BaseLoader<RocpApi> Loader;

  typedef bool(RegisterCallback_t)(uint32_t op, void* callback, void* arg);
  typedef bool(OperateCallback_t)(uint32_t op);
  typedef bool(InitCallback_t)(void* callback, void* arg);
  typedef bool(EnableCallback_t)(uint32_t op, bool enable);
  typedef const char*(NameCallback_t)(uint32_t op);

  RegisterCallback_t* RegisterApiCallback;
  OperateCallback_t* RemoveApiCallback;
  InitCallback_t* InitActivityCallback;
  EnableCallback_t* EnableActivityCallback;
  NameCallback_t* GetOpName;

  RegisterCallback_t* RegisterEvtCallback;
  OperateCallback_t* RemoveEvtCallback;
  NameCallback_t* GetEvtName;

 protected:
  void init(Loader* loader) {
    RegisterApiCallback = loader->GetFun<RegisterCallback_t>("RegisterApiCallback");
    RemoveApiCallback = loader->GetFun<OperateCallback_t>("RemoveApiCallback");
    InitActivityCallback = loader->GetFun<InitCallback_t>("InitActivityCallback");
    EnableActivityCallback = loader->GetFun<EnableCallback_t>("EnableActivityCallback");
    GetOpName = loader->GetFun<NameCallback_t>("GetOpName");

    RegisterEvtCallback = loader->GetFun<RegisterCallback_t>("RegisterEvtCallback");
    RemoveEvtCallback = loader->GetFun<OperateCallback_t>("RemoveEvtCallback");
    GetEvtName = loader->GetFun<NameCallback_t>("GetEvtName");
  }
};

// HIP runtime library loader class
#include "roctracer_hip.h"
#if STATIC_BUILD
__attribute__((weak)) hipError_t hipRegisterApiCallback(uint32_t id, void* fun, void* arg) {
  return hipErrorUnknown;
}
__attribute__((weak)) hipError_t hipRemoveApiCallback(uint32_t id) { return hipErrorUnknown; }
__attribute__((weak)) hipError_t hipRegisterActivityCallback(uint32_t id, void* fun, void* arg) {
  return hipErrorUnknown;
}
__attribute__((weak)) hipError_t hipRemoveActivityCallback(uint32_t id) { return hipErrorUnknown; }
__attribute__((weak)) const char* hipKernelNameRef(const hipFunction_t f) { return NULL; }
__attribute__((weak)) const char* hipKernelNameRefByPtr(const void* hostFunction,
                                                        hipStream_t stream) {
  return NULL;
}
__attribute__((weak)) int hipGetStreamDeviceId(hipStream_t stream) { return 0; }
__attribute__((weak)) const char* hipApiName(uint32_t id) { return NULL; }

__attribute__((weak)) void hipInitActivityCallback(void* id_callback, void* op_callback,
                                                   void* arg) {}
__attribute__((weak)) bool hipEnableActivityCallback(unsigned op, bool enable) { return false; }
__attribute__((weak)) const char* hipGetCmdName(unsigned op) { return NULL; }

class HipLoaderStatic {
 public:
  typedef std::mutex mutex_t;
  typedef HipLoaderStatic loader_t;
  typedef std::atomic<loader_t*> instance_t;

  typedef hipError_t(RegisterApiCallback_t)(uint32_t id, void* fun, void* arg);
  typedef hipError_t(RemoveApiCallback_t)(uint32_t id);
  typedef hipError_t(RegisterActivityCallback_t)(uint32_t id, void* fun, void* arg);
  typedef hipError_t(RemoveActivityCallback_t)(uint32_t id);
  typedef const char*(KernelNameRef_t)(const hipFunction_t f);
  typedef const char*(KernelNameRefByPtr_t)(const void* hostFunction, hipStream_t stream);
  typedef int(GetStreamDeviceId_t)(hipStream_t stream);
  typedef const char*(ApiName_t)(uint32_t id);

  RegisterApiCallback_t* RegisterApiCallback;
  RemoveApiCallback_t* RemoveApiCallback;
  RegisterActivityCallback_t* RegisterActivityCallback;
  RemoveActivityCallback_t* RemoveActivityCallback;
  KernelNameRef_t* KernelNameRef;
  KernelNameRefByPtr_t* KernelNameRefByPtr_;
  const char* KernelNameRefByPtr(const void* function, hipStream_t stream = nullptr) const {
    return KernelNameRefByPtr_(function, stream);
  }
  GetStreamDeviceId_t* GetStreamDeviceId;
  ApiName_t* ApiName;

  hipInitAsyncActivityCallback_t* InitActivityCallback;
  hipEnableAsyncActivityCallback_t* EnableActivityCallback;
  hipGetOpName_t* GetOpName;

  static inline loader_t& Instance() {
    loader_t* obj = instance_.load(std::memory_order_acquire);
    if (obj == NULL) {
      std::lock_guard<mutex_t> lck(mutex_);
      if (instance_.load(std::memory_order_relaxed) == NULL) {
        obj = new loader_t();
        instance_.store(obj, std::memory_order_release);
      }
    }
    return *instance_;
  }

  bool Enabled() const { return true; }
  bool& InitActivityDone() { return init_activity_done_; }

 private:
  HipLoaderStatic() {
    RegisterApiCallback = hipRegisterApiCallback;
    RemoveApiCallback = hipRemoveApiCallback;
    RegisterActivityCallback = hipRegisterActivityCallback;
    RemoveActivityCallback = hipRemoveActivityCallback;
    KernelNameRef = hipKernelNameRef;
    KernelNameRefByPtr_ = hipKernelNameRefByPtr;
    GetStreamDeviceId = hipGetStreamDeviceId;
    ApiName = hipApiName;

    InitActivityCallback = hipInitActivityCallback;
    EnableActivityCallback = hipEnableActivityCallback;
    GetOpName = hipGetCmdName;
  }

  static mutex_t mutex_;
  static instance_t instance_;
  bool init_activity_done_ = false;
};
#else
class HipApi {
 public:
  typedef BaseLoader<HipApi> Loader;

  typedef decltype(hipRegisterApiCallback) RegisterApiCallback_t;
  typedef decltype(hipRemoveApiCallback) RemoveApiCallback_t;
  typedef decltype(hipRegisterActivityCallback) RegisterActivityCallback_t;
  typedef decltype(hipRemoveActivityCallback) RemoveActivityCallback_t;
  typedef decltype(hipKernelNameRef) KernelNameRef_t;
  typedef decltype(hipKernelNameRefByPtr) KernelNameRefByPtr_t;
  typedef decltype(hipGetStreamDeviceId) GetStreamDeviceId_t;
  typedef decltype(hipApiName) ApiName_t;

  RegisterApiCallback_t* RegisterApiCallback;
  RemoveApiCallback_t* RemoveApiCallback;
  RegisterActivityCallback_t* RegisterActivityCallback;
  RemoveActivityCallback_t* RemoveActivityCallback;
  KernelNameRef_t* KernelNameRef;
  KernelNameRefByPtr_t* KernelNameRefByPtr_;
  const char* KernelNameRefByPtr(const void* function, hipStream_t stream = nullptr) const {
    return KernelNameRefByPtr_(function, stream);
  }
  GetStreamDeviceId_t* GetStreamDeviceId;
  ApiName_t* ApiName;

  hipInitAsyncActivityCallback_t* InitActivityCallback;
  hipEnableAsyncActivityCallback_t* EnableActivityCallback;
  hipGetOpName_t* GetOpName;

  bool& InitActivityDone() { return init_activity_done_; }

 protected:
  void init(Loader* loader) {
    RegisterApiCallback = loader->GetFun<RegisterApiCallback_t>("hipRegisterApiCallback");
    RemoveApiCallback = loader->GetFun<RemoveApiCallback_t>("hipRemoveApiCallback");
    RegisterActivityCallback =
        loader->GetFun<RegisterActivityCallback_t>("hipRegisterActivityCallback");
    RemoveActivityCallback = loader->GetFun<RemoveActivityCallback_t>("hipRemoveActivityCallback");
    KernelNameRef = loader->GetFun<KernelNameRef_t>("hipKernelNameRef");
    KernelNameRefByPtr_ = loader->GetFun<KernelNameRefByPtr_t>("hipKernelNameRefByPtr");
    GetStreamDeviceId = loader->GetFun<GetStreamDeviceId_t>("hipGetStreamDeviceId");
    ApiName = loader->GetFun<ApiName_t>("hipApiName");

    InitActivityCallback =
        loader->GetFun<hipInitAsyncActivityCallback_t>("hipInitActivityCallback");
    EnableActivityCallback =
        loader->GetFun<hipEnableAsyncActivityCallback_t>("hipEnableActivityCallback");
    GetOpName = loader->GetFun<hipGetOpName_t>("hipGetCmdName");
  }

 private:
  bool init_activity_done_ = false;
};
#endif

// rocTX runtime library loader class
#include "roctracer_roctx.h"
class RocTxApi {
 public:
  typedef BaseLoader<RocTxApi> Loader;

  typedef bool(RegisterApiCallback_t)(uint32_t op, void* callback, void* arg);
  typedef bool(RemoveApiCallback_t)(uint32_t op);

  RegisterApiCallback_t* RegisterApiCallback;
  RemoveApiCallback_t* RemoveApiCallback;

 protected:
  void init(Loader* loader) {
    RegisterApiCallback = loader->GetFun<RegisterApiCallback_t>("RegisterApiCallback");
    RemoveApiCallback = loader->GetFun<RemoveApiCallback_t>("RemoveApiCallback");
  }
};

typedef BaseLoader<RocpApi> RocpLoader;
typedef BaseLoader<RocTxApi> RocTxLoader;

#if STATIC_BUILD
typedef HipLoaderStatic HipLoader;
#else
typedef BaseLoader<HipApi> HipLoaderShared;
typedef HipLoaderShared HipLoader;
#endif

}  // namespace roctracer

#define LOADER_INSTANTIATE_2()                                                                     \
  template <class T> typename roctracer::BaseLoader<T>::mutex_t roctracer::BaseLoader<T>::mutex_;  \
  template <class T> std::atomic<roctracer::BaseLoader<T>*> roctracer::BaseLoader<T>::instance_{}; \
  template <class T> bool roctracer::BaseLoader<T>::to_load_ = false;                              \
  template <class T> bool roctracer::BaseLoader<T>::to_check_open_ = true;                         \
  template <class T> bool roctracer::BaseLoader<T>::to_check_symb_ = true;                         \
  template <> const char* roctracer::RocpLoader::lib_name_ = "librocprofiler64.so";                \
  template <> bool roctracer::RocpLoader::to_load_ = true;                                         \
  template <> const char* roctracer::RocTxLoader::lib_name_ = "libroctx64.so";                     \
  template <> bool roctracer::RocTxLoader::to_load_ = true;

#if STATIC_BUILD
#define LOADER_INSTANTIATE_HIP()                                                                   \
  roctracer::HipLoaderStatic::mutex_t roctracer::HipLoaderStatic::mutex_;                          \
  roctracer::HipLoaderStatic::instance_t roctracer::HipLoaderStatic::instance_{};
#else
#define LOADER_INSTANTIATE_HIP()                                                                   \
  template <> const char* roctracer::HipLoaderShared::lib_name_ = "libamdhip64.so";
#endif

#define LOADER_INSTANTIATE()                                                                       \
  LOADER_INSTANTIATE_2();                                                                          \
  LOADER_INSTANTIATE_HIP();

#endif  // SRC_CORE_LOADER_H_
