#include <stdio.h>
#include "fibril.h"

int fib(int n)
{
  if (n < 2) return n;

  int x, y;

  fibril_t fr;
  fibril_init(&fr);

  fibril_fork(&fr, fib, x, n - 1);
  y = fib(n - 2);

  fibril_join(&fr);
  return x + y;
}

int main(int argc, const char *argv[])
{
  fibril_rt_init(FIBRIL_MAX_PROCS);

  int n = 7;
  int m = fib(n);

  fibril_rt_exit();

  printf("fib(%d)=%d\n", n, m);
  return 0;
}
