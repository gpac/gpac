# Unit tests in GPAC

## Build and run

Unit tests can be activate with ```configure --enable-unittests```.

Unit tests require extra visibility of symbols. That's why a second build is required. This is done automatically.

At the moment unit tests are executed each time a build occurs. Note: it could happen along with other reformatting and checks in a ```precommit``` command.

# Writing unit tests

Include the unit test framework:
```
#include "tests.h"
```

If the function is static, replace ```static``` with ```GF_STATIC```. Then add a definition in your unit test file:
```
char *xml_translate_xml_string(char *str);

```

Then write test functions:
```
unittest(xml_translate_xml_string)
{
    assert_equal_str(xml_translate_xml_string("&amp;"), "&");
}
```
