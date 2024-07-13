//---------------------------------------------------------------------
//---------------------------------------------------------------------
#ifndef _WIN32
#include <dlfcn.h>
#endif

//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include <iostream>
#include <cstring>
#include <memory>
#include <grpcpp/grpcpp.h>

#include "./lv_interop.h"
#include "./feature_toggles.h"

//---------------------------------------------------------------------
//---------------------------------------------------------------------
// Value passed to the RTSetCleanupProc to remove the clean up proc for the VI.
static int kCleanOnRemove = 0;
// Value passed to the RTSetCleanUpProc to register a cleanup proc for a VI which
// gets called when the VI state changes to Idle.
static int kCleanOnIdle = 2;

//---------------------------------------------------------------------
//---------------------------------------------------------------------
typedef int (*NumericArrayResize_T)(int32_t, int32_t, void *handle, size_t size);
typedef int (*PostLVUserEvent_T)(grpc_labview::LVUserEventRef ref, void *data);
typedef int (*Occur_T)(grpc_labview::MagicCookie occurrence);
typedef int32_t (*RTSetCleanupProc_T)(grpc_labview::CleanupProcPtr cleanUpProc, grpc_labview::gRPCid *id, int32_t mode);
typedef unsigned char **(*DSNewHandlePtr_T)(size_t);
typedef int (*DSSetHandleSize_T)(void *h, size_t);
typedef long (*DSDisposeHandle_T)(void *h);

//---------------------------------------------------------------------
//---------------------------------------------------------------------
static NumericArrayResize_T NumericArrayResizeImp = nullptr;
static PostLVUserEvent_T PostLVUserEvent = nullptr;
static Occur_T Occur = nullptr;
static RTSetCleanupProc_T RTSetCleanupProc = nullptr;
static DSNewHandlePtr_T DSNewHandleImpl = nullptr;
static DSSetHandleSize_T DSSetHandleSizeImpl = nullptr;
static DSDisposeHandle_T DSDisposeHandleImpl = nullptr;

namespace grpc_labview
{
    grpc_labview::PointerManager<grpc_labview::gRPCid> gPointerManager;

#ifdef _WIN32

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int32_t InitCallbacks(grpc_labview::MagicCookie occurrence)
    {
        if (PostLVUserEvent)
        {
            return grpc::StatusCode::OK;
        }

        int32_t result{-1000 - grpc::StatusCode::INTERNAL}; // non-zero = failed

        // Search for a Module which Exports the LabVIEW Functions we are after
        HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
        MODULEENTRY32 me32;

        //  Take a snapshot of all modules in the specified process.
        hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (hModuleSnap == INVALID_HANDLE_VALUE)
        {
            // snapshot failed
            return result;
        }

        //  Set the size of the structure before using it.
        me32.dwSize = sizeof(MODULEENTRY32);

        //  Retrieve information about the first module
        if (!Module32First(hModuleSnap, &me32))
        {
            // first retrieval failed
            return result;
        }

        //  Loop through, looking for a Module that exports the functions we are after
        do
        {
            Occur = (Occur_T)GetProcAddress(me32.hModule, "Occur");
            // validate that this module is in the correct context by trying to fire the occurance
            // Occur returns non-zero value on failure
            if (Occur == nullptr || Occur(occurrence) != 0)
            {
                // skip this iteration and keep looping
                continue;
            }
            PostLVUserEvent = (PostLVUserEvent_T)GetProcAddress(me32.hModule, "PostLVUserEvent");
            NumericArrayResizeImp = (NumericArrayResize_T)GetProcAddress(me32.hModule, "NumericArrayResize");
            RTSetCleanupProc = (RTSetCleanupProc_T)GetProcAddress(me32.hModule, "RTSetCleanupProc");
            DSNewHandleImpl = (DSNewHandlePtr_T)GetProcAddress(me32.hModule, "DSNewHandle");
            DSSetHandleSizeImpl = (DSSetHandleSize_T)GetProcAddress(me32.hModule, "DSSetHandleSize");
            DSDisposeHandleImpl = (DSDisposeHandle_T)GetProcAddress(me32.hModule, "DSDisposeHandle");

            result = grpc::StatusCode::OK; // success!
            break;
        } while (Module32Next(hModuleSnap, &me32));

        CloseHandle(hModuleSnap);

        return result;
    }

#else

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int32_t InitCallbacks(grpc_labview::MagicCookie occurrence)
    {
        if (NumericArrayResizeImp != nullptr)
        {
            return 0;
        }

        NumericArrayResizeImp = (NumericArrayResize_T)dlsym(RTLD_DEFAULT, "NumericArrayResize");
        PostLVUserEvent = (PostLVUserEvent_T)dlsym(RTLD_DEFAULT, "PostLVUserEvent");
        Occur = (Occur_T)dlsym(RTLD_DEFAULT, "Occur");
        RTSetCleanupProc = (RTSetCleanupProc_T)dlsym(RTLD_DEFAULT, "RTSetCleanupProc");
        DSNewHandleImpl = (DSNewHandlePtr_T)dlsym(RTLD_DEFAULT, "DSNewHandle");
        DSSetHandleSizeImpl = (DSSetHandleSize_T)dlsym(RTLD_DEFAULT, "DSSetHandleSize");
        DSDisposeHandleImpl = (DSDisposeHandle_T)dlsym(RTLD_DEFAULT, "DSDisposeHandle");

        if (NumericArrayResizeImp == nullptr ||
            PostLVUserEvent == nullptr ||
            Occur == nullptr ||
            RTSetCleanupProc == nullptr ||
            DSNewHandleImpl == nullptr ||
            DSSetHandleSizeImpl == nullptr ||
            DSDisposeHandleImpl == nullptr)
        {
            return -1000 - grpc::StatusCode::INTERNAL;
        }

        return grpc::StatusCode::OK;
    }

#endif

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int SignalOccurrence(MagicCookie occurrence)
    {
        return Occur(occurrence);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int NumericArrayResize(int32_t typeCode, int32_t numDims, void *handle, size_t size)
    {
        return NumericArrayResizeImp(typeCode, numDims, handle, size);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int PostUserEvent(LVUserEventRef ref, void *data)
    {
        return PostLVUserEvent(ref, data);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    unsigned char **DSNewHandle(size_t n)
    {
        return DSNewHandleImpl(n);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int DSSetHandleSize(void *h, size_t n)
    {
        return DSSetHandleSizeImpl(h, n);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    long DSDisposeHandle(void *h)
    {
        return DSDisposeHandleImpl(h);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void SetLVString(LStrHandle *lvString, const std::string &str)
    {
        auto length = str.length();
        auto error = NumericArrayResize(0x01, 1, lvString, length);
        std::memcpy((**lvString)->str, str.data(), length);
        (**lvString)->cnt = (int)length;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    std::string GetLVString(LStrHandle lvString)
    {
        if (lvString == nullptr || *lvString == nullptr)
        {
            return std::string();
        }

        auto count = (*lvString)->cnt;
        auto chars = (*lvString)->str;

        std::string result(chars, count);
        return result;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int32_t RegisterCleanupProc(CleanupProcPtr cleanUpProc, gRPCid *id)
    {
        return RTSetCleanupProc(cleanUpProc, id, kCleanOnIdle);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int32_t DeregisterCleanupProc(CleanupProcPtr cleanUpProc, gRPCid *id)
    {
        return RTSetCleanupProc(cleanUpProc, id, kCleanOnRemove);
    }

    int AlignClusterOffset(int clusterOffset, int alignmentRequirement)
    {
#ifndef _PS_4
        int remainder = abs(clusterOffset) % alignmentRequirement;
        if (remainder == 0)
        {
            return clusterOffset;
        }
        return clusterOffset + alignmentRequirement - remainder;
#else
        return clusterOffset;
#endif
    }
}
