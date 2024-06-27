#include <string>
#include <whereami.h>
#include <ghc/fs_std.hpp>

#include <feature_toggles.h>

#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((constructor))
#endif

// use static to restrict access to this function to this translation unit
static void onSharedLibraryLoad()
{
  int length, dirnameLength;
  std::string modulePathString;

  length = wai_getModulePath(nullptr, 0, &dirnameLength);
  if (length > 0)
  {
    modulePathString.reserve(length);
    wai_getModulePath(&modulePathString[0], length, &dirnameLength);
    fs::path modulePath{modulePathString};
    fs::path featuresConfigFilePath = modulePath.parent_path().append("feature_config.ini");

    grpc_labview::FeatureConfig::getInstance().readConfigFromFile(featuresConfigFilePath.string());
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