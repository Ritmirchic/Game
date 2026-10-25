# DON'T LOOK BACK -- NIGHTFALL engine build
CXX      ?= g++
ZIG      ?= zig
SRC       = src/nf_core.cpp src/nf_render.cpp src/nf_audio.cpp src/nf_platform.cpp \
            src/dl_sfx.cpp src/dl_geom.cpp src/dl_world.cpp src/dl_game.cpp \
            src/dl_anom.cpp src/dl_ai.cpp src/dl_ui.cpp src/dl_test.cpp src/main.cpp
OBJ       = $(SRC:src/%.cpp=build/obj/%.o)
OBJ_WIN   = $(SRC:src/%.cpp=build/win/%.o)
INC       = -Isrc -O2 -std=c++17 -MMD -MP
DEFS      = -DNOMINMAX
LDLIBS    = -lwinmm -lgdi32 -luser32 -lshell32 -lole32
WARN      = -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare

all: build/DONT_LOOK_BACK_linux
win: dist/DONT_LOOK_BACK.exe

build/obj/%.o: src/%.cpp
	@mkdir -p build/obj
	$(CXX) $(INC) $(DEFS) $(WARN) -c $< -o $@

build/win/%.o: src/%.cpp
	@mkdir -p build/win
	$(ZIG) c++ -target x86_64-windows-gnu $(INC) $(DEFS) $(WARN) -c $< -o $@

build/DONT_LOOK_BACK_linux: $(OBJ)
	$(CXX) $(OBJ) -o $@ -lpthread -lm

dist/DONT_LOOK_BACK.exe: $(OBJ_WIN)
	@mkdir -p dist
	$(ZIG) c++ -target x86_64-windows-gnu $(OBJ_WIN) -o $@ -lwinmm -lgdi32 -luser32 -lshell32 -lole32 -ldwmapi -Wl,--subsystem,windows -Wl,-s

build/bench: tools/bench.cpp $(filter-out build/obj/main.o,$(OBJ))
	$(CXX) $(INC) $(DEFS) $(WARN) tools/bench.cpp $(filter-out build/obj/main.o,$(OBJ)) -o $@ -lpthread -lm

# ---- Windows installer (self-extracting, per-user, creates shortcuts)
dist/_setup_stub.exe: tools/installer.cpp
	@mkdir -p dist
	$(ZIG) c++ -target x86_64-windows-gnu -O2 -std=c++17 -DUNICODE -D_UNICODE -municode \
		-Wl,--subsystem,windows -Wl,-s tools/installer.cpp -o $@ \
		-lole32 -lshell32 -luser32 -lgdi32 -luuid

setup: dist/_setup_stub.exe
	python3 tools/make_setup.py

bench: build/bench

tests: build/testworld

build/testworld: tools/testworld.cpp $(filter-out build/obj/main.o,$(OBJ))
	$(CXX) $(INC) $(DEFS) tools/testworld.cpp $(filter-out build/obj/main.o,$(OBJ)) -o $@ -lpthread -lm

clean:
	rm -rf build/obj build/win

.PHONY: all win tests bench setup clean

-include $(OBJ:.o=.d)
