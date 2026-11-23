//
// XCOPY: bounded path building
//

#include <string.h>

#include "xcopy.h"

// Append "/name" to path (no "/" after an empty path, "/" or ":").
// Returns the old length, for path_pop, or 0xffff if the result would
// be longer than XC_PATH_MAX; path is then unchanged.
uint16_t path_push(char *path, const char *name)
{
    uint16_t len = strlen(path);
    uint16_t need = len + strlen(name);
    uint8_t sep = len && (path[len - 1] != '/') && (path[len - 1] != ':');

    if (need + sep > XC_PATH_MAX) return 0xffff;

    if (sep) path[len] = '/';
    strcpy(path + len + sep, name);
    return len;
}

void path_pop(char *path, uint16_t len)
{
    path[len] = 0;
}

// The part after the last "/" or ":"
const char *path_basename(const char *path)
{
    const char *base = path;

    for (; *path; ++path)
        if ((*path == '/') || (*path == ':')) base = path + 1;

    return base;
}
