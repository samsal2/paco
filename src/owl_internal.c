#include "owl_internal.h"

#include "owl_definitions.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void *owl_debug_malloc(size_t s, char const *f, int l) {
  void *p = malloc(s);
  printf("\033[33m[OWL_MALLOC]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p "
         "s:%lu\n",
         f, l, p, s);
  return p;
}

void *owl_debug_calloc(size_t c, size_t s, char const *f, int l) {
  void *p = calloc(c, s);
  printf("\033[33m[OWL_CALLOC]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p "
         "c:%lu s:%lu\n",
         f, l, p, c, s);
  return p;
}

void *owl_debug_realloc(void *p, size_t s, char const *f, int l) {
  void *np = realloc(p, s);
  printf("\033[33m[OWL_REALLOC]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p "
         "np:%p s:%lu\n",
         f, l, p, np, s);
  return np;
}

void owl_debug_free(void *p, char const *f, int l) {
  printf("\033[33m[OWL_FREE]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p\n", f, l,
         p);
  free(p);
}

void owl_debug_log(char const *f, int l, char const *format, ...) {
  va_list args;
  va_start(args, format);
#if 0
  printf("\033[33m[OWL_DEBUG_LOG]\033[0m \033[31m(f:%s l:%d)\033[0m ", f, l);
#else
  OWL_UNUSED(f);
  OWL_UNUSED(l);
  printf("\033[33m[OWL_DEBUG_LOG]\033[0m ");
#endif
  vprintf(format, args);
  va_end(args);
}
