#ifndef PERSIST_H
#define PERSIST_H

// Tiny JSON int key-value store for one runtime save file in
// ~/Library/Application Support/Boing (high score, settings, progress all share
// it). The file is a flat JSON
// object of string keys to integer values, e.g. {"highScore": 42, ...} —
// non-int values (a volume fraction) are stored scaled to an int by the caller.
//
// configLoad parses the file into a shared in-memory map once at startup; a
// missing or malformed file leaves the map empty, so configGetInt returns each
// caller's default. Every subsystem reads/writes its own keys through this one
// map. configSave writes a temporary file and atomically replaces the live
// file only after a complete write, so independent saves merge without risking
// a truncated profile.
//
// The map is a fixed-size array, and running out of slots is **silent**:
// configSetInt drops the key and that field simply never persists. So the
// capacity is public, and any module contributing a variable number of keys is
// expected to STATIC_ASSERT its share against it. progress.c guards its task
// slots and one-key-per-upgrade-track scheme. Raise this when key capacity gets
// tight; it costs 28 bytes a slot.
#define CONFIG_MAX_KEYS 32

// Production entry points use the per-user Application Support path. On first
// launch there, configLoadUser falls back to a legacy save.json in the working
// directory so existing development saves migrate on the next write.
void configLoadUser(void);
void configSaveUser(void);

// Explicit paths are retained for the parser's isolated self-test.
void configLoad(const char *path);
int configGetInt(const char *key, int def);
void configSetInt(const char *key, int val);
void configSave(const char *path);

#endif // PERSIST_H
