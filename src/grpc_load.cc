#ifndef _WIN32
#include <dlfcn.h>
#else
#include <wtypes.h>
#endif

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
        auto dllHandle = LoadLibrary(SharedLibraryName);
#else
        auto dllHandle = dlopen(SharedLibraryName, RTLD_LAZY);
#endif
        return dllHandle;
    }

    LibHandle gSelfLibHandle = LockgRPCLibraryIntoProcessMem();
}