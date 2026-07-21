#include "persist.h"

#include <stdio.h>

bool readInts(const char *path, int *buf, int n) {
  FILE *f = fopen(path, "r");
  if (!f) {
    return false;
  }
  int tmp[n];
  int got = 0;
  for (; got < n; got++) {
    if (fscanf(f, "%d", &tmp[got]) != 1) {
      break;
    }
  }
  fclose(f);
  if (got != n) {
    return false; // partial/garbage file: leave buf (caller's defaults) alone
  }
  for (int i = 0; i < n; i++) {
    buf[i] = tmp[i];
  }
  return true;
}

void writeInts(const char *path, const int *buf, int n) {
  FILE *f = fopen(path, "w");
  if (!f) {
    return;
  }
  for (int i = 0; i < n; i++) {
    fprintf(f, i + 1 < n ? "%d " : "%d\n", buf[i]);
  }
  fclose(f);
}
