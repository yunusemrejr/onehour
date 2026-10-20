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
BUILD_DIR ?= build
OBJ := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SRC))
BIN := $(BUILD_DIR)/onehour

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(SDL_LIBS) $(LDFLAGS) -lm

$(BUILD_DIR)/%.o: src/%.cpp src/*.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
