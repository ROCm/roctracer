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

#ifndef INC_ROCTRACER_KFD_H_
#define INC_ROCTRACER_KFD_H_
#include <iostream>
#include <mutex>

#include <hsa.h>
//#include <hsa_api_trace.h>
//#include <hsa_ext_amd.h>

#include "roctracer.h"
#include "hsakmt.h"

namespace roctracer {
namespace kfd_support {
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

}; // namespace kfd_support

typedef kfd_support::ops_properties_t kfd_ops_properties_t;
}; 

namespace roctracer {
namespace kfd_support {
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
// begin ostream ops for KFD
template<>
struct output_streamer<HsaVersionInfo&> {
  inline static std::ostream& put(std::ostream& out, HsaVersionInfo& v) 
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.KernelInterfaceMajorVersion);    
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.KernelInterfaceMinorVersion);    
    return out;
} 
};
template<>
struct output_streamer<HsaSystemProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaSystemProperties& v) {
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumNodes);         
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.PlatformOem);      
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.PlatformId);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.PlatformRev);      
    return out;
} 
};
template<>
struct output_streamer<HSA_CAPABILITY&> {
  inline static std::ostream& put(std::ostream& out, HSA_CAPABILITY& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Value);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.HotPluggable);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.HSAMMUPresent);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.SharedWithGraphics);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.QueueSizePowerOfTwo);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.QueueSize32bit);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.QueueIdleEvent);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.VALimit);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.WatchPointsSupported); 	 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.WatchPointsTotalBits);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.DoorbellType);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Reserved); 
    return out;
}
};
template<>
struct output_streamer<HsaNodeProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaNodeProperties& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumCPUCores);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumFComputeCores);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumMemoryBanks);    
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumCaches);         
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumIOLinks);        
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CComputeIdLo);      
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.FComputeIdLo);      
    roctracer::kfd_support::output_streamer<HSA_CAPABILITY&>::put(out,v.Capability);        
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MaxWavesPerSIMD);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.LDSSizeInKB);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.GDSSizeInKB);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.WaveFrontSize);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumShaderBanks);    
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumArrays);         
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumCUPerArray);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumSIMDPerCU);      
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MaxSlotsScratchCU); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.EngineId);          
    roctracer::kfd_support::output_streamer<uint16_t>::put(out,v.VendorId);          
    roctracer::kfd_support::output_streamer<uint16_t>::put(out,v.DeviceId);          
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.LocationId);        
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.LocalMemSize);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MaxEngineClockMhzFCompute);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MaxEngineClockMhzCCompute);  
    roctracer::kfd_support::output_streamer<uint16_t>::put(out,v.MarketingName[HSA_PUBLIC_NAME_SIZE]);   
    return out;
} 
};
template<>
struct output_streamer<HSA_MEMORYPROPERTY&> {
  inline static std::ostream& put(std::ostream& out, HSA_MEMORYPROPERTY& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MemoryProperty);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.HotPluggable);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.NonVolatile);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Reserved);
    return out;
}
};
template<>
struct output_streamer<HsaMemoryProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaMemoryProperties& v)
{
    switch (v.HeapType){
      case HSA_HEAPTYPE_SYSTEM:                
        out << "HSA_HEAPTYPE_SYSTEM = ";
      case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:   
        out << "HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC = ";
      case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        out << "HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE = ";
      case HSA_HEAPTYPE_GPU_GDS:
        out << "HSA_HEAPTYPE_GPU_GDS = ";
      case HSA_HEAPTYPE_GPU_LDS:
        out << "HSA_HEAPTYPE_GPU_LDS = ";
      case HSA_HEAPTYPE_GPU_SCRATCH:
        out << "HSA_HEAPTYPE_GPU_SCRATCH = ";
      case HSA_HEAPTYPE_NUMHEAPTYPES:
        out << "HSA_HEAPTYPE_NUMHEAPTYPES = ";
      case HSA_HEAPTYPE_SIZE:
        out << "HSA_HEAPTYPE_SIZE = ";
    }
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.HeapType);          
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.SizeInBytes);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.SizeInBytesLow);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.SizeInBytesHigh); 
    //WIP roctracer::kfd_support::output_streamer<HSA_MEMORYPROPERTY>::put(out,v.Flags);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Width);                
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MemoryClockMax);       
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.VirtualBaseAddress);   
    return out;
}
};
template<>
struct output_streamer<HsaCacheType&> {
  inline static std::ostream& put(std::ostream& out, HsaCacheType& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Value);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Data); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Instruction); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.CPU); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.HSACU); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Reserved);
    return out;
}
};
template<>
struct output_streamer<HsaCacheProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaCacheProperties& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ProcessorIdLow);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CacheLevel);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CacheSize);        
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CacheLineSize);    
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CacheLinesPerTag); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CacheAssociativity); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CacheLatency);     
    //WIP roctracer::kfd_support::output_streamer<HsaCacheType>::put(out,v.CacheType);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.SiblingMap[HSA_CPU_SIBLINGS]);
    return out;
}
};
template<>
struct output_streamer<HsaCComputeProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaCComputeProperties& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.SiblingMap[HSA_CPU_SIBLINGS]);
    return out;
}
};
template<>
struct output_streamer<HSA_LINKPROPERTY&> {
  inline static std::ostream& put(std::ostream& out, HSA_LINKPROPERTY& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.LinkProperty);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Override);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.NonCoherent);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.NoAtomics32bit);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.NoAtomics64bit);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Reserved);
    return out;
}
};
template<>
struct output_streamer<HsaIoLinkProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaIoLinkProperties& v)
{
    switch(v.IoLinkType) {
      case HSA_IOLINKTYPE_UNDEFINED:
        out << "HSA_IOLINKTYPE_UNDEFINED = ";
      case HSA_IOLINKTYPE_HYPERTRANSPORT:
        out << "HSA_IOLINKTYPE_HYPERTRANSPORT = ";
      case HSA_IOLINKTYPE_PCIEXPRESS:
        out << "HSA_IOLINKTYPE_PCIEXPRESS = ";
      case HSA_IOLINKTYPE_AMBA: 
        out << "HSA_IOLINKTYPE_AMBA = ";
      case HSA_IOLINKTYPE_MIPI:
        out << "HSA_IOLINKTYPE_MIPI = ";
      case HSA_IOLINKTYPE_OTHER: 
        out << "HSA_IOLINKTYPE_OTHER = ";
      case HSA_IOLINKTYPE_NUMIOLINKTYPES:
        out << "HSA_IOLINKTYPE_NUMIOLINKTYPES = ";
      case HSA_IOLINKTYPE_SIZE:
        out << "HSA_IOLINKTYPE_SIZE = ";
    }
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.IoLinkType); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.VersionMajor);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.VersionMinor);       
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NodeFrom);           
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NodeTo);             
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Weight);             
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MinimumLatency);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MaximumLatency);     
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MinimumBandwidth);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.MaximumBandwidth);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.RecTransferSize);    
    roctracer::kfd_support::output_streamer<HSA_LINKPROPERTY&>::put(out,v.Flags);
    return out;
}
};
template<>
struct output_streamer<HsaMemFlags&> {
  inline static std::ostream& put(std::ostream& out, HsaMemFlags& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.NonPaged);  
    switch (v.ui32.CachePolicy){
      case HSA_CACHING_CACHED:
        out << "HSA_CACHING_CACHED = ";
      case HSA_CACHING_NONCACHED:
        out << "HSA_CACHING_NONCACHED = ";
      case HSA_CACHING_WRITECOMBINED:
        out << "HSA_CACHING_WRITECOMBINED = ";
      case HSA_CACHING_RESERVED:
        out << "HSA_CACHING_RESERVED = ";
      //case HSA_CACHING_NUM_CACHING:
        //out << "HSA_CACHING_NUM_CACHING = ";
      //case HSA_CACHING_SIZE:
        //out << "HSA_CACHING_SIZE = ";
    }
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.CachePolicy);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.ReadOnly);  
    switch(v.ui32.PageSize) {
      case HSA_PAGE_SIZE_4KB:
        out << "HSA_PAGE_SIZE_4KB = ";
      case HSA_PAGE_SIZE_64KB:
        out << "HSA_PAGE_SIZE_64KB = ";
      case HSA_PAGE_SIZE_2MB:
        out << "HSA_PAGE_SIZE_2MB = ";
      case HSA_PAGE_SIZE_1GB:
        out << "HSA_PAGE_SIZE_1GB = ";
    }
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.PageSize);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.HostAccess);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.NoSubstitute);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.GDSMemory);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Scratch);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.AtomicAccessFull);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.AtomicAccessPartial);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.ExecuteAccess);  
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Reserved); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Value);
    return out;
}
};
template<>
struct output_streamer<HsaQueueResource&> {
  inline static std::ostream& put(std::ostream& out, HsaQueueResource& v)
{
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.QueueId);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,*(v.Queue_DoorBell));
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,*(v.Queue_DoorBell_aql));
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.QueueDoorBell);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,*(v.Queue_write_ptr));
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,*(v.Queue_write_ptr_aql));
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.QueueWptrValue);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,*(v.Queue_read_ptr));
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,*(v.Queue_read_ptr_aql));
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.QueueRptrValue);
    return out;
}
};
template<>
struct output_streamer<HsaQueueReport&> {
  inline static std::ostream& put(std::ostream& out, HsaQueueReport& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.VMID);         
    out << "<void *" << v.QueueAddress << ">"; 
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.QueueSize);    return out;
}
};
template<>
struct output_streamer<HsaDbgWaveMsgAMDGen2&> {
  inline static std::ostream& put(std::ostream& out, HsaDbgWaveMsgAMDGen2& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out, v.Value);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out, v.Reserved2);
    return out;
}
};
template<>
struct output_streamer<HsaDbgWaveMessageAMD&> {
  inline static std::ostream& put(std::ostream& out, HsaDbgWaveMessageAMD& v)
{
    //WIP roctracer::kfd_support::output_streamer<HsaDbgWaveMsgAMDGen2>::put(out,v.WaveMsgInfoGen2);
    return out;
} 
};
template<>
struct output_streamer<HsaDbgWaveMessage&> {
  inline static std::ostream& put(std::ostream& out, HsaDbgWaveMessage& v)
{
    out << "<void* " << v.MemoryVA << ">";
    // WIP roctracer::kfd_support::output_streamer<HsaDbgWaveMessageAMD>::put(out,v.DbgWaveMsg);
    return out;
}
};
template<>
struct output_streamer<HsaSyncVar&> {
  inline static std::ostream& put(std::ostream& out, HsaSyncVar& v)
{
    out << "<void * " << v.SyncVar.UserData << ">";
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.SyncVar.UserDataPtrValue);   
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.SyncVarSize);
    return out;
} 
};
template<>
struct output_streamer<HsaNodeChange&> {
  inline static std::ostream& put(std::ostream& out, HsaNodeChange& v)
{
  switch(v.Flags) {
    case HSA_EVENTTYPE_NODECHANGE_ADD:
      out << "HSA_EVENTTYPE_NODECHANGE_ADD = ";
    case HSA_EVENTTYPE_NODECHANGE_REMOVE:
      out << "HSA_EVENTTYPE_NODECHANGE_REMOVE = ";
    case HSA_EVENTTYPE_NODECHANGE_SIZE:
      out << "HSA_EVENTTYPE_NODECHANGE_SIZE = ";
  }
  roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Flags);
  return out;
}
};
template<>
struct output_streamer<HsaDeviceStateChange&> {
  inline static std::ostream& put(std::ostream& out, HsaDeviceStateChange& v)
{
  roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NodeId);     
  switch(v.Device) {
    case HSA_DEVICE_CPU:
      out << "HSA_DEVICE_CPU = ";
    case HSA_DEVICE_GPU:
      out << "HSA_DEVICE_GPU = ";
    case MAX_HSA_DEVICE:
      out << "MAX_HSA_DEVICE = ";
  }
  roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Device);
  switch(v.Flags) {
    case HSA_EVENTTYPE_DEVICESTATUSCHANGE_START:
      out << "HSA_EVENTTYPE_DEVICESTATUSCHANGE_START = ";
    case HSA_EVENTTYPE_DEVICESTATUSCHANGE_STOP:
      out << "HSA_EVENTTYPE_DEVICESTATUSCHANGE_STOP = ";
    case HSA_EVENTTYPE_DEVICESTATUSCHANGE_SIZE:
      out << "HSA_EVENTTYPE_DEVICESTATUSCHANGE_SIZE = ";
  }
  roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Flags);
  return out;
}
};
template<>
struct output_streamer<HsaAccessAttributeFailure&> {
  inline static std::ostream& put(std::ostream& out, HsaAccessAttributeFailure& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NotPresent);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ReadOnly);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NoExecute);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.GpuAccess);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ECC);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Reserved);  
    return out;
}
};
template<>
struct output_streamer<HsaMemoryAccessFault&> {
  inline static std::ostream& put(std::ostream& out, HsaMemoryAccessFault& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NodeId);             
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.VirtualAddress);     
    //WIP roctracer::kfd_support::output_streamer<HsaAccessAttributeFailure>::put(out,v. Failure);            
switch(v.Flags)
{
    case HSA_EVENTID_MEMORY_RECOVERABLE:
      out << "HSA_EVENTID_MEMORY_RECOVERABLE = ";
    case HSA_EVENTID_MEMORY_FATAL_PROCESS:
      out << "HSA_EVENTID_MEMORY_FATAL_PROCESS = ";
    case HSA_EVENTID_MEMORY_FATAL_VM:
      out << "HSA_EVENTID_MEMORY_FATAL_VM = ";
}
    return out;
}
};
template<>
struct output_streamer<HsaEventData&> {
  inline static std::ostream& put(std::ostream& out, HsaEventData& v)
{
switch(v.EventType)
{
    case HSA_EVENTTYPE_SIGNAL:
      out << "HSA_EVENTTYPE_SIGNAL = ";
    case HSA_EVENTTYPE_NODECHANGE:
      out << "HSA_EVENTTYPE_NODECHANGE = ";
    case HSA_EVENTTYPE_DEVICESTATECHANGE:
      out << "HSA_EVENTTYPE_DEVICESTATECHANGE = ";
    case HSA_EVENTTYPE_HW_EXCEPTION:
      out << "HSA_EVENTTYPE_HW_EXCEPTION = ";
    case HSA_EVENTTYPE_SYSTEM_EVENT:
      out << "HSA_EVENTTYPE_SYSTEM_EVENT = ";
    case HSA_EVENTTYPE_DEBUG_EVENT:
      out << "HSA_EVENTTYPE_DEBUG_EVENT = ";
    case HSA_EVENTTYPE_PROFILE_EVENT:
      out << "HSA_EVENTTYPE_PROFILE_EVENT = ";
    case HSA_EVENTTYPE_QUEUE_EVENT:
      out << "HSA_EVENTTYPE_QUEUE_EVENT = ";
    case HSA_EVENTTYPE_MEMORY:
      out << "HSA_EVENTTYPE_MEMORY = ";
    case HSA_EVENTTYPE_MAXID:
      out << "HSA_EVENTTYPE_MAXID = ";
    case HSA_EVENTTYPE_TYPE_SIZE:
      out << "HSA_EVENTTYPE_TYPE_SIZE = ";
}
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.EventType);
    //WIP roctracer::kfd_support::output_streamer<HsaSyncVar>::put(out,v.EventData.SyncVar);
    // WIP roctracer::kfd_support::output_streamer<HsaNodeChange>::put(out,v.EventData.NodeChangeState);
    //WIP roctracer::kfd_support::output_streamer<HsaDeviceStateChange>::put(out,v.EventData.DeviceState);
    // WIP roctracer::kfd_support::output_streamer<HsaMemoryAccessFault>::put(out,v.EventData.MemoryAccessFault);
    // roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.HWData1); 
    // roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.HWData2);                    
    // roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.HWData3);                    
    return out;
}
};
template<>
struct output_streamer<HsaEventDescriptor&> {
  inline static std::ostream& put(std::ostream& out, HsaEventDescriptor& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.EventType);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NodeId);                     
    // WIP roctracer::kfd_support::output_streamer<HsaSyncVar>::put(out,v.SyncVar);     
    return out;
}
};
template<>
struct output_streamer<HsaEvent&> {
  inline static std::ostream& put(std::ostream& out, HsaEvent& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.EventId);
    // WIP roctracer::kfd_support::output_streamer<HsaEventData>::put(out,v.EventData);
    return out;
}
};
template<>
struct output_streamer<HsaClockCounters&> {
  inline static std::ostream& put(std::ostream& out, HsaClockCounters& v)
{
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.GPUClockCounter);
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.CPUClockCounter);
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.SystemClockCounter);
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.SystemClockFrequencyHz);
    return out;
}
};
template<>
struct output_streamer<HSA_UUID&> {
  inline static std::ostream& put(std::ostream& out, HSA_UUID& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Data1);
    roctracer::kfd_support::output_streamer<uint16_t>::put(out,v.Data2);
    roctracer::kfd_support::output_streamer<uint16_t>::put(out,v.Data3);
    roctracer::kfd_support::output_streamer<uint8_t>::put(out,v.Data4[8]);
    return out;
}
};
template<>
struct output_streamer<HsaCounterFlags&> {
  inline static std::ostream& put(std::ostream& out, HsaCounterFlags& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Global);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Resettable);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.ReadOnly);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Stream);   
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.ui32.Reserved); 
    roctracer::kfd_support::output_streamer<uint32_t>::put(out, v.Value);
    return out;
}
};
template<>
struct output_streamer<HsaCounter&> {
  inline static std::ostream& put(std::ostream& out, HsaCounter& v)
{
switch(v.Type)
{
    case HSA_PROFILE_TYPE_PRIVILEGED_IMMEDIATE:
      out << "HSA_PROFILE_TYPE_PRIVILEGED_IMMEDIATE = ";
    case HSA_PROFILE_TYPE_PRIVILEGED_STREAMING:
      out << "HSA_PROFILE_TYPE_PRIVILEGED_STREAMING = ";
    case HSA_PROFILE_TYPE_NONPRIV_IMMEDIATE:
      out << "HSA_PROFILE_TYPE_NONPRIV_IMMEDIATE = ";
    case HSA_PROFILE_TYPE_NONPRIV_STREAMING:
      out << "HSA_PROFILE_TYPE_NONPRIV_STREAMING = ";
    case HSA_PROFILE_TYPE_NUM:
      out << "HSA_PROFILE_TYPE_NUM = ";
    case HSA_PROFILE_TYPE_SIZE:
      out << "HSA_PROFILE_TYPE_SIZE = ";
}
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.Type);
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.CounterId);         
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.CounterSizeInBits); 
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.CounterMask);       
    // WIP roctracer::kfd_support::output_streamer<HsaCounterFlags>::put(out,v.Flags);             
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.BlockIndex);        
    return out;
}
};
template<>
struct output_streamer<HsaCounterBlockProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaCounterBlockProperties& v)
{
    // WIP roctracer::kfd_support::output_streamer<HSA_UUID>::put(out,v.BlockId);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumCounters);    
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumConcurrent);  
    // WIP roctracer::kfd_support::output_streamer<HsaCounter>::put(out,v.Counters[1]);
    return out;
}
};
template<>
struct output_streamer<HsaCounterProperties&> {
  inline static std::ostream& put(std::ostream& out, HsaCounterProperties& v)
{
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumBlocks);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumConcurrent);  
    // WIP roctracer::kfd_support::output_streamer<HsaCounterBlockProperties>::put(out,v.Blocks[1]);
    return out;
}
};
template<>
struct output_streamer<HsaPmcTraceRoot&> {
  inline static std::ostream& put(std::ostream& out, HsaPmcTraceRoot& v)
{
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.TraceBufferMinSizeBytes);
    roctracer::kfd_support::output_streamer<uint32_t>::put(out,v.NumberOfPasses);
    roctracer::kfd_support::output_streamer<uint64_t>::put(out,v.TraceId);
    return out;
}
};
// end ostream ops for KFD
};};

//#include "inc/kfd_prof_str.h"
#endif // INC_ROCTRACER_KFD_H_
