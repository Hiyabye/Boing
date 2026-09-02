#ifndef PERSIST_H
#define PERSIST_H

// Tiny JSON int key-value store for a single runtime save file beside the exe
// (high score, settings, progress all share it). The file is a flat JSON
// object of string keys to integer values, e.g. {"highScore": 42, ...} —
// non-int values (a volume fraction) are stored scaled to an int by the caller.
//
// configLoad parses the file into a shared in-memory map once at startup; a
// missing or malformed file leaves the map empty, so configGetInt returns each
// caller's default. Every subsystem reads/writes its own keys through this one
// map, and configSave serializes the whole map — so independent saves merge
// instead of clobbering each other's keys.
//
// The map is a fixed-size array, and running out of slots is **silent**:
// configSetInt drops the key and that field simply never persists. So the
// capacity is public, and any module contributing a variable number of keys is
// expected to STATIC_ASSERT its share against it — progress.c does, because its
// one-key-per-upgrade-track scheme is the only thing here that grows when an
// enum grows. Raise this when key capacity gets tight; it costs 28 bytes a
// slot.
#define CONFIG_MAX_KEYS 24

void configLoad(const char *path);
int configGetInt(const char *key, int def);
void configSetInt(const char *key, int val);
void configSave(const char *path);

#endif // PERSIST_H
