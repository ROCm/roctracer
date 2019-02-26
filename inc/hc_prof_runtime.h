#pragma once

namespace hc {
// HCC Op IDs enumeration
enum HSAOpId {
    HSA_OP_ID_DISPATCH = 0,
    HSA_OP_ID_COPY = 1,
    HSA_OP_ID_BARRIER = 2,
    HSA_OP_ID_NUMBER = 3
};
}; // namespace hc

namespace Kalmar {
namespace CLAMP {
// Activity profiling primitives
inline void InitActivityCallback(void* id_callback, void* op_callback, void* arg) {}
inline bool EnableActivityCallback(uint32_t op, bool enable) { return false; }
inline const char* GetCmdName(uint32_t id) { return NULL; }
} // namespace CLAMP
} // namespace Kalmar

/** \endcond */
