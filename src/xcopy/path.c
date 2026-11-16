//
// XCOPY: bounded path building and canonical paths
//

#include <arch/zxn/esxdos.h>
#include <errno.h>
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

// Canonical form of directory dir in out: drive, 8.3 names in upper
// case and a trailing "/", whatever spelling dir uses (long or short
// names, any case, relative, "..").
// Returns 0, or an esxDOS error code.
uint8_t path_canonical(char *dir, char *out)
{
    uint16_t len = path_push(dir, "X");

    if (len == 0xffff) return ESX_ENAMETOOLONG;

    errno = 0;
    out[0] = 0;
    esx_f_get_canonical_path(dir, out);
    path_pop(dir, len);

    if (errno) return errno;
    if ((out[1] != ':') || (out[2] != '/')) return ESX_ENOTDIR;
    return 0;
}

// Is canonical directory inner the same as, or below, canonical outer?
uint8_t path_inside(const char *inner, const char *outer)
{
    return strnicmp(inner, outer, strlen(outer)) == 0;
}
