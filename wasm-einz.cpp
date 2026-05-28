// WebAssembly entry point. Builds the World and hands it to the
// wasm backend, which registers a requestAnimationFrame loop.

#include "engine.hpp"
#include "wasm.hpp"

int main(int argc, char *argv[])
{
   std::srand(static_cast<unsigned>(
      std::chrono::steady_clock::now().time_since_epoch().count()));

   World world(512);
   return run_wasm(world, /*target_fps=*/60, cfg::FPS_EMA_ALPHA);
}
