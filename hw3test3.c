// high priority child should print faster than low priority child

#include "types.h"
#include "stat.h"
#include "user.h"
#include "testhelpers.h"

extern int nice(int pid, int val);

int main(void) {
  int p1 = fork();

  if (p1 == 0) {
    nice(getpid(), 0);
    int cnt = 0;
    uint start = uptime();
    while ((int)(uptime() - start) < 200) { // ~2 seconds
      cnt++;
    }
    printf(1, "fast=%d\n", cnt);
    exit();
  }

  int p2 = fork();
  if (p2 == 0) {
    nice(getpid(), 4);
    int cnt = 0;
    uint start = uptime();
    while ((int)(uptime() - start) < 200) {
      cnt++;
    }
    printf(1, "slow=%d\n", cnt);
    exit();
  }

  wait(); 
  wait();

  printf(1, "hw3test3 (preempt): visually fast >> slow expected\n");
  
  exit();
}