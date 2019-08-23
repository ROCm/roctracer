// automatically generated

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

/////////////////////////////////////////////////////////////////////////////

#include "kfd_prof_str.h"
inline std::ostream& operator<< (std::ostream& out, const HsaMemFlags& v) { out << "HsaMemFlags"; return out; }

// section: API callback fcts

extern "C" {
PUBLIC_API bool RegisterApiCallback(uint32_t op, void* callback, void* user_data) {
    roctracer::kfd_support::cb_table.set(op, reinterpret_cast<activity_rtapi_callback_t>(callback), user_data);
    return true;
}
PUBLIC_API bool RemoveApiCallback(uint32_t op) {
    roctracer::kfd_support::cb_table.set(op, NULL, NULL);
    return true;
}

    // block: HSAKMTAPI API
PUBLIC_API HSAKMT_STATUS hsaKmtOpenKFD(void) { roctracer::kfd_support::hsaKmtOpenKFD_callback(); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtCloseKFD(void) { roctracer::kfd_support::hsaKmtCloseKFD_callback(); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtGetVersion(HsaVersionInfo* VersionInfo) { roctracer::kfd_support::hsaKmtGetVersion_callback(VersionInfo); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtAcquireSystemProperties(HsaSystemProperties* SystemProperties) { roctracer::kfd_support::hsaKmtAcquireSystemProperties_callback(SystemProperties); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtReleaseSystemProperties(void) { roctracer::kfd_support::hsaKmtReleaseSystemProperties_callback(); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtGetNodeProperties(HSAuint32               NodeId, HsaNodeProperties* NodeProperties) { roctracer::kfd_support::hsaKmtGetNodeProperties_callback(NodeId, NodeProperties); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtGetNodeMemoryProperties(HSAuint32             NodeId, HSAuint32             NumBanks, HsaMemoryProperties* MemoryProperties) { roctracer::kfd_support::hsaKmtGetNodeMemoryProperties_callback(NodeId, NumBanks, MemoryProperties); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtGetNodeCacheProperties(HSAuint32           NodeId, HSAuint32           ProcessorId, HSAuint32           NumCaches, HsaCacheProperties* CacheProperties) { roctracer::kfd_support::hsaKmtGetNodeCacheProperties_callback(NodeId, ProcessorId, NumCaches, CacheProperties); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtGetNodeIoLinkProperties(HSAuint32            NodeId, HSAuint32            NumIoLinks, HsaIoLinkProperties* IoLinkProperties) { roctracer::kfd_support::hsaKmtGetNodeIoLinkProperties_callback(NodeId, NumIoLinks, IoLinkProperties); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtCreateEvent(HsaEventDescriptor* EventDesc, bool                ManualReset, bool                IsSignaled, HsaEvent** Event) { roctracer::kfd_support::hsaKmtCreateEvent_callback(EventDesc, ManualReset, IsSignaled, Event); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtDestroyEvent(HsaEvent* Event) { roctracer::kfd_support::hsaKmtDestroyEvent_callback(Event); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtSetEvent(HsaEvent* Event) { roctracer::kfd_support::hsaKmtSetEvent_callback(Event); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtResetEvent(HsaEvent* Event) { roctracer::kfd_support::hsaKmtResetEvent_callback(Event); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtQueryEventState(HsaEvent* Event) { roctracer::kfd_support::hsaKmtQueryEventState_callback(Event); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtWaitOnEvent(HsaEvent* Event, HSAuint32   Milliseconds) { roctracer::kfd_support::hsaKmtWaitOnEvent_callback(Event, Milliseconds); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtWaitOnMultipleEvents(HsaEvent* Events[], HSAuint32   NumEvents, bool        WaitOnAll, HSAuint32   Milliseconds) { roctracer::kfd_support::hsaKmtWaitOnMultipleEvents_callback(Events, NumEvents, WaitOnAll, Milliseconds); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtReportQueue(HSA_QUEUEID     QueueId, HsaQueueReport* QueueReport) { roctracer::kfd_support::hsaKmtReportQueue_callback(QueueId, QueueReport); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtCreateQueue(HSAuint32           NodeId, HSA_QUEUE_TYPE      Type, HSAuint32           QueuePercentage, HSA_QUEUE_PRIORITY  Priority, void* QueueAddress, HSAuint64           QueueSizeInBytes, HsaEvent* Event, HsaQueueResource* QueueResource) { roctracer::kfd_support::hsaKmtCreateQueue_callback(NodeId, Type, QueuePercentage, Priority, QueueAddress, QueueSizeInBytes, Event, QueueResource); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtUpdateQueue(HSA_QUEUEID         QueueId, HSAuint32           QueuePercentage, HSA_QUEUE_PRIORITY  Priority, void* QueueAddress, HSAuint64           QueueSize, HsaEvent* Event) { roctracer::kfd_support::hsaKmtUpdateQueue_callback(QueueId, QueuePercentage, Priority, QueueAddress, QueueSize, Event); return HSAKMT_STATUS_SUCCESS;} 
PUBLIC_API HSAKMT_STATUS hsaKmtDestroyQueue(HSA_QUEUEID         QueueId) { roctracer::kfd_support::hsaKmtDestroyQueue_callback(QueueId); return HSAKMT_STATUS_SUCCESS;} 
}
