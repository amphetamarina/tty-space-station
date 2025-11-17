CC = cc
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
ifneq ($(SDL_CFLAGS),)
CFLAGS += $(SDL_CFLAGS)
endif
ifneq ($(SDL_LIBS),)
LDFLAGS += $(SDL_LIBS)
endif

# Include directories
CFLAGS += -std=c11 -Wall -Wextra -pedantic -O2 -Isrc -Iinclude
LDFLAGS += -lm

TARGET = poom
MAPEDITOR = mapeditor

# Source files
SOURCES = src/main.c \
          src/game.c \
          src/player.c \
          src/map.c \
          src/memory.c \
          src/furniture.c \
          src/npc.c \
          src/renderer.c \
          src/ui.c \
          src/texture.c \
          src/utils.c \
          src/network.c

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
