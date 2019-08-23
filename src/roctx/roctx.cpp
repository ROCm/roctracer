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

#include "roctx.h"

//void fun() {}
extern "C" {
PUBLIC_API bool RegisterApiCallback(uint32_t op, void* callback, void* user_data) {
    roctracer::roctx::cb_table.set(op, reinterpret_cast<activity_rtapi_callback_t>(callback), user_data);
    return true;
}
PUBLIC_API bool RemoveApiCallback(uint32_t op) {
    roctracer::roctx::cb_table.set(op, NULL, NULL);
    return true;
}
    // block: HSAROCTXAPI API
PUBLIC_API void hsaOpenROCTX() { roctracer::roctx::hsaOpenROCTX_callback();}
PUBLIC_API void hsaCloseROCTX() { roctracer::roctx::hsaCloseROCTX_callback();}
PUBLIC_API void roctxMarkA (roctxStringHandle_t message) { roctracer::roctx::roctxMarkA_callback(message);}
PUBLIC_API roctxRangeId_t roctxRangePushA(roctxStringHandle_t message) { roctracer::roctx::roctxRangePushA_callback(message); return strlen(message);} // try md5sum  
PUBLIC_API roctxRangeId_t roctxRangePop() { roctracer::roctx::roctxRangePop_callback(); return 0;}
PUBLIC_API roctxRangeId_t roctxRangeStart(roctxStringHandle_t message)  { roctracer::roctx::roctxRangeStart_callback(message); return strlen(message);}
PUBLIC_API roctxRangeId_t roctxRangeEnd(roctxRangeId_t rocTxRangeID) { roctracer::roctx::roctxRangeEnd_callback(rocTxRangeID); return rocTxRangeID;}
}
