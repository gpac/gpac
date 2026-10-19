#include "tests.h"

#if !defined(GPAC_DISABLE_DASHIN)

#include "../dmx_dash.c"

static Bool probed_as_dash(const char *doc)
{
	// the prober writes into the buffer while scanning it
	char buf[512];
	GF_FilterProbeScore score = GF_FPROBE_NOT_SUPPORTED;
	u32 len = (u32) gf_strlcpy(buf, doc, sizeof(buf));
	const char *mime = dashdmx_probe_data((u8 *) buf, len, &score);
	return mime && !strcmp(mime, "application/dash+xml") && (score == GF_FPROBE_SUPPORTED);
}

unittest(dashdmx_probe_start_tag_whitespace)
{
	// element names followed by a newline, tab or CRLF, as some packagers write them
	assert_true(probed_as_dash("<?xml version=\"1.0\"?>\n<MPD\n  xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"dynamic\">\n<Period\n  id=\"1\">\n</Period>\n</MPD>\n"));
	assert_true(probed_as_dash("<MPD\txmlns=\"urn:mpeg:dash:schema:mpd:2011\"></MPD>"));
	assert_true(probed_as_dash("<Period\r\n id=\"1\"></Period>"));
	// controls: a space still matches, longer element names do not
	assert_true(probed_as_dash("<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\"></MPD>"));
	assert_false(probed_as_dash("<MPDx a=\"1\"/><Periods a=\"1\"/>"));
}

#else
unittest_disabled(dashdmx_probe_start_tag_whitespace, "DASH input disabled")
#endif
