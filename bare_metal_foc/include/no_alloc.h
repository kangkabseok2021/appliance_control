#ifndef NO_ALLOC_H
#define NO_ALLOC_H

/* Bare-metal constraint: no dynamic memory allocation.
   Any accidental call to malloc/calloc/realloc/free becomes a compile error. */
#pragma GCC poison malloc calloc realloc free

#endif /* NO_ALLOC_H */
