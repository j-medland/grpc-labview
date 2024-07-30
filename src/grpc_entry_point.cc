//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include <string>
#include <whereami.h>

//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include "./lv_interop.h"
#include "./feature_toggles.h"

#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((constructor))
#endif

// use static to restrict access to this function to this translation unit
static void onSharedLibraryLoad()
{
  int length;
  std::string modulePathString, moduleDirectoryString;

  int dirnameLength;
  length = wai_getModulePath(nullptr, 0, nullptr);
  if (length > 0)
  {
    modulePathString.resize(length);
    wai_getModulePath(&modulePathString[0], length, &dirnameLength);

    moduleDirectoryString = modulePathString.substr(0,dirnameLength);

    grpc_labview::FeatureConfig::getInstance().readConfigFromFile(moduleDirectoryString + "/features_config.ini");
  }
}

#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((destructor))
#endif
static void onSharedLibraryUnload()
{
  // nothing to do
}

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER)
#pragma warning(push, 3)
#endif
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
    case DLL_PROCESS_ATTACH:
      onSharedLibraryLoad();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      onSharedLibraryUnload();
      break;
  }
  return TRUE;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif