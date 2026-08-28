#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(PKIMAGEIO_BUILDING_LIBRARY)
#    define PKIMAGEIO_EXPORT __declspec(dllexport)
#  else
#    define PKIMAGEIO_EXPORT __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define PKIMAGEIO_EXPORT __attribute__((visibility("default")))
#else
#  define PKIMAGEIO_EXPORT
#endif
