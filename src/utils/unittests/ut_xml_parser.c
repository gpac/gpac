#include "tests.h"

char *xml_translate_xml_string(char *str);
unittest(xml_translate_xml_string)
{
    assert_equal_str(xml_translate_xml_string("&amp;"), "&");
}
