CXX      := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

EMCC      := emcc
EMCCFLAGS := -O2 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=HEAP8

PORT := 8000

CORE_DEPS := platform.hpp engine.hpp

.PHONY: all terminal run web pages clean

# Default target builds the terminal binary.
all: terminal

# ---------- Terminal (POSIX) ----------

terminal: einz

einz: project-einz.cpp $(CORE_DEPS) terminal.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: einz
	./einz

# ---------- WebAssembly / GitHub Pages ----------
# `make pages` builds the browser bundle into ./docs (consumed by the
# Pages workflow in CI). `make web` builds + serves it locally.

pages: docs/index.html

docs:
	mkdir -p docs

docs/index.html: wasm-einz.cpp $(CORE_DEPS) wasm.hpp shell.html | docs
	$(EMCC) wasm-einz.cpp -o docs/index.html --shell-file shell.html $(EMCCFLAGS)

web: pages
	@echo "Open http://localhost:$(PORT)/"
	python3 -m http.server $(PORT) -d docs

# ---------- Housekeeping ----------

clean:
	rm -f einz *.wasm.map *.js.map
	rm -rf docs
