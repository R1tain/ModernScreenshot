APP := modern-screenshot
VERSION ?= 0.1.0
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SYSTEMD_USER_DIR ?= $(PREFIX)/lib/systemd/user

CC ?= cc
PKG_CFLAGS := $(shell pkg-config --cflags x11 xrender 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs x11 xrender 2>/dev/null)
X11_LIBS := $(if $(PKG_LIBS),$(PKG_LIBS),-lX11 -lXrender)

CPPFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS ?= -std=c11 -Os -Wall -Wextra -Wpedantic
CFLAGS += $(PKG_CFLAGS)
LDFLAGS ?=

SRC := src/modern_screenshot.c
BIN := build/$(APP)

.PHONY: all clean release install uninstall smoke memory-check

all: $(BIN)

$(BIN): $(SRC) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(X11_LIBS) -lm

build:
	mkdir -p $@

release: CFLAGS += -fdata-sections -ffunction-sections
release: LDFLAGS += -Wl,--gc-sections -s
release: clean all

install: release
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(APP)
	install -Dm644 packaging/modern-screenshot.service $(DESTDIR)$(SYSTEMD_USER_DIR)/modern-screenshot.service

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(APP)
	rm -f $(DESTDIR)$(SYSTEMD_USER_DIR)/modern-screenshot.service

smoke: all
	$(BIN) --version

memory-check: all
	scripts/check-memory.sh $(BIN)

clean:
	rm -rf build
