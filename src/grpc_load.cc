//---------------------------------------------------------------------
//---------------------------------------------------------------------
#ifndef _WIN32
#include <dlfcn.h>
#else
#include <wtypes.h>
#endif

#include <whereami.h>

#include "./lv_interop.h"

//---------------------------------------------------------------------
//---------------------------------------------------------------------
namespace grpc_labview
{
#ifndef _WIN32
    typedef void *LibHandle;
#else
    typedef HMODULE LibHandle;
#endif

    LibHandle LockgRPCLibraryIntoProcessMem()
    {

        int length;
        std::string modulePathString;

        length = wai_getModulePath(nullptr, 0, nullptr);
        if (length > 0)
        {
            modulePathString.resize(length);
            wai_getModulePath(&modulePathString[0], length, nullptr);

#if _WIN32
            auto dllHandle = LoadLibrary(modulePathString.c_str());
#else
            auto dllHandle = dlopen(modulePathString.c_str(), RTLD_LAZY);
#endif
            return dllHandle;
        }
        return nullptr;
    }

    LibHandle gSelfLibHandle = LockgRPCLibraryIntoProcessMem();
}