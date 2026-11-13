#pragma once

#include <stdio.h>

#define BMO_CHECK(cond)                                                       \
  do {                                                                        \
    if (!(cond)) {                                                            \
      fprintf(stderr, "CHECK FAILED (%s:%d): %s\n", __FILE__, __LINE__, #cond); \
      return 1;                                                               \
    }                                                                         \
  } while (0)
