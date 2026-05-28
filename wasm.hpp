#ifndef PROJECT_EINZ_WASM_HPP
#define PROJECT_EINZ_WASM_HPP

// WebAssembly backend (emscripten). The main loop is driven by the
// browser's requestAnimationFrame via `emscripten_set_main_loop`;
// we can't block in a `while(true)` — that would freeze the tab.
//
// Rendering is delegated to JS (shell.html owns the DOM grid + diff
// cache). C++ just hands the cell buffer pointer over EM_ASM.

#ifndef __EMSCRIPTEN__
#error "wasm.hpp is only usable when compiling with emscripten (emcc)"
#endif

#include "platform.hpp"
#include "engine.hpp"

#include <emscripten.h>
#include <chrono>

class WasmRenderer : public Renderer
{
public:
   void init() override
   {
      // Nothing: the JS shell creates the DOM grid lazily on the first
      // render_frame() call (because that's when it learns the dimensions).
   }

   void shutdown() override {}

   void present(const RenderFrame &frame) override
   {
      EM_ASM(
         {
            render_frame($0, $1, $2);
         },
         frame.cells, frame.width, frame.height);
   }
};

class WasmInput : public InputSource
{
public:
   void init()
   {
      EM_ASM({
         install_input_handler();
      });
   }

   Key poll() override
   {
      int k = EM_ASM_INT({
         return poll_key();
      });
      return static_cast<Key>(k);
   }
};

// Globals so the rAF callback (which takes no user pointer) can find
// the state. One-per-process; we only run a single game at a time.
namespace
{
   WasmRenderer *g_wasm_renderer = nullptr;
   WasmInput    *g_wasm_input    = nullptr;
   World        *g_wasm_world    = nullptr;
   double        g_wasm_last_ms  = 0.0;
   float         g_wasm_ema_dt   = 1.0f / 60.0f;
   float         g_wasm_alpha    = 0.05f;
}

inline void wasm_tick()
{
   double now_ms = emscripten_get_now();
   float dt = static_cast<float>((now_ms - g_wasm_last_ms) / 1000.0);
   g_wasm_last_ms = now_ms;
   g_wasm_ema_dt = g_wasm_ema_dt * (1.0f - g_wasm_alpha) + dt * g_wasm_alpha;
   float fps = g_wasm_ema_dt > 0.0f ? 1.0f / g_wasm_ema_dt : 0.0f;

   // Drain the entire key queue this frame so e.g. rapid taps aren't lost.
   while (true)
   {
      Key k = g_wasm_input->poll();
      if (k == Key::None) break;
      g_wasm_world->handle_input(k);
   }

   g_wasm_world->update(dt);
   g_wasm_world->prepare_frame(fps);
   g_wasm_renderer->present(g_wasm_world->get_frame());
}

inline int run_wasm(World &world, int target_fps, float fps_ema_alpha)
{
   static WasmRenderer renderer;
   static WasmInput input;

   g_wasm_renderer = &renderer;
   g_wasm_input    = &input;
   g_wasm_world    = &world;
   g_wasm_alpha    = fps_ema_alpha;
   g_wasm_ema_dt   = 1.0f / static_cast<float>(target_fps);
   g_wasm_last_ms  = emscripten_get_now();

   renderer.init();
   input.init();
   world.init();

   // fps = 0 → drive from requestAnimationFrame (whatever the browser allows,
   // typically the display's refresh rate). simulate_infinite_loop = 1
   // throws an exception in C++ that emscripten catches; this returns to
   // browser event loop without unwinding stack locals (which would destroy
   // World/the renderers prematurely).
   emscripten_set_main_loop(wasm_tick, 0, 1);
   return 0;  // unreachable
}

#endif // PROJECT_EINZ_WASM_HPP
