# ---------- COMMON VARIABLES ---------- #

BUILD_DIR = build
GIT_TAG = $(shell git describe --tags --abbrev=0)

# ---------- BUILD DIR ---------- #

$(BUILD_DIR):
	mkdir -p $@

# ---------- LIBFFI ---------- #

LIBFFI_VER = 3.8.0
LIBFFI_STATIC_LIB = libffi.a
LIBFFI_TARGET = $(BUILD_DIR)/$(LIBFFI_STATIC_LIB)
LIBFFI_ARCHIVE = libffi-$(LIBFFI_VER).tar.gz
LIBFFI_LINK = https://github.com/libffi/libffi/releases/download/v$(LIBFFI_VER)/$(LIBFFI_ARCHIVE)

$(LIBFFI_TARGET): | $(BUILD_DIR)
	cd $(BUILD_DIR) && \
	wget $(LIBFFI_LINK) && \
	tar -xzf $(LIBFFI_ARCHIVE) && rm -f $(LIBFFI_ARCHIVE) && \
	cd libffi-$(LIBFFI_VER) && \
	./configure --disable-shared --disable-docs && \
	$(MAKE) && \
	cp x86_64-pc-linux-gnu/.libs/$(LIBFFI_STATIC_LIB) ../

# ---------- QUO CLI ---------- #

QUO_CLI_TARGET = $(BUILD_DIR)/quo
QUO_SOURCES = $(wildcard cli/*.c)
QUO_HEADERS = $(wildcard include/*.h)
CFLAGS += -Wall -Wextra
LDFLAGS = -L$(BUILD_DIR) -l:$(LIBFFI_STATIC_LIB) -lcurl -lm

ifdef DEBUG
    CFLAGS += -g
ifdef SANITIZE
    CFLAGS += -fsanitize=address
endif
ifdef GPERF
    CFLAGS += -pg -O2
endif
else
    CFLAGS += -O3 -DNDEBUG
endif

$(QUO_CLI_TARGET): $(LIBFFI_TARGET) $(QUO_SOURCES) $(QUO_HEADERS)
	$(CC) $(CFLAGS) -o $@ $(QUO_SOURCES) $(LDFLAGS)

# ---------- TESTS ---------- #

check: $(QUO_CLI_TARGET)
	@cd tests  && \
	for f in *; do \
		name=$$(basename "$$f"); \
		printf "Test: %-20s " "$$name"; \
		../$(QUO_CLI_TARGET) "$$name"; \
		if [ $$? -eq 0 ]; then \
			printf "\033[32mPASS\033[0m\n"; \
		else \
			printf "\033[31mFAIL\033[0m\n"; exit 1; \
		fi; \
	done

# ---------- DISTRIBUTION ---------- #

DIST_ARCHIVE = quo-$(GIT_TAG)-linux.tar.gz

dist: $(QUO_CLI_TARGET)
	tar -czvf $(BUILD_DIR)/$(DIST_ARCHIVE) -C $(BUILD_DIR) quo -C .. include

# ---------- RUN ---------- #

ifdef DEBUG
    RUNNER = gdb -ex run -ex bt -ex quit --args
else
    RUNNER =
endif

run: $(QUO_CLI_TARGET)
	$(RUNNER) $(QUO_CLI_TARGET) test.quo

# ---------- UTILS ---------- #

sloc:
	@echo "quo.h: $(shell cat include/quo.h | wc -l) lines"
	@echo "quo-mod-*.h: $(shell  cat include/quo-mod-*.h | wc -l) lines"

# --- GIT UTILS --- #

git-commit:
	@printf "Commit message: "
	@read msg
	git add .
	git commit -m "$$msg"

git-tag-create:
	@echo "Latest tag: $(GIT_TAG)"
	@printf "Enter new version (e.g 0.0.0): "
	@read tag
	@git tag -a $$tag -m "Release $$tag"
	@git push origin $$tag

git-tag-delete:
	@echo "Latest tag: $(GIT_TAG)"
	@echo "Enter tag to delete: "
	@read tag
	@git tag -d $$tag
	@git push origin --delete $$tag

# ---------- CLEAN ---------- #

clean:
	rm -rf $(BUILD_DIR)

# ---------- MAKE CONFIG ---------- #

.DEFAULT_GOAL := $(QUO_CLI_TARGET)
.PHONY: all run clean dist check stats git-commit git-tag-create git-tag-delete
