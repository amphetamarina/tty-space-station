CC = cc

# Detect operating system
UNAME_S := $(shell uname -s)

# SDL2 configuration
SDL_CONFIG := $(shell which sdl2-config 2>/dev/null)
ifeq ($(SDL_CONFIG),)
SDL_CONFIG := $(shell which sdl-config 2>/dev/null)
endif
SDL_CFLAGS ?= $(if $(SDL_CONFIG),$(shell $(SDL_CONFIG) --cflags 2>/dev/null))
SDL_LIBS ?= $(if $(SDL_CONFIG),$(shell $(SDL_CONFIG) --libs 2>/dev/null))
ifeq ($(SDL_CFLAGS),)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
endif

# macOS Homebrew support
ifeq ($(UNAME_S),Darwin)
    # Check for Homebrew installation paths
    # Apple Silicon Macs
    ifneq (,$(wildcard /opt/homebrew/include))
        SDL_CFLAGS += -I/opt/homebrew/include
        SDL_LIBS += -L/opt/homebrew/lib
    endif
    # Intel Macs
    ifneq (,$(wildcard /usr/local/include))
        SDL_CFLAGS += -I/usr/local/include
        SDL_LIBS += -L/usr/local/lib
    endif
endif

ifneq ($(SDL_CFLAGS),)
CFLAGS += $(SDL_CFLAGS)
endif
ifneq ($(SDL_LIBS),)
LDFLAGS += $(SDL_LIBS)
endif

# Include directories
CFLAGS += -std=c11 -Wall -Wextra -pedantic -O2 -Isrc -Iinclude

# Platform-specific linker flags
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -lm
else
LDFLAGS += -lm -lutil
endif

TARGET = tty-space-station
MAPEDITOR = mapeditor

# Source files
SOURCES = src/main.c \
          src/game.c \
          src/player.c \
          src/map.c \
          src/cabinet.c \
          src/display.c \
          src/terminal.c \
          src/renderer.c \
          src/ui.c \
          src/texture.c \
          src/utils.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run editor

all: $(TARGET) $(MAPEDITOR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

$(MAPEDITOR): tools/mapeditor.c
	$(CC) $(CFLAGS) tools/mapeditor.c $(LDFLAGS) -o $(MAPEDITOR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

editor: $(MAPEDITOR)
	./$(MAPEDITOR) maps/palace.map

clean:
	rm -f $(TARGET) $(MAPEDITOR) $(OBJECTS)
