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
CFLAGS += -std=c11 -Wall -Wextra -pedantic -O2
LDFLAGS += -lm
TARGET = poom
SOURCES = poom.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
