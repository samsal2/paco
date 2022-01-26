#ifndef OWL_INTERNAL_H_
#define OWL_INTERNAL_H_

#define OWL_GLOBAL static
#define OWL_INTERNAL static
#define OWL_LOCAL_PERSIST static

#include <assert.h>
#define OWL_ASSERT(e) assert(e)

#include <string.h>
#define OWL_MEMSET(dst, c, s) memset(dst, c, s)
#define OWL_MEMCPY(dst, src, s) memcpy(dst, src, s)

#ifndef NDEBUG

void *owl_dbg_malloc_(size_t s, char const *f, int l);
#define OWL_MALLOC(s) owl_dbg_malloc_(s, __FILE__, __LINE__)

void *owl_dbg_calloc_(size_t c, size_t s, char const *f, int l);
#define OWL_CALLOC(c, s) owl_dbg_calloc_(c, s, __FILE__, __LINE__)

void *owl_dbg_realloc_(void *p, size_t s, char const *f, int l);
#define OWL_REALLOC(p, s) owl_dbg_realloc_(p, s, __FILE__, __LINE__)

void owl_dbg_free_(void *p, char const *f, int l);
#define OWL_FREE(p) owl_dbg_free_(p, __FILE__, __LINE__)

void owl_dbg_log_(char const *f, int l, char const *fmt, ...);
#define OWL_DBG_LOG(...) owl_dbg_log_(__FILE__, __LINE__, __VA_ARGS__)

#else

#include <stdlib.h>
#define OWL_MALLOC(s) malloc(s)
#define OWL_CALLOC(c, s) calloc(c, s)
#define OWL_REALLOC(p, s) realloc(p, s)
#define OWL_FREE(p) free(p)

#define OWL_DBG_LOG(...)

#endif

#define OWL_CLAMP(v, l, h) ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define OWL_MAX(a, b) ((a) < (b) ? (b) : (a))
#define OWL_MIN(a, b) ((a) > (b) ? (b) : (a))
#define OWL_UNUSED(e) ((void)e)
#define OWL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define OWL_ALIGN2(v, a) (v + a - 1) & ~(a - 1)
#define OWL_SIZEOF(v) ((long)sizeof(v))

#ifndef NDEBUG

#include <stdio.h>

#define OWL_VK_CHECK(e)                                                        \
  do {                                                                         \
    VkResult const result_ = e;                                                \
    if (VK_SUCCESS != result_)                                                 \
      OWL_DBG_LOG("OWL_VK_CHECK: error with code %i\n", result_);              \
    OWL_ASSERT(VK_SUCCESS == result_);                                         \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif

#endif
