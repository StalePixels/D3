//
// XCOPY: match a long name on an NBN server against a pattern (-z).
// In the main bank: the 8K dot part has no room for it.
//

#include <ctype.h>
#include <string.h>

#include "xcopy.h"

// Does the long name match pattern, as NextZXOS matches 8.3 names?
// The name and the extension, after the last ".", are matched apart and
// in any case. "?" matches one character, or none at the end of a part;
// "*" matches the rest of its part. No "." in the pattern matches only
// names with no extension.
static uint8_t match_part(const char *p, const char *pe, const char *n, const char *ne)
{
    char pc, nc;

    for (;;) {
        pc = (p < pe) ? *p++ : 0;
        nc = (n < ne) ? *n++ : 0;
        if (pc == '*') return 1;
        if (!pc && !nc) return 1;
        if (pc != '?' && toupper(pc) != toupper(nc)) return 0;
    }
}

uint8_t nbn_match(const char *pat, const char *name)
{
    const char *pd = strrchr(pat, '.');
    const char *nd = strrchr(name, '.');
    const char *pz = pat + strlen(pat);
    const char *nz = name + strlen(name);

    return match_part(pat, pd ? pd : pz, name, nd ? nd : nz)
        && match_part(pd ? pd + 1 : pz, pz, nd ? nd + 1 : nz, nz);
}
