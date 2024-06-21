#include "tests.h"

char *xml_translate_xml_string(char *str);
unittest(xml_translate_xml_string)
{
    char *str= xml_translate_xml_string("&amp;");
    assert_equal_str(str, "&");
    gf_free(str);
}
