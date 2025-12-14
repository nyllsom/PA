#include <stdio.h>
#include <sys/time.h>

int main() {
  struct timeval tv_last, tv_now;

  gettimeofday(&tv_last, NULL);

  while (1) {
    gettimeofday(&tv_now, NULL);

    long delta_us =
      (tv_now.tv_sec - tv_last.tv_sec) * 1000000 +
      (tv_now.tv_usec - tv_last.tv_usec);

    if (delta_us >= 500000) {
      printf("0.5 sec passed\n");
      tv_last = tv_now;
    }
  }

  return 0;
}
