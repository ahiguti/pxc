
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void range_reverse(char *start, char *finish)
{
  while (start < finish) {
    char ch = *start;
    --finish;
    *start = *finish;
    *finish = ch;
    ++start;
  }
}

void to_decimal(int v, char *buf)
{
  if (v == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }
  char *p;
  if (v < 0) {
    buf[0] = '-';
    ++buf;
    p = buf;
    while (v != 0) {
      #if 1
      int rem = v % 10; /* expects rem < 0 if v < 0 */
      int quot = v / 10;
      #endif
      p[0] = '0' - rem;
      v = quot;
      ++p;
    }
  } else {
    p = buf;
    while (v != 0) {
      #if 0
      div_t d = div(v, 10);
      int rem = d.rem;
      int quot = d.quot;
      #endif
      #if 1
      int rem = v % 10;
      int quot = v / 10;
      #endif
      p[0] = '0' + rem;
      v = quot;
      ++p;
    }
  }
  range_reverse(buf, p);
  p[0] = '\0';
}

int test1()
{
  char buf[1024];
  for (int i = -10000000; i < 10000000; ++i) {
    to_decimal(i, buf);
  }
}

int test2()
{
  char buf[1024];
  for (int i = -10000000; i < 10000000; ++i) {
    sprintf(buf, "%d", i);
  }
}

int test3()
{
  char buf1[1024], buf2[1024];
  for (int i = -10000000; i < 10000000; ++i) {
    sprintf(buf1, "%d", i);
    to_decimal(i, buf2);
    if (strcmp(buf1, buf2) != 0) {
      printf("[%s] [%s]\n", buf1, buf2);
      abort();
    }
  }
}

int main(int argc, char **argv)
{
  int p = argc > 1 ? atoi(argv[1]) : 1;
  printf("test-%d\n", p);
  if (p == 1) {
    test1();
  } else if (p == 2) {
    test2();
  } else if (p == 3) {
    test3();
  } else {
  }
}

