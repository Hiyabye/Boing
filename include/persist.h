#ifndef PERSIST_H
#define PERSIST_H

#include <stdbool.h>

// Tiny plain-text persistence for runtime files beside the exe (high score,
// settings). Each file is n space-separated ints.
//
// readInts fills buf and returns true only if all n ints parsed; on a missing
// or malformed file it returns false and leaves buf untouched, so callers keep
// their defaults. Non-int values (e.g. a volume fraction) are stored scaled to
// an int by the caller.
bool readInts(const char *path, int *buf, int n);
void writeInts(const char *path, const int *buf, int n);

#endif // PERSIST_H
