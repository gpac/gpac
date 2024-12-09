# Unit tests in GPAC

## Build and run

Unit tests can be activate with ```configure --unittests```.

Some unit tests require extra visibility of symbols. That's why a second build is required. This is done automatically.

At the moment unit tests are executed each time a build occurs. Execution happens as soon as possible and it will stop the build on errors. Note: this processing could happen along with other reformatting and checks in a ```precommit``` command.

# Writing unit tests

Include the unit test framework:
```
#include "tests.h"
```

## Static function testing

If the function is static, replace ```static``` with ```GF_STATIC```. If the function is not static and not exported while you need it, add ```GF_NOT_EXPORTED```. Then add a definition in your unit test file:
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

## Testing a full filter

You can include the filter source code file in your test file. This has the advantage of allowing to reuse private contexts in the unit tests:
```
#include "tests.h"
#include "../dec_scte35.c" // include the source code

...

unittest(scte35dec_segmentation_end)
{
    SCTE35DecCtx ctx = {0};
    assert_equal(scte35dec_initialize_internal(&ctx), GF_OK);

    ctx.segdur = (GF_Fraction){1, 1};

    u64 pts = 0;
    SEND_VIDEO(1);
    SEND_EVENT(SCTE35_DUR);

    scte35dec_flush(&ctx);
    scte35dec_finalize_internal(&ctx);

    ctx.pck_send(NULL); // trigger final checks
}
```
