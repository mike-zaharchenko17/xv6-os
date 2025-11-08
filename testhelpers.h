// helper method that wastes CPU time (busy wait)

#pragma once
#include "types.h"
#include "user.h"

static inline void burn(int ticks) {
  uint start = uptime();
  while ((int)(uptime() - start) < ticks) {
    // spin
  }
}

// static void yieldn(int n) {
//   for (int i = 0; i < n; i++) {
//     yield();
//   }
// }