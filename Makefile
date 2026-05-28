CXX      := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

EMCC      := emcc
EMCCFLAGS := -O2 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=HEAP8

PORT := 8000

CORE_DEPS := platform.hpp engine.hpp

.PHONY: all terminal wasm run web clean

# Default target builds the terminal binary.
all: terminal

# ---------- Terminal (POSIX) ----------

terminal: einz

einz: project-einz.cpp $(CORE_DEPS) terminal.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: einz
	./einz

# ---------- WebAssembly ----------

wasm: einz.html

einz.html einz.js einz.wasm: wasm-einz.cpp $(CORE_DEPS) wasm.hpp shell.html
	$(EMCC) wasm-einz.cpp -o einz.html --shell-file shell.html $(EMCCFLAGS)

# Build + serve via python's HTTP module; visit http://localhost:$(PORT)/einz.html
web: wasm
	@echo "Open http://localhost:$(PORT)/einz.html"
	python3 -m http.server $(PORT)

# ---------- Housekeeping ----------

clean:
	rm -f einz einz.html einz.js einz.wasm einz.data einz.worker.js *.wasm.map *.js.map
	rm -rf einz_asan einz_asan.dSYM
