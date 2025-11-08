// The goal of this test is to get roughly alternating output between children

#include "types.h"
#include "stat.h"
#include "user.h"
#include "testhelpers.h"

int main(void) {
  int p1 = fork();

  if (p1 == 0) {
    for (int i = 0; i < 20; i++) { 
        printf(1, "A\n"); 
        burn(2); 
    }
    printf(1, "\nA done\n");
    exit();
  }

  int p2 = fork();
  if (p2 == 0) {
    for (int i = 0; i < 20; i++) { 
        printf(1, "B\n"); 
        burn(2); 
    }
    printf(1, "\nB done\n");
    exit();
  }

  wait(); 
  wait();

  printf(1, "hw3test1 (alternating print): OK\n");

  exit();
}