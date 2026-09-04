#include "persist.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ponytail: fixed CONFIG_MAX_KEYS map (persist.h), grow the constant if more
// save fields appear. New saves use 28 of 32: highScore(1) + settings(4) +
// progress(11 fixed task/economy fields) + one pu%d per upgrade track(12).
// A legacy save can retain resIndex/skinMask/selectedSkin too, peaking at 31.
// Overflow is silent, so progress.c checks that peak against capacity.
typedef struct {
  char key[24];
  int val;
} ConfigEntry;

static ConfigEntry gMap[CONFIG_MAX_KEYS];
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

// Parse a flat {"key": int, ...} object. Build into a temporary map and only
// commit a complete object, so a truncated or malformed save cannot leak a
// valid-looking prefix into the game (this is a user-editable trust boundary).
void configLoad(const char *path) {
  gCount = 0;
  FILE *f = fopen(path, "r");
  if (!f) {
    return;
  }
  char text[2048];
  size_t n = fread(text, 1, sizeof(text) - 1, f);
  bool tooLong = n == sizeof(text) - 1 && fgetc(f) != EOF;
  bool readFailed = ferror(f);
  fclose(f);
  if (readFailed || tooLong) {
    return;
  }
  text[n] = '\0';

  ConfigEntry parsed[CONFIG_MAX_KEYS];
  int count = 0;
  const char *p = skipWs(text);
  if (*p != '{') {
    return;
  }
  p = skipWs(p + 1);
  while (*p != '}' && *p != '\0' && count < CONFIG_MAX_KEYS) {
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
    errno = 0;
    long v = strtol(p, &end, 10);
    if (end == p || errno == ERANGE || v < INT_MIN || v > INT_MAX) {
      return; // missing integer, or one the store cannot represent
    }
    p = skipWs(end);
    strcpy(parsed[count].key, key);
    parsed[count].val = (int)v;
    count++;
    if (*p == ',') {
      p = skipWs(p + 1);
    } else if (*p != '}') {
      return; // entries must be comma-separated
    }
  }
  if (*p != '}' || *skipWs(p + 1) != '\0') {
    return; // missing close, excess entries, or trailing garbage
  }
  memcpy(gMap, parsed, (size_t)count * sizeof(gMap[0]));
  gCount = count;
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
    gMap[gCount].key[sizeof(gMap[gCount].key) - 1] = '\0';
    gMap[gCount].val = val;
    gCount++;
  }
}

// Write beside the live file, then atomically replace it only after fclose
// confirms the complete JSON was written successfully.
void configSave(const char *path) {
  char temp[PATH_MAX];
  int n = snprintf(temp, sizeof(temp), "%s.tmp", path);
  if (n < 0 || n >= (int)sizeof(temp)) {
    return;
  }
  FILE *f = fopen(temp, "w");
  if (!f) {
    return;
  }
  fprintf(f, "{\n");
  for (int i = 0; i < gCount; i++) {
    fprintf(f, "  \"%s\": %d%s\n", gMap[i].key, gMap[i].val,
            i + 1 < gCount ? "," : "");
  }
  fprintf(f, "}\n");
  bool failed = ferror(f);
  failed |= fclose(f) != 0;
  if (failed || rename(temp, path) != 0) {
    unlink(temp); // keep the previous save intact on any write/replace failure
  }
}

static bool makeDir(const char *path) {
  return mkdir(path, 0700) == 0 || errno == EEXIST;
}

static const char *userSavePath(void) {
  static char path[PATH_MAX];
  char library[PATH_MAX], support[PATH_MAX], dir[PATH_MAX];
  const char *home = getenv("HOME");
  if (!home || !home[0] ||
      snprintf(library, sizeof(library), "%s/Library", home) >=
          (int)sizeof(library) ||
      snprintf(support, sizeof(support), "%s/Application Support", library) >=
          (int)sizeof(support) ||
      snprintf(dir, sizeof(dir), "%s/Boing", support) >= (int)sizeof(dir) ||
      !makeDir(library) || !makeDir(support) || !makeDir(dir) ||
      snprintf(path, sizeof(path), "%s/save.json", dir) >= (int)sizeof(path)) {
    return "save.json";
  }
  return path;
}

void configLoadUser(void) {
  const char *path = userSavePath();
  if (strcmp(path, "save.json") != 0 && access(path, F_OK) != 0 &&
      access("save.json", F_OK) == 0) {
    configLoad("save.json"); // migrate the old working-directory save on write
  } else {
    configLoad(path);
  }
}

void configSaveUser(void) { configSave(userSavePath()); }

#ifdef PERSIST_SELFTEST
#include <assert.h>
// Atomic-write, round-trip, parser-boundary, and user-path check:
//   cc -DPERSIST_SELFTEST src/persist.c -o /tmp/pt && /tmp/pt
int main(void) {
  const char *path = "/tmp/persist_selftest.json";
  const char *temp = "/tmp/persist_selftest.json.tmp";
  unlink(path);
  rmdir(temp);
  gCount = 0;
  configSetInt("highScore", 42);
  configSetInt("coinBank", -3);
  configSetInt("highScore", 100); // upsert overwrites
  configSave(path);

  configLoad(path); // round-trip
  assert(configGetInt("highScore", 0) == 100);
  assert(configGetInt("coinBank", 0) == -3);
  assert(configGetInt("missing", 7) == 7); // default fallback

  // A failed temporary-file write must not truncate the live save.
  assert(mkdir(temp, 0700) == 0);
  configSetInt("highScore", 999);
  configSave(path);
  configLoad(path);
  assert(configGetInt("highScore", 0) == 100);
  assert(rmdir(temp) == 0);

  // Long internal keys truncate safely instead of relying on zeroed storage for
  // strncpy's missing terminator.
  gCount = 0;
  memset(gMap[0].key, 'x', sizeof(gMap[0].key));
  configSetInt("123456789012345678901234567890", 1);
  assert(gMap[0].key[sizeof(gMap[0].key) - 1] == '\0');

  FILE *g = fopen(path, "w");
  assert(g != NULL);
  fputs("{\"highScore\": 12, garbage", g);
  fclose(g);
  configLoad(path); // malformed suffix must discard the valid-looking prefix
  assert(configGetInt("highScore", 55) == 55);

  g = fopen(path, "w");
  assert(g != NULL);
  fputs("{\"highScore\": 12}", g);
  for (int i = 0; i < 2048; i++) {
    fputc(' ', g);
  }
  fclose(g);
  configLoad(path); // oversized input is truncation, even after a valid prefix
  assert(configGetInt("highScore", 55) == 55);

  g = fopen(path, "w");
  assert(g != NULL);
  fputs("{\"highScore\": 999999999999999999999}", g);
  fclose(g);
  configLoad(path); // out-of-range int is malformed, not a narrowing cast
  assert(configGetInt("highScore", 55) == 55);

  configLoad("/no/such/file.json"); // missing file
  assert(configGetInt("anything", 9) == 9);

  char home[] = "/tmp/boing_persist_XXXXXX";
  assert(mkdtemp(home) != NULL);
  assert(setenv("HOME", home, 1) == 0);
  gCount = 0;
  configSetInt("highScore", 321);
  configSaveUser();
  gCount = 0;
  configLoadUser();
  assert(configGetInt("highScore", 0) == 321);
  const char *user = userSavePath();
  assert(strncmp(user, home, strlen(home)) == 0);
  assert(unlink(user) == 0);
  char dir[PATH_MAX];
  snprintf(dir, sizeof(dir), "%s/Library/Application Support/Boing", home);
  assert(rmdir(dir) == 0);
  snprintf(dir, sizeof(dir), "%s/Library/Application Support", home);
  assert(rmdir(dir) == 0);
  snprintf(dir, sizeof(dir), "%s/Library", home);
  assert(rmdir(dir) == 0);
  assert(rmdir(home) == 0);
  assert(unlink(path) == 0);
  return 0;
}
#endif
