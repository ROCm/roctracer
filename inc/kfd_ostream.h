PUBLIC_API HSAKMT_STATUS HSAKMTAPI hsaKmtOpenKFD( void ) {roctracer::kfd_support::hsaKmtOpenKFD_callback();}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtCloseKFD( void ) {roctracer::kfd_support::hsaKmtCloseKFD_callback();}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtGetVersion( HsaVersionInfo* VersionInfo ) {roctracer::kfd_support::hsaKmtGetVersion_callback(VersionInfo);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtAcquireSystemProperties( HsaSystemProperties* SystemProperties ) {roctracer::kfd_support::hsaKmtAcquireSystemProperties_callback(SystemProperties);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtReleaseSystemProperties( void ) {}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtGetNodeProperties( HSAuint32 NodeId, HsaNodeProperties* NodeProperties ) {roctracer::kfd_support::hsaKmtGetNodeProperties_callback(NodeId, NodeProperties);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtGetNodeMemoryProperties( HSAuint32 NodeId, HSAuint32 NumBanks, HsaMemoryProperties* MemoryProperties ) {roctracer::kfd_support::hsaKmtGetNodeMemoryProperties_callback(NodeId, NumBanks, MemoryProperties);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtGetNodeCacheProperties( HSAuint32 NodeId, HSAuint32 ProcessorId, HSAuint32 NumCaches, HsaCacheProperties* CacheProperties ) {roctracer::kfd_support::hsaKmtGetNodeCacheProperties_callback(NodeId, ProcessorId, NumCaches, CacheProperties);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtGetNodeIoLinkProperties( HSAuint32 NodeId, HSAuint32 NumIoLinks, HsaIoLinkProperties* IoLinkProperties ) {roctracer::kfd_support::hsaKmtGetNodeIoLinkProperties_callback(NodeId, NumIoLinks, IoLinkProperties);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtCreateEvent( HsaEventDescriptor* EventDesc, bool ManualReset, bool IsSignaled, HsaEvent** Event ) {roctracer::kfd_support::hsaKmtCreateEvent_callback(EventDesc, ManualReset, IsSignaled, Event);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDestroyEvent( HsaEvent* Event ) {roctracer::kfd_support::hsaKmtDestroyEvent_callback(Event);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtSetEvent( HsaEvent* Event ) {roctracer::kfd_support::hsaKmtSetEvent_callback(Event);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtResetEvent( HsaEvent* Event ) {roctracer::kfd_support::hsaKmtResetEvent_callback(Event);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtQueryEventState( HsaEvent* Event ) {roctracer::kfd_support::hsaKmtQueryEventState_callback(Event);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtWaitOnEvent( HsaEvent* Event, HSAuint32 Milliseconds ) {roctracer::kfd_support::hsaKmtWaitOnEvent_callback(Event, Milliseconds);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtWaitOnMultipleEvents( HsaEvent* Events[], HSAuint32 NumEvents, bool WaitOnAll, HSAuint32 Milliseconds ) {roctracer::kfd_support::hsaKmtWaitOnMultipleEvents_callback(Events, NumEvents, WaitOnAll, Milliseconds);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtReportQueue( HSA_QUEUEID QueueId, HsaQueueReport* QueueReport ) {roctracer::kfd_support::hsaKmtReportQueue_callback(QueueId, QueueReport);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtCreateQueue( HSAuint32 NodeId, HSA_QUEUE_TYPE Type, HSAuint32 QueuePercentage, HSA_QUEUE_PRIORITY Priority, void* QueueAddress, HSAuint64 QueueSizeInBytes, HsaEvent* Event, HsaQueueResource* QueueResource ) {roctracer::kfd_support::hsaKmtCreateQueue_callback(NodeId, Type, QueuePercentage, Priority, QueueAddress, QueueSizeInBytes, Event, QueueResource);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtUpdateQueue( HSA_QUEUEID QueueId, HSAuint32 QueuePercentage, HSA_QUEUE_PRIORITY Priority, void* QueueAddress, HSAuint64 QueueSize, HsaEvent* Event ) {roctracer::kfd_support::hsaKmtUpdateQueue_callback(QueueId, QueuePercentage, Priority, QueueAddress, QueueSize, Event);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDestroyQueue( HSA_QUEUEID QueueId ) {roctracer::kfd_support::hsaKmtDestroyQueue_callback(QueueId);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtSetMemoryPolicy( HSAuint32 Node, HSAuint32 DefaultPolicy, 	 	 HSAuint32 AlternatePolicy, 	 void* MemoryAddressAlternate, HSAuint64 MemorySizeInBytes 	 ) {roctracer::kfd_support::hsaKmtSetMemoryPolicy_callback(Node, DefaultPolicy, AlternatePolicy, MemoryAddressAlternate, MemorySizeInBytes);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI   hsaKmtAllocMemory( HSAuint32 PreferredNode, HSAuint64 SizeInBytes, HsaMemFlags MemFlags, void** MemoryAddress ) {roctracer::kfd_support::hsaKmtAllocMemory_callback(PreferredNode, SizeInBytes, MemFlags, MemoryAddress);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtFreeMemory( void* MemoryAddress, HSAuint64 SizeInBytes ) {roctracer::kfd_support::hsaKmtFreeMemory_callback(MemoryAddress, SizeInBytes);} 
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtRegisterMemory( void* MemoryAddress, HSAuint64 MemorySizeInBytes ) {roctracer::kfd_support::hsaKmtRegisterMemory_callback(MemoryAddress, MemorySizeInBytes);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDeregisterMemory( void* MemoryAddress ) {roctracer::kfd_support::hsaKmtDeregisterMemory_callback(MemoryAddress);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtMapMemoryToGPU( void* MemoryAddress, HSAuint64 MemorySizeInBytes, HSAuint64* AlternateVAGPU ) {roctracer::kfd_support::hsaKmtMapMemoryToGPU_callback(MemoryAddress, MemorySizeInBytes, AlternateVAGPU);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtUnmapMemoryToGPU( void* MemoryAddress ) {roctracer::kfd_support::hsaKmtUnmapMemoryToGPU_callback(MemoryAddress);} 
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDbgRegister( HSAuint32 NodeId ) {roctracer::kfd_support::hsaKmtDbgRegister_callback(NodeId);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDbgUnregister( HSAuint32 NodeId ) {roctracer::kfd_support::hsaKmtDbgUnregister_callback(NodeId);}
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDbgWavefrontControl( HSAuint32 NodeId, HSA_DBG_WAVEOP Operand, HSA_DBG_WAVEMODE Mode, HSAuint32 TrapId, HsaDbgWaveMessage* DbgWaveMsgRing ) {roctracer::kfd_support::hsaKmtDbgWavefrontControl_callback(NodeId, Operand, Mode, TrapId, DbgWaveMsgRing);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtDbgAddressWatch( HSAuint32 NodeId, HSAuint32 NumWatchPoints, HSA_DBG_WATCH_MODE WatchMode[], void* WatchAddress[], HSAuint64 WatchMask[], HsaEvent* WatchEvent[] ) {roctracer::kfd_support::hsaKmtDbgAddressWatch_callback(NodeId, NumWatchPoints, WatchMode, WatchAddress, WatchMask, WatchEvent);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtGetClockCounters( HSAuint32 NodeId, HsaClockCounters* Counters ) {roctracer::kfd_support::hsaKmtGetClockCounters_callback(NodeId, Counters);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcGetCounterProperties( HSAuint32 NodeId, HsaCounterProperties** CounterProperties ) {roctracer::kfd_support::hsaKmtPmcGetCounterProperties_callback(NodeId, CounterProperties);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcRegisterTrace( HSAuint32 NodeId, HSAuint32 NumberOfCounters, HsaCounter* Counters, HsaPmcTraceRoot* TraceRoot ) {roctracer::kfd_support::hsaKmtPmcRegisterTrace_callback(NodeId, NumberOfCounters, Counters, TraceRoot);} 
PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcUnregisterTrace( HSAuint32 NodeId, HSATraceId TraceId ) {roctracer::kfd_support::hsaKmtPmcUnregisterTrace_callback(NodeId, TraceId);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcAcquireTraceAccess( HSAuint32 NodeId, HSATraceId TraceId ) {roctracer::kfd_support::hsaKmtPmcAcquireTraceAccess_callback(NodeId, TraceId);}


PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcReleaseTraceAccess( HSAuint32 NodeId, HSATraceId TraceId ) {roctracer::kfd_support::hsaKmtPmcReleaseTraceAccess_callback(NodeId, TraceId);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcStartTrace( HSATraceId TraceId, void* TraceBuffer, HSAuint64 TraceBufferSizeBytes ) {roctracer::kfd_support::hsaKmtPmcStartTrace_callback(TraceId, TraceBuffer, TraceBufferSizeBytes);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcQueryTrace( HSATraceId TraceId ) {roctracer::kfd_support::hsaKmtPmcQueryTrace_callback(TraceId);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtPmcStopTrace( HSATraceId TraceId ) {roctracer::kfd_support::hsaKmtPmcStopTrace_callback(TraceId);}

PUBLIC_API HSAKMT_STATUS HSAKMTAPI  hsaKmtSetTrapHandler( HSAuint32 NodeId, void* TrapHandlerBaseAddress, HSAuint64 TrapHandlerSizeInBytes, void* TrapBufferBaseAddress, HSAuint64 TrapBufferSizeInBytes ) {roctracer::kfd_support::hsaKmtSetTrapHandler_callback(NodeId, TrapHandlerBaseAddress, TrapHandlerSizeInBytes, TrapBufferBaseAddress, TrapBufferSizeInBytes);}

