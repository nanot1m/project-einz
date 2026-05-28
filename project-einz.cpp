// POSIX terminal entry point. Pick the backend and hand the world to it.
// For a Web/WASM build, write a parallel `wasm-einz.cpp` that includes
// `wasm.hpp` instead and calls `run_wasm(world, ...)`.

#include "engine.hpp"
#include "terminal.hpp"

int main(int argc, char *argv[])
{
   std::srand(static_cast<unsigned>(
      std::chrono::steady_clock::now().time_since_epoch().count()));

   World world(512);
   return run_terminal(world, cfg::TARGET_FPS, cfg::FPS_EMA_ALPHA);
}
