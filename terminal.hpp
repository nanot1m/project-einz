#ifndef PROJECT_EINZ_TERMINAL_HPP
#define PROJECT_EINZ_TERMINAL_HPP

// POSIX terminal backend. Self-contained: include this and it works.
// Also owns the main loop (`run_terminal`), because the loop's timing
// model (sleep_until + spin-wait, SIGINT handler) is itself POSIX-specific.

#include "platform.hpp"
#include "engine.hpp"

#include <thread>
#include <csignal>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

// ============================================================
// POSIX terminal-specific Renderer + InputSource.
// To port to another platform, write a new Renderer/InputSource
// pair and swap them in main().
// ============================================================

class TerminalRenderer : public Renderer
{
private:
   termios original_termios;
   bool raw_mode_active = false;
   DynamicArray<Cell> prev_cells;

public:
   ~TerminalRenderer() override { shutdown(); }

   void init() override
   {
      tcgetattr(STDIN_FILENO, &original_termios);
      termios raw = original_termios;
      raw.c_lflag &= ~(ICANON | ECHO);
      raw.c_cc[VMIN] = 0;
      raw.c_cc[VTIME] = 0;
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
      std::cout << "\033[?1049h\033[?25l" << std::flush;
      raw_mode_active = true;
   }

   void shutdown() override
   {
      if (!raw_mode_active)
         return;
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
      std::cout << "\033[?25h\033[?1049l" << std::flush;
      raw_mode_active = false;
   }

   void present(const RenderFrame &frame) override
   {
      static const char *color_seqs[] = {
         "\033[0;1;34m",      // 0 player blue
         "\033[0;1;31m",      // 1 enemy red / heart filled
         "\033[0;1;35m",      // 2 strong magenta
         "\033[0;1;33m",      // 3 yellow
         "\033[0;1;37m",      // 4 bullets white
         "\033[0;1;32m",      // 5 grass / tree green
         "\033[0;38;5;240m",  // 6 wall dim
         "\033[0;90m",        // 7 floor gray / empty heart
         "\033[0m",           // 8 HUD default
         "\033[0;1;36m",      // 9 water bold cyan
         "\033[0;38;5;130m",  // 10 bridge brown
      };

      size_t expected = static_cast<size_t>(frame.width * frame.height);
      while (prev_cells.get_size() < expected)
         prev_cells.push_back(Cell{});

      std::string buffer;
      buffer.reserve(2048);

      int last_x = -2, last_y = -2;
      int last_color = -2;

      for (int y = 0; y < frame.height; ++y)
      {
         for (int x = 0; x < frame.width; ++x)
         {
            size_t idx = static_cast<size_t>(y * frame.width + x);
            const Cell &cur = frame.cells[idx];
            Cell &prv = prev_cells[idx];
            if (cur.ch == prv.ch && cur.color == prv.color)
               continue;

            if (y != last_y || x != last_x + 1)
            {
               buffer += "\033[";
               buffer += std::to_string(y + 1);
               buffer += ";";
               buffer += std::to_string(x + 1);
               buffer += "H";
            }

            if (cur.color != last_color)
            {
               if (cur.color < 0)
                  buffer += "\033[0m";
               else
                  buffer += color_seqs[cur.color];
               last_color = cur.color;
            }

            switch (cur.ch)
            {
               case '#': buffer += "\xe2\x96\x93"; break;
               case 'H': buffer += "\xe2\x99\xa5"; break;
               case 'h': buffer += "\xe2\x99\xa1"; break;
               default:  buffer += cur.ch;
            }

            prv = cur;  // sync the previous-frame snapshot
            last_x = x;
            last_y = y;
         }
      }

      if (!buffer.empty())
      {
         buffer += "\033[0m";
         std::cout << buffer << std::flush;
      }
   }
};

class TerminalInput : public InputSource
{
public:
   Key poll() override
   {
      char buf[8];
      ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
      if (n <= 0)
         return Key::None;
      if (n == 1)
      {
         if (buf[0] == 'q' || buf[0] == 'Q') return Key::Quit;
         if (buf[0] == 'r' || buf[0] == 'R') return Key::Restart;
         if (buf[0] == ' ' || buf[0] == 'j' || buf[0] == 'J') return Key::Shoot;
      }
      if (n >= 3 && buf[0] == '\x1b' && buf[1] == '[')
      {
         switch (buf[2])
         {
            case 'A': return Key::Up;
            case 'B': return Key::Down;
            case 'C': return Key::Right;
            case 'D': return Key::Left;
         }
      }
      return Key::None;
   }
};

// ============================================================
// Main loop driver. The engine just hands us its World; we own
// the loop, the timing, and the SIGINT handler.
// ============================================================

namespace
{
   volatile sig_atomic_t terminal_quit_requested = 0;
}

inline int run_terminal(World &world, int target_fps, float fps_ema_alpha)
{
   std::ios::sync_with_stdio(false);

   TerminalRenderer renderer;
   TerminalInput input;

   renderer.init();
   world.init();

   std::signal(SIGINT, [](int) { terminal_quit_requested = 1; });

   const auto frame_duration = std::chrono::microseconds(1000000 / target_fps);
   auto last = std::chrono::steady_clock::now();
   auto next_frame = last + frame_duration;
   float ema_dt = 1.0f / target_fps;

   while (!terminal_quit_requested)
   {
      Key key = input.poll();
      if (key == Key::Quit)
         break;
      world.handle_input(key);

      auto now = std::chrono::steady_clock::now();
      float dt = std::chrono::duration<float>(now - last).count();
      last = now;
      ema_dt = ema_dt * (1.0f - fps_ema_alpha) + dt * fps_ema_alpha;
      float fps = ema_dt > 0.0f ? 1.0f / ema_dt : 0.0f;

      world.update(dt);
      world.prepare_frame(fps);
      renderer.present(world.get_frame());

      auto sleep_until = next_frame - std::chrono::milliseconds(1);
      if (std::chrono::steady_clock::now() < sleep_until)
         std::this_thread::sleep_until(sleep_until);
      while (std::chrono::steady_clock::now() < next_frame)
      {
      }
      next_frame += frame_duration;
   }

   // Renderer destructor will call shutdown() and restore the terminal.
   return 0;
}

#endif // PROJECT_EINZ_TERMINAL_HPP
