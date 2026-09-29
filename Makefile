# One Hour - build
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -pipe -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -fno-exceptions -fno-rtti
LDFLAGS  ?=

# SDL2: use the system dev package when present, otherwise the vendored headers + the runtime library
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
ifeq ($(SDL_LIBS),)
  SDL_CFLAGS := -Ithird_party/SDL2/include -D_REENTRANT
  SDL_SO     := $(firstword $(wildcard /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 /usr/lib/x86_64-linux-gnu/sdl2-classic/libSDL2-2.0.so.0 /usr/lib/libSDL2-2.0.so.0 /usr/lib64/libSDL2-2.0.so.0))
  ifeq ($(SDL_SO),)
    SDL_LIBS := -lSDL2
  else
    SDL_LIBS := $(SDL_SO)
  endif
endif

SRC := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC))
BIN := build/onehour

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(SDL_LIBS) $(LDFLAGS) -lm

build/%.o: src/%.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
