# Included from src/Makefile: reuse its compiler flags and library dependencies.
UT_DIR=../unittests/build
UT_SRCS:=$(sort $(patsubst $(SRC_PATH)/src/%,%,$(wildcard $(SRC_PATH)/src/*/unittests/ut_*.c)))
UT_OBJS:=$(addprefix $(UT_DIR)/,$(UT_SRCS:.c=.o))
UT_LISTS:=$(UT_OBJS:.o=.list)
UT_RUNNER=$(UT_DIR)/runner.c
UT_SUPPORT=$(UT_DIR)/tests.o $(UT_DIR)/runner.o
UT_CONFIG=../config.h ../config.mak $(SRC_PATH)/unittests/unit_tests.mak $(SRC_PATH)/src/Makefile $(SRC_PATH)/static.mak
UT_FLAGS=-Wno-unused-variable -Wno-unused-function

.PHONY: unit_tests unit_tests_clean unit_tests_registry
ifeq ($(UNIT_TESTS),yes)
unit_tests: ../bin/gcc/unittests$(EXE_SUFFIX) ../bin/gcc/unittests-selftest$(EXE_SUFFIX)
endif

# Test objects precede the archive. An included implementation supplies all its
# external symbols, so the original archive member is not pulled into this link.
../bin/gcc/unittests$(EXE_SUFFIX): $(UT_OBJS) $(UT_SUPPORT) ../bin/gcc/libgpac_static.a $(UT_CONFIG)
	@echo "  LINK $@"
	$(CC) $(CFLAGS) -o $@ $(UT_OBJS) $(UT_SUPPORT) ../bin/gcc/libgpac_static.a $(ALL_LIBS) $(LDFLAGS)

../bin/gcc/unittests-selftest$(EXE_SUFFIX): $(UT_DIR)/selftest.o $(UT_DIR)/tests.o ../bin/gcc/libgpac_static.a $(UT_CONFIG)
	@echo "  LINK $@"
	$(CC) $(CFLAGS) -o $@ $(UT_DIR)/selftest.o $(UT_DIR)/tests.o ../bin/gcc/libgpac_static.a $(ALL_LIBS) $(LDFLAGS)

$(UT_DIR)/filters/%.o $(UT_DIR)/filters/%.list: CFLAGS += $(FILTERS_CFLAGS)
$(UT_DIR)/compositor/%.o $(UT_DIR)/compositor/%.list: CFLAGS += $(COMPOSITOR_CFLAGS)
$(UT_DIR)/media_tools/%.o $(UT_DIR)/media_tools/%.list: CFLAGS += $(MEDIATOOLS_CFLAGS)
$(UT_DIR)/filters/unittests/ut_dec_cc.o $(UT_DIR)/filters/unittests/ut_dec_cc.list: CFLAGS += $(caption_cflags)
$(UT_DIR)/utils/unittests/ut_downloader_cache.o $(UT_DIR)/utils/unittests/ut_downloader_cache.list: CFLAGS := $(ngtcp2_cflags) $(nghttp3_cflags) $(nghttp2_cflags) $(ssl_cflags) $(CFLAGS)

$(UT_DIR)/%.o: %.c $(UT_CONFIG)
	@mkdir -p $(dir $@)
	@echo "  UT $<"
	$(CC) $(CFLAGS) $(UT_FLAGS) -MMD -MP -MF $(@:.o=.d) -MT $@ -MT $(@:.o=.list) -c $< -o $@

# Preprocessing uses the same flags as compilation and honours feature guards.
# Keep this separate from compilation so no discovery-only macros reach objects.
$(UT_DIR)/%.list: %.c $(UT_CONFIG) $(SRC_PATH)/unittests/build.sh
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(UT_FLAGS) -DGPAC_UNIT_TEST_LIST -E $< -o $@.i
	@$(SRC_PATH)/unittests/build.sh --list $@.i > $@.tmp
	@mv $@.tmp $@
	@rm -f $@.i

# Checking the small list files also detects added/removed test sources. Keep the
# runner's timestamp unchanged when registration has not changed.
$(UT_RUNNER): unit_tests_registry $(UT_LISTS) $(SRC_PATH)/unittests/build.sh
	@mkdir -p $(dir $@)
	@$(SRC_PATH)/unittests/build.sh $(UT_LISTS) > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

$(UT_DIR)/runner.o: $(UT_RUNNER) $(UT_CONFIG)
	@echo "  UT $<"
	$(CC) $(CFLAGS) $(UT_FLAGS) -MMD -MP -c $< -o $@

$(UT_DIR)/tests.o: $(SRC_PATH)/unittests/tests.c $(UT_CONFIG)
	@mkdir -p $(dir $@)
	@echo "  UT $<"
	$(CC) $(CFLAGS) $(UT_FLAGS) -MMD -MP -c $< -o $@

$(UT_DIR)/selftest.o: $(SRC_PATH)/unittests/selftest.c $(UT_CONFIG)
	@mkdir -p $(dir $@)
	@echo "  UT $<"
	$(CC) $(CFLAGS) $(UT_FLAGS) -MMD -MP -c $< -o $@

-include $(UT_OBJS:.o=.d) $(UT_SUPPORT:.o=.d) $(UT_DIR)/selftest.d

unit_tests_clean:
	rm -rf $(UT_DIR)
	rm -f ../bin/gcc/unittests$(EXE_SUFFIX) ../bin/gcc/unittests-selftest$(EXE_SUFFIX)
