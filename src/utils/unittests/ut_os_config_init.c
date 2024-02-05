#include <gpac/main.h>
#include "tests.h"

unittest(gf_sys_word_match)
{
    // gf_assert(gf_sys_word_match("toto", "toto") == GF_TRUE); //TODO: provide more assert flavors
    // verbose_assert(gf_sys_word_match("toto", "toto") == GF_TRUE);
    // custom_message_assert(gf_sys_word_match("toto", "toto") == GF_TRUE, "toto must  match");
    // custom_action_assert( gf_sys_word_match("toto", "tot") == GF_TRUE , printf("Assertion failed: toto has lost an o \n"));

    // Test exact match scenario
    assert_true(gf_sys_word_match("abc", "abc"));
    
    // Test short orig, longer dst scenario
    assert_true(gf_sys_word_match("abc", "abcd"));

    // Test short dst, longer orig scenario
    assert_true(gf_sys_word_match("abcd", "abc"));

    // Test scenario with ':' in orig but not in dst
    assert_false(gf_sys_word_match("a:b:c", "xyz"));

    // Test scenario with ':' in dst but not in orig
    assert_false(gf_sys_word_match("abc", "x:y:z"));

    // Test strnistr match scenario
    assert_true(gf_sys_word_match("abc", "xabcz"));

    // Test repeated characters scenario
    assert_false(gf_sys_word_match("aabbc", "axayabaz"));

    // Test match*2 < olen scenario
    assert_false(gf_sys_word_match("abc", "xyz"));

    // Test half characters in order scenario
    assert_true(gf_sys_word_match("abcd", "aebfcd"));

    // Test null pointer
    assert_true(gf_sys_word_match(NULL, NULL));
    assert_false(gf_sys_word_match("abc", NULL));
    assert_false(gf_sys_word_match(NULL, "abc"));

    // Test empty string
    assert_true(gf_sys_word_match("", ""));
    assert_false(gf_sys_word_match("abc", ""));
    assert_false(gf_sys_word_match("", "abc"));

    // Test a very long string
    const char *longString = "a very long string that exceeds the normal limit of the function, it may go further and further until it can break the behaviour of the function apparently not easy so lets write more and more";
    assert_true(gf_sys_word_match(longString, longString));
    assert_false(gf_sys_word_match("abc", longString));
    assert_false(gf_sys_word_match(longString, "abc"));

    // Test a non-ASCII buffer
    const char *nonAsciiBuffer = "\x01\x02\x03\xFF\xFE\xFD"; 
    assert_false(gf_sys_word_match("abc", nonAsciiBuffer));
    assert_false(gf_sys_word_match(nonAsciiBuffer, "abc"));
    assert_true(gf_sys_word_match(nonAsciiBuffer, nonAsciiBuffer));

    return EXIT_SUCCESS;
}
