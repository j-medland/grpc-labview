#ifndef _WIN32
#include <dlfcn.h>
#else
#include <wtypes.h>
#endif

#include <lv_interop.h>

namespace grpc_labview
{
#ifndef _WIN32
    typedef void* LibHandle;
#else
    typedef HMODULE LibHandle;
#endif

    LibHandle LockgRPCLibraryIntoProcessMem()
    {
#if _WIN32
        auto dllHandle = LoadLibrary(GetSharedLibraryName().c_str());
#else
        auto dllHandle = dlopen(GetSharedLibraryName().c_str(), RTLD_LAZY);
#endif
        return dllHandle;
    }

    LibHandle gSelfLibHandle = LockgRPCLibraryIntoProcessMem();
}