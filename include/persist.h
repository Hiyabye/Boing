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
void configLoad(const char *path);
int configGetInt(const char *key, int def);
void configSetInt(const char *key, int val);
void configSave(const char *path);

#endif // PERSIST_H
