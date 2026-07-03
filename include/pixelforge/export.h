// pixelforge/export.h -- DLL/SO symbol export macros.
//
// Industry pattern: every public class/function in a shared library
// must be tagged with PIXELFORGE_API so:
//   * Windows: dllexport when building, dllimport when consuming
//   * Linux/macOS: default visibility (we hide everything else globally)
//
// The build system defines pixelforge_EXPORTS automatically while
// compiling the SHARED target (CMake convention).
#ifndef PIXELFORGE_EXPORT_H
#define PIXELFORGE_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(PIXELFORGE_STATIC)
#    define PIXELFORGE_API
#  elif defined(pixelforge_EXPORTS)
#    define PIXELFORGE_API __declspec(dllexport)
#  else
#    define PIXELFORGE_API __declspec(dllimport)
#  endif
#else
#  if __GNUC__ >= 4
#    define PIXELFORGE_API __attribute__((visibility("default")))
#  else
#    define PIXELFORGE_API
#  endif
#endif

#endif  // PIXELFORGE_EXPORT_H
