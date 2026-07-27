#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ponytail: fixed 24-key map, grow the constant if more save fields appear
// (currently ~20: highScore + 3 settings + 4 progress + 12 powerup tracks)
static struct {
  char key[24];
  int val;
} gMap[24];
static int gCount;

static int mapFind(const char *key) {
  for (int i = 0; i < gCount; i++) {
    if (strcmp(gMap[i].key, key) == 0) {
      return i;
    }
  }
  return -1;
}

// Skip spaces/tabs/newlines; returns the next non-space char pointer.
static const char *skipWs(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
    p++;
  }
  return p;
}

// Parse a flat {"key": int, ...} object. Defensive: stop on any malformed
// token and keep whatever parsed (this is a user-editable trust boundary).
void configLoad(const char *path) {
  gCount = 0;
  FILE *f = fopen(path, "r");
  if (!f) {
    return;
  }
  char text[2048];
  size_t n = fread(text, 1, sizeof(text) - 1, f);
  fclose(f);
  text[n] = '\0';

  const char *p = skipWs(text);
  if (*p != '{') {
    return;
  }
  p = skipWs(p + 1);
  while (*p != '}' && *p != '\0' &&
         gCount < (int)(sizeof(gMap) / sizeof(gMap[0]))) {
    if (*p != '"') {
      return; // expected a key string
    }
    p++;
    char key[24];
    size_t k = 0;
    while (*p != '"' && *p != '\0' && k < sizeof(key) - 1) {
      key[k++] = *p++;
    }
    if (*p != '"') {
      return;
    }
    key[k] = '\0';
    p = skipWs(p + 1);
    if (*p != ':') {
      return;
    }
    p = skipWs(p + 1);
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p) {
      return; // no integer
    }
    p = end;
    strcpy(gMap[gCount].key, key);
    gMap[gCount].val = (int)v;
    gCount++;
    p = skipWs(p);
    if (*p == ',') {
      p = skipWs(p + 1);
    }
  }
}

int configGetInt(const char *key, int def) {
  int i = mapFind(key);
  return i < 0 ? def : gMap[i].val;
}

void configSetInt(const char *key, int val) {
  int i = mapFind(key);
  if (i >= 0) {
    gMap[i].val = val;
    return;
  }
  if (gCount < (int)(sizeof(gMap) / sizeof(gMap[0]))) {
    strncpy(gMap[gCount].key, key, sizeof(gMap[gCount].key) - 1);
    gMap[gCount].val = val;
    gCount++;
  }
}

void configSave(const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) {
    return;
  }
  fprintf(f, "{\n");
  for (int i = 0; i < gCount; i++) {
    fprintf(f, "  \"%s\": %d%s\n", gMap[i].key, gMap[i].val,
            i + 1 < gCount ? "," : "");
  }
  fprintf(f, "}\n");
  fclose(f);
}

#ifdef PERSIST_SELFTEST
#include <assert.h>
// Build: cc -DPERSIST_SELFTEST src/persist.c -o /tmp/pt && /tmp/pt
int main(void) {
  const char *path = "/tmp/persist_selftest.json";
  gCount = 0;
  configSetInt("highScore", 42);
  configSetInt("coinBank", -3);
  configSetInt("highScore", 100); // upsert overwrites
  configSave(path);

  configLoad(path); // round-trip
  assert(configGetInt("highScore", 0) == 100);
  assert(configGetInt("coinBank", 0) == -3);
  assert(configGetInt("missing", 7) == 7); // default fallback

  FILE *g = fopen(path, "w");
  fputs("{ garbage not json", g);
  fclose(g);
  configLoad(path); // must not crash; map ends empty/partial
  assert(configGetInt("highScore", 55) == 55);

  configLoad("/no/such/file.json"); // missing file
  assert(configGetInt("anything", 9) == 9);
  return 0;
}
#endif
