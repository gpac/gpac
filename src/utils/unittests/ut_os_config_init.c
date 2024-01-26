#include <gpac/main.h>
#include "tests.h"

unittest(gf_sys_word_match)
{
    gf_assert(gf_sys_word_match("toto", "toto") == GF_TRUE); //TODO: provide more assert flavors
    verbose_assert(gf_sys_word_match("toto", "toto") == GF_TRUE);
    custom_message_assert(gf_sys_word_match("toto", "toto") == GF_TRUE, "toto must  match");
    custom_action_assert( gf_sys_word_match("toto", "tot") == GF_TRUE , printf("Assertion failed: toto has lost an o \n"));

    return EXIT_SUCCESS;
}
