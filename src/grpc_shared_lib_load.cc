#include <string>
#include <whereami.h>

#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((constructor))
#endif

// use static to restrict access to this function to this translation unit
static void on_shared_library_load(void)
{
  int length, dirname_length;
  std::string path;

  length = wai_getModulePath(nullptr, 0, &dirname_length);
  if (length > 0)
  {
    path.reserve(length);
    wai_getModulePath(&path[0], length, &dirname_length);

  }
}

#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((destructor))
#endif
static void on_shared_library_unload(void)
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
      load();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      unload();
      break;
  }
  return TRUE;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif