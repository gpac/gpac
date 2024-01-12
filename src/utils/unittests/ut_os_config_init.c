#include <gpac/main.h>
#include "tests.h"

unittest(gf_sys_word_match)
{
    gf_assert(gf_sys_word_match("toto", "toto") == GF_TRUE); //TODO: provide more assert flavors
    return EXIT_SUCCESS;
}
