
#ifndef APPLICATE_INCLUDE_UBASE_DEFINE_H
#define APPLICATE_INCLUDE_UBASE_DEFINE_H

#include <stdint.h>
#include <stddef.h>

#ifndef osNullptr
#define osNullptr    0
#endif

#ifdef _MSC_VER_
#ifdef API_DLL_BUILD
#define EXPORT_API _declspec(dllexport)
#elif  define API_DLL_USE
#define EXPORT_API _declspec(dllimport)
#else
#define EXPORT_API
#endif
#else
#define EXPORT_API
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#ifdef __GNUC__
#define UBASE_LIKELY(x)          __builtin_expect(!!(x), 1)
#define UBASE_UNLIKELY(x)        __builtin_expect(!!(x), 0)
#else
#define UBASE_LIKELY(x)          (!!(x))
#define UBASE_UNLIKELY(x)        (!!(x))
#endif

#define UFACE_JOIN(X,Y)         UFACE_DO_JOIN(X,Y)
#define UFACE_DO_JOIN(X,Y)      UFACE_DO_JOIN2(X,Y)
#define UFACE_DO_JOIN2(X,Y)     X##Y

#endif //APPLICATE_INCLUDE_UBASE_DEFINE_H
