#ifndef PROJECT_EINZ_ENGINE_HPP
#define PROJECT_EINZ_ENGINE_HPP

// Engine: game logic only. No I/O, no main loop, no signal/thread.
// Depends only on platform.hpp (shared ABI).

#include "platform.hpp"

// ============================================================
// Tuning constants — change these to fine-tune the game.
// ============================================================
namespace cfg
{
   // World
   constexpr int   WORLD_WIDTH         = 120;
   constexpr int   WORLD_HEIGHT        = 40;

   // Player
   constexpr int   PLAYER_STARTING_AMMO   = 10;
   constexpr int   PLAYER_MAX_AMMO        = 10;
   constexpr int   PLAYER_MAX_HEALTH      = 3;
   constexpr float PLAYER_IMPULSE         = 16.0f;
   constexpr float PLAYER_MAX_SPEED       = 16.0f;
   constexpr float PLAYER_INVINCIBILITY   = 1.0f;

   // Bullet
   constexpr float BULLET_SPEED        = 40.0f;

   // Enemies
   constexpr float ENEMY_REGULAR_SPEED = 2.5f;
   constexpr int   ENEMY_REGULAR_HP    = 1;
   constexpr float ENEMY_STRONG_SPEED  = 1.5f;
   constexpr int   ENEMY_STRONG_HP     = 2;
   constexpr float ENEMY_SWIFT_SPEED   = 5.0f;
   constexpr int   ENEMY_SWIFT_HP      = 1;

   // Spawn intervals (seconds)
   constexpr float SPAWN_PICKUP_INTERVAL = 2.0f;
   constexpr float SPAWN_ENEMY_INTERVAL  = 2.0f;
   constexpr int   AMMO_PACK_RARITY      = 10;  // 1/N chance ammo, else grass

   // Pickup values
   constexpr int   GRASS_AMMO_MIN   = 1;
   constexpr int   GRASS_AMMO_RANGE = 2;        // grass gives MIN..MIN+RANGE-1
   constexpr int   AMMO_PACK_VALUE  = 5;

   // Physics
   constexpr float DAMPING = 20.0f;

   // Effects (seconds)
   constexpr float HIT_EFFECT_DAMAGE_TIME = 0.1f;
   constexpr float HIT_EFFECT_KILL_TIME   = 0.25f;

   // Scoring
   constexpr int   SCORE_PER_KILL = 10;

   // Trees
   constexpr int   INITIAL_TREE_COUNT = 12;

   // Terrain
   constexpr int   LAKE_COUNT          = 2;
   constexpr int   LAKE_MIN_RX         = 4;
   constexpr int   LAKE_MAX_RX         = 8;
   constexpr int   LAKE_MIN_RY         = 2;
   constexpr int   LAKE_MAX_RY         = 4;
   constexpr int   RIVER_COUNT         = 2;
   constexpr int   RIVER_THICKNESS     = 2;
   constexpr int   BRIDGES_PER_RIVER   = 2;

   // Pathfinding
   constexpr float PATHFINDING_INTERVAL = 0.2f;

   // Performance
   constexpr int   TARGET_FPS     = 120;
   constexpr float FPS_EMA_ALPHA  = 0.05f;
}


enum class EntityType
{
   Player,
   Enemy,
   NPC,
   Wall,
   Bullet,
   Grass,
   Ammo,
   Tree
};


enum class Direction
{
   Up,
   Down,
   Left,
   Right
};

enum class GameState
{
   Playing,
   GameOver
};

enum class Tile : unsigned char
{
   Floor,
   Wall,
   Water,
   Bridge,
   Tree
};

class World;

class Entity
{
private:
   EntityType type;
   int id;
   bool in_use = false;

   friend class World;

public:
   Entity() : type(EntityType::NPC), id(-1) {}

   Entity(EntityType t, int i) : type(t), id(i) {}

   int get_id() const { return id; }

   void set_type(EntityType t) { type = t; }

   EntityType get_type() const { return type; }

   bool is_in_use() const { return in_use; }
};

struct HitEffect
{
   int x = 0;
   int y = 0;
   float time = 0.0f;
};


class World
{
private:
   DynamicArray<Entity> entities;
   DynamicArray<int> free_ids;
   DynamicArray<std::pair<float, float>> positions;
   DynamicArray<std::pair<float, float>> velocities;
   DynamicArray<int> hps;
   DynamicArray<float> speeds;

   int player_id = -1;
   int player_ammo = cfg::PLAYER_STARTING_AMMO;
   int max_ammo = cfg::PLAYER_MAX_AMMO;
   int player_health = cfg::PLAYER_MAX_HEALTH;
   int max_health = cfg::PLAYER_MAX_HEALTH;
   int score = 0;
   float invincibility_time = 0.0f;
   GameState game_state = GameState::Playing;
   Direction player_facing = Direction::Right;

   float spawn_pickup_timer = 0.0f;
   float spawn_enemy_timer = 0.0f;

   int width = cfg::WORLD_WIDTH;
   int height = cfg::WORLD_HEIGHT;
   DynamicArray<Tile> terrain;
   DynamicArray<HitEffect> hit_effects;

   DynamicArray<Cell> current_cells;

   DynamicArray<int> dist_field;
   float pathfinding_timer = 0.0f;

   void init_player()
   {
      player_id = spawn(EntityType::Player);
      int px = width / 2;
      int py = height / 2;
      // Make sure the player's tile is safe to stand on.
      if (in_bounds(px, py) && terrain[py * width + px] != Tile::Floor)
         terrain[py * width + px] = Tile::Floor;
      positions[player_id] = {static_cast<float>(px), static_cast<float>(py)};
   }

   int spawn_wall(int x, int y)
   {
      int id = spawn(EntityType::Wall);
      positions[id] = {static_cast<float>(x), static_cast<float>(y)};
      terrain[y * width + x] = Tile::Wall;
      return id;
   }

   void init_walls()
   {
      for (int i = 0; i < width * height; ++i)
         terrain.push_back(Tile::Floor);

      for (int x = 0; x < width; ++x)
      {
         spawn_wall(x, 1);
         spawn_wall(x, height - 1);
      }
      for (int y = 2; y < height - 1; ++y)
      {
         spawn_wall(0, y);
         spawn_wall(width - 1, y);
      }
   }

   bool is_wall(int x, int y) const
   {
      if (x < 0 || x >= width || y < 1 || y >= height)
         return true;
      Tile t = terrain[y * width + x];
      return t == Tile::Wall || t == Tile::Water || t == Tile::Tree;
   }

   bool in_bounds(int x, int y) const
   {
      return x >= 0 && x < width && y >= 1 && y < height;
   }

   void generate_lake(int cx, int cy, int rx, int ry)
   {
      for (int dy = -ry; dy <= ry; ++dy)
      {
         for (int dx = -rx; dx <= rx; ++dx)
         {
            float nx = static_cast<float>(dx) / rx;
            float ny = static_cast<float>(dy) / ry;
            if (nx * nx + ny * ny > 1.0f) continue;
            int x = cx + dx, y = cy + dy;
            if (!in_bounds(x, y)) continue;
            if (terrain[y * width + x] == Tile::Floor)
               terrain[y * width + x] = Tile::Water;
         }
      }
   }

   void generate_river_horizontal()
   {
      int y = 4 + std::rand() % (height - 10);
      DynamicArray<int> river_y_at;
      for (int i = 0; i < width; ++i)
         river_y_at.push_back(-1);

      for (int x = 1; x < width - 1; ++x)
      {
         river_y_at[x] = y;
         for (int t = 0; t < cfg::RIVER_THICKNESS; ++t)
         {
            int ty = y + t;
            if (in_bounds(x, ty) && terrain[ty * width + x] == Tile::Floor)
               terrain[ty * width + x] = Tile::Water;
         }
         if (std::rand() % 4 == 0)
         {
            int dy = (std::rand() % 2 == 0) ? -1 : 1;
            int ny = y + dy;
            if (ny > 3 && ny + cfg::RIVER_THICKNESS < height - 2)
               y = ny;
         }
      }
      // Bridges: only 2 cells per bridge, only inside THIS river's path.
      for (int b = 0; b < cfg::BRIDGES_PER_RIVER; ++b)
      {
         int bx = 5 + std::rand() % (width - 10);
         int y_start = river_y_at[bx];
         if (y_start < 0) continue;
         for (int t = 0; t < cfg::RIVER_THICKNESS; ++t)
         {
            int ty = y_start + t;
            if (in_bounds(bx, ty))
               terrain[ty * width + bx] = Tile::Bridge;
         }
      }
   }

   void generate_river_vertical()
   {
      int x = 4 + std::rand() % (width - 10);
      DynamicArray<int> river_x_at;
      for (int i = 0; i < height; ++i)
         river_x_at.push_back(-1);

      for (int y = 2; y < height - 1; ++y)
      {
         river_x_at[y] = x;
         for (int t = 0; t < cfg::RIVER_THICKNESS; ++t)
         {
            int tx = x + t;
            if (in_bounds(tx, y) && terrain[y * width + tx] == Tile::Floor)
               terrain[y * width + tx] = Tile::Water;
         }
         if (std::rand() % 4 == 0)
         {
            int dx = (std::rand() % 2 == 0) ? -1 : 1;
            int nx = x + dx;
            if (nx > 3 && nx + cfg::RIVER_THICKNESS < width - 2)
               x = nx;
         }
      }
      // Bridges: only inside THIS river's path.
      for (int b = 0; b < cfg::BRIDGES_PER_RIVER; ++b)
      {
         int by = 4 + std::rand() % (height - 8);
         int x_start = river_x_at[by];
         if (x_start < 0) continue;
         for (int t = 0; t < cfg::RIVER_THICKNESS; ++t)
         {
            int tx = x_start + t;
            if (in_bounds(tx, by))
               terrain[by * width + tx] = Tile::Bridge;
         }
      }
   }

   void generate_terrain()
   {
      // Rivers first (they may be partially overwritten by lakes; still ok)
      generate_river_horizontal();
      generate_river_vertical();
      // Lakes
      for (int i = 0; i < cfg::LAKE_COUNT; ++i)
      {
         int cx = 5 + std::rand() % (width - 10);
         int cy = 5 + std::rand() % (height - 10);
         int rx = cfg::LAKE_MIN_RX + std::rand() % (cfg::LAKE_MAX_RX - cfg::LAKE_MIN_RX + 1);
         int ry = cfg::LAKE_MIN_RY + std::rand() % (cfg::LAKE_MAX_RY - cfg::LAKE_MIN_RY + 1);
         generate_lake(cx, cy, rx, ry);
      }
   }

   bool is_wall_at(float fx, float fy) const
   {
      return is_wall(static_cast<int>(fx + 0.5f), static_cast<int>(fy + 0.5f));
   }

   bool find_random_free_2x3_tile(int &out_x, int &out_y, int tries = 50)
   {
      int px = -1, py = -1;
      if (player_id >= 0 && entities[player_id].in_use)
      {
         px = static_cast<int>(positions[player_id].first + 0.5f);
         py = static_cast<int>(positions[player_id].second + 0.5f);
      }
      for (int t = 0; t < tries; ++t)
      {
         int x = 1 + std::rand() % (width - 3);
         int y = 2 + std::rand() % (height - 5);
         bool ok = true;
         for (int dy = 0; dy < 3 && ok; ++dy)
         {
            for (int dx = 0; dx < 2 && ok; ++dx)
            {
               int tx = x + dx, ty = y + dy;
               if (terrain[ty * width + tx] != Tile::Floor) ok = false;
               if (tx == px && ty == py) ok = false;
            }
         }
         if (ok)
         {
            out_x = x;
            out_y = y;
            return true;
         }
      }
      return false;
   }

   int spawn_tree(int x, int y)
   {
      int id = spawn(EntityType::Tree);
      positions[id] = {static_cast<float>(x), static_cast<float>(y)};
      for (int dy = 0; dy < 3; ++dy)
         for (int dx = 0; dx < 2; ++dx)
            terrain[(y + dy) * width + (x + dx)] = Tile::Tree;
      return id;
   }

   void init_trees()
   {
      for (int i = 0; i < cfg::INITIAL_TREE_COUNT; ++i)
      {
         int x, y;
         if (find_random_free_2x3_tile(x, y))
            spawn_tree(x, y);
      }
   }

   bool find_random_free_tile(int &out_x, int &out_y, int tries = 50)
   {
      int px = -1, py = -1;
      if (player_id >= 0 && entities[player_id].in_use)
      {
         px = static_cast<int>(positions[player_id].first + 0.5f);
         py = static_cast<int>(positions[player_id].second + 0.5f);
      }
      for (int i = 0; i < tries; ++i)
      {
         int x = std::rand() % width;
         int y = std::rand() % height;
         if (is_wall(x, y))
            continue;
         if (x == px && y == py)
            continue;
         out_x = x;
         out_y = y;
         return true;
      }
      return false;
   }

   void shoot()
   {
      if (player_ammo <= 0)
         return;
      int id = spawn(EntityType::Bullet);
      positions[id] = positions[player_id];
      switch (player_facing)
      {
         case Direction::Up:    velocities[id] = {0.0f, -cfg::BULLET_SPEED}; break;
         case Direction::Down:  velocities[id] = {0.0f, cfg::BULLET_SPEED};  break;
         case Direction::Left:  velocities[id] = {-cfg::BULLET_SPEED, 0.0f}; break;
         case Direction::Right: velocities[id] = {cfg::BULLET_SPEED, 0.0f};  break;
      }
      player_ammo--;
   }

   void spawn_system(float dt)
   {
      spawn_pickup_timer += dt;
      spawn_enemy_timer += dt;

      if (spawn_pickup_timer >= cfg::SPAWN_PICKUP_INTERVAL)
      {
         spawn_pickup_timer = 0.0f;
         int x, y;
         if (find_random_free_tile(x, y))
         {
            EntityType t = (std::rand() % cfg::AMMO_PACK_RARITY == 0) ? EntityType::Ammo : EntityType::Grass;
            int id = spawn(t);
            positions[id] = {static_cast<float>(x), static_cast<float>(y)};
         }
      }

      if (spawn_enemy_timer >= cfg::SPAWN_ENEMY_INTERVAL)
      {
         spawn_enemy_timer = 0.0f;
         int x, y;
         if (find_random_free_tile(x, y))
         {
            int id = spawn(EntityType::Enemy);
            positions[id] = {static_cast<float>(x), static_cast<float>(y)};
            int variant = std::rand() % 10;
            if (variant < 6)
            {
               hps[id] = cfg::ENEMY_REGULAR_HP;
               speeds[id] = cfg::ENEMY_REGULAR_SPEED;
            }
            else if (variant < 9)
            {
               hps[id] = cfg::ENEMY_STRONG_HP;
               speeds[id] = cfg::ENEMY_STRONG_SPEED;
            }
            else
            {
               hps[id] = cfg::ENEMY_SWIFT_HP;
               speeds[id] = cfg::ENEMY_SWIFT_SPEED;
            }
         }
      }
   }

   void compute_pathfinding()
   {
      size_t expected = static_cast<size_t>(width * height);
      while (dist_field.get_size() < expected)
         dist_field.push_back(-1);
      for (size_t i = 0; i < expected; ++i)
         dist_field[i] = -1;

      if (player_id < 0 || !entities[player_id].in_use)
         return;
      int px = static_cast<int>(positions[player_id].first + 0.5f);
      int py = static_cast<int>(positions[player_id].second + 0.5f);
      if (!in_bounds(px, py))
         return;

      // BFS (uniform-cost grid) from player
      DynamicArray<int> queue;
      queue.push_back(py * width + px);
      dist_field[py * width + px] = 0;
      size_t head = 0;
      while (head < queue.get_size())
      {
         int idx = queue[head++];
         int x = idx % width;
         int y = idx / width;
         int d = dist_field[idx];

         static const int dxs[] = {0, 0, -1, 1};
         static const int dys[] = {-1, 1, 0, 0};
         for (int k = 0; k < 4; ++k)
         {
            int nx = x + dxs[k];
            int ny = y + dys[k];
            if (!in_bounds(nx, ny)) continue;
            if (is_wall(nx, ny)) continue;
            int nidx = ny * width + nx;
            if (dist_field[nidx] != -1) continue;
            dist_field[nidx] = d + 1;
            queue.push_back(nidx);
         }
      }
   }

   void enemy_ai_system()
   {
      if (player_id < 0 || !entities[player_id].in_use)
         return;
      if (dist_field.get_size() == 0)
         return;

      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         if (entities[i].get_type() != EntityType::Enemy)
            continue;

         int ex = static_cast<int>(positions[i].first + 0.5f);
         int ey = static_cast<int>(positions[i].second + 0.5f);
         if (!in_bounds(ex, ey))
            continue;

         int my_d = dist_field[ey * width + ex];
         if (my_d <= 0)
         {
            velocities[i].first = 0.0f;
            velocities[i].second = 0.0f;
            continue;
         }

         // Pick neighbor with smallest distance to player
         int best_dx = 0, best_dy = 0;
         int best_d = my_d;
         static const int dxs[] = {0, 0, -1, 1};
         static const int dys[] = {-1, 1, 0, 0};
         for (int k = 0; k < 4; ++k)
         {
            int nx = ex + dxs[k];
            int ny = ey + dys[k];
            if (!in_bounds(nx, ny)) continue;
            int nd = dist_field[ny * width + nx];
            if (nd < 0) continue;
            if (nd < best_d)
            {
               best_d = nd;
               best_dx = dxs[k];
               best_dy = dys[k];
            }
         }

         velocities[i].first = best_dx * speeds[i];
         velocities[i].second = best_dy * speeds[i];
      }
   }

   void speed_limit_system()
   {
      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         EntityType t = entities[i].get_type();
         if (t == EntityType::Bullet || t == EntityType::Wall)
            continue;
         if (velocities[i].first > cfg::PLAYER_MAX_SPEED)
            velocities[i].first = cfg::PLAYER_MAX_SPEED;
         else if (velocities[i].first < -cfg::PLAYER_MAX_SPEED)
            velocities[i].first = -cfg::PLAYER_MAX_SPEED;
         if (velocities[i].second > cfg::PLAYER_MAX_SPEED)
            velocities[i].second = cfg::PLAYER_MAX_SPEED;
         else if (velocities[i].second < -cfg::PLAYER_MAX_SPEED)
            velocities[i].second = -cfg::PLAYER_MAX_SPEED;
      }
   }

   void movement_system(float dt)
   {
      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         EntityType t = entities[i].get_type();
         if (t == EntityType::Wall || t == EntityType::Grass ||
             t == EntityType::Ammo || t == EntityType::Bullet)
            continue;

         float new_x = positions[i].first + velocities[i].first * dt;
         if (is_wall_at(new_x, positions[i].second))
            velocities[i].first = 0.0f;
         else
            positions[i].first = new_x;

         float new_y = positions[i].second + velocities[i].second * dt;
         if (is_wall_at(positions[i].first, new_y))
            velocities[i].second = 0.0f;
         else
            positions[i].second = new_y;
      }
   }

   void pickup_system()
   {
      if (player_id < 0 || !entities[player_id].in_use)
         return;
      int px = static_cast<int>(positions[player_id].first + 0.5f);
      int py = static_cast<int>(positions[player_id].second + 0.5f);
      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         EntityType t = entities[i].get_type();
         if (t != EntityType::Grass && t != EntityType::Ammo)
            continue;
         int ex = static_cast<int>(positions[i].first + 0.5f);
         int ey = static_cast<int>(positions[i].second + 0.5f);
         if (ex == px && ey == py)
         {
            if (t == EntityType::Grass)
               player_ammo += cfg::GRASS_AMMO_MIN + std::rand() % cfg::GRASS_AMMO_RANGE;
            else
               player_ammo += cfg::AMMO_PACK_VALUE;
            if (player_ammo > max_ammo)
               player_ammo = max_ammo;
            despawn(static_cast<int>(i));
         }
      }
   }

   void bullet_system(float dt)
   {
      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         if (entities[i].get_type() != EntityType::Bullet)
            continue;

         float old_x = positions[i].first;
         float old_y = positions[i].second;
         float new_x = old_x + velocities[i].first * dt;
         float new_y = old_y + velocities[i].second * dt;

         int from_tx = static_cast<int>(old_x + 0.5f);
         int from_ty = static_cast<int>(old_y + 0.5f);
         int to_tx = static_cast<int>(new_x + 0.5f);
         int to_ty = static_cast<int>(new_y + 0.5f);

         int sx = (to_tx > from_tx) ? 1 : (to_tx < from_tx ? -1 : 0);
         int sy = (to_ty > from_ty) ? 1 : (to_ty < from_ty ? -1 : 0);

         int cx = from_tx, cy = from_ty;
         bool hit = false;
         while (true)
         {
            if (is_wall(cx, cy))
            {
               despawn(static_cast<int>(i));
               hit = true;
               break;
            }
            for (size_t j = 0; j < entities.get_size(); ++j)
            {
               if (!entities[j].in_use)
                  continue;
               if (entities[j].get_type() != EntityType::Enemy)
                  continue;
               int ex = static_cast<int>(positions[j].first + 0.5f);
               int ey = static_cast<int>(positions[j].second + 0.5f);
               if (ex == cx && ey == cy)
               {
                  despawn(static_cast<int>(i));
                  hps[j]--;
                  HitEffect effect;
                  effect.x = cx;
                  effect.y = cy;
                  if (hps[j] <= 0)
                  {
                     despawn(static_cast<int>(j));
                     score += cfg::SCORE_PER_KILL;
                     effect.time = cfg::HIT_EFFECT_KILL_TIME;
                  }
                  else
                  {
                     effect.time = cfg::HIT_EFFECT_DAMAGE_TIME;
                  }
                  hit_effects.push_back(effect);
                  hit = true;
                  break;
               }
            }
            if (hit)
               break;
            if (cx == to_tx && cy == to_ty)
               break;
            cx += sx;
            cy += sy;
         }

         if (!hit)
         {
            positions[i].first = new_x;
            positions[i].second = new_y;
         }
      }
   }

   void enemy_damage_system(float dt)
   {
      if (invincibility_time > 0.0f)
         invincibility_time -= dt;

      if (player_id < 0 || !entities[player_id].in_use)
         return;
      if (invincibility_time > 0.0f)
         return;

      int px = static_cast<int>(positions[player_id].first + 0.5f);
      int py = static_cast<int>(positions[player_id].second + 0.5f);
      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         if (entities[i].get_type() != EntityType::Enemy)
            continue;
         int ex = static_cast<int>(positions[i].first + 0.5f);
         int ey = static_cast<int>(positions[i].second + 0.5f);
         if (ex == px && ey == py)
         {
            player_health--;
            invincibility_time = cfg::PLAYER_INVINCIBILITY;
            if (player_health <= 0)
            {
               player_health = 0;
               game_state = GameState::GameOver;
            }
            break;
         }
      }
   }

   void effects_system(float dt)
   {
      size_t i = 0;
      while (i < hit_effects.get_size())
      {
         hit_effects[i].time -= dt;
         if (hit_effects[i].time <= 0.0f)
         {
            hit_effects[i] = hit_effects[hit_effects.get_size() - 1];
            hit_effects.pop_back();
         }
         else
         {
            ++i;
         }
      }
   }

   void friction_system(float dt)
   {
      float decay = 1.0f - cfg::DAMPING * dt;
      if (decay < 0.0f)
         decay = 0.0f;

      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         EntityType t = entities[i].get_type();
         if (t == EntityType::Wall || t == EntityType::Bullet)
            continue;
         velocities[i].first *= decay;
         velocities[i].second *= decay;
         if (velocities[i].first > -0.01f && velocities[i].first < 0.01f)
            velocities[i].first = 0.0f;
         if (velocities[i].second > -0.01f && velocities[i].second < 0.01f)
            velocities[i].second = 0.0f;
      }
   }

public:
   World(size_t initial_reserve = 8)
       : entities(initial_reserve),
         free_ids(initial_reserve),
         positions(initial_reserve),
         velocities(initial_reserve) {}

   int spawn(EntityType type)
   {
      int id;
      if (free_ids.get_size() > 0)
      {
         id = free_ids[free_ids.get_size() - 1];
         free_ids.pop_back();
         entities[id] = Entity(type, id);
         positions[id] = {0.0f, 0.0f};
         velocities[id] = {0.0f, 0.0f};
         hps[id] = 1;
         speeds[id] = 0.0f;
      }
      else
      {
         id = static_cast<int>(entities.get_size());
         entities.push_back(Entity(type, id));
         positions.push_back({0.0f, 0.0f});
         velocities.push_back({0.0f, 0.0f});
         hps.push_back(1);
         speeds.push_back(0.0f);
      }
      entities[id].in_use = true;
      return id;
   }

   void despawn(int id)
   {
      if (id < 0 || static_cast<size_t>(id) >= entities.get_size())
         throw std::out_of_range("Invalid entity id");
      if (!entities[id].in_use)
         throw std::runtime_error("Entity not in use (double despawn?)");
      entities[id].in_use = false;
      free_ids.push_back(id);
   }

   void init()
   {
      init_walls();
      generate_terrain();
      init_player();
      init_trees();
   }

   void reset()
   {
      entities.clear();
      free_ids.clear();
      positions.clear();
      velocities.clear();
      hps.clear();
      speeds.clear();
      terrain.clear();
      hit_effects.clear();
      dist_field.clear();

      player_id = -1;
      player_ammo = cfg::PLAYER_STARTING_AMMO;
      player_health = cfg::PLAYER_MAX_HEALTH;
      score = 0;
      invincibility_time = 0.0f;
      player_facing = Direction::Right;
      spawn_pickup_timer = 0.0f;
      spawn_enemy_timer = 0.0f;
      pathfinding_timer = 0.0f;
      game_state = GameState::Playing;

      init();
   }

   void handle_input(Key key)
   {
      if (game_state == GameState::GameOver)
      {
         if (key == Key::Restart)
            reset();
         return;
      }

      if (player_id < 0)
         return;
      switch (key)
      {
         case Key::Up:
            velocities[player_id].second -= cfg::PLAYER_IMPULSE;
            player_facing = Direction::Up;
            break;
         case Key::Down:
            velocities[player_id].second += cfg::PLAYER_IMPULSE;
            player_facing = Direction::Down;
            break;
         case Key::Left:
            velocities[player_id].first -= cfg::PLAYER_IMPULSE;
            player_facing = Direction::Left;
            break;
         case Key::Right:
            velocities[player_id].first += cfg::PLAYER_IMPULSE;
            player_facing = Direction::Right;
            break;
         case Key::Shoot:
            shoot();
            break;
         default:
            break;
      }
   }

   void update(float dt)
   {
      if (game_state != GameState::Playing)
         return;
      pathfinding_timer += dt;
      if (pathfinding_timer >= cfg::PATHFINDING_INTERVAL || dist_field.get_size() == 0)
      {
         pathfinding_timer = 0.0f;
         compute_pathfinding();
      }
      spawn_system(dt);
      enemy_ai_system();
      speed_limit_system();
      movement_system(dt);
      bullet_system(dt);
      pickup_system();
      enemy_damage_system(dt);
      effects_system(dt);
      friction_system(dt);
   }

   void put_cell(int x, int y, char ch, int color)
   {
      if (x < 0 || x >= width || y < 0 || y >= height)
         return;
      current_cells[y * width + x] = Cell{ch, static_cast<signed char>(color)};
   }

   void put_text(int x, int y, const std::string &text, int color)
   {
      for (size_t k = 0; k < text.size(); ++k)
         put_cell(x + static_cast<int>(k), y, text[k], color);
   }

   void build_cells(float fps)
   {
      size_t expected = static_cast<size_t>(width * height);
      while (current_cells.get_size() < expected)
         current_cells.push_back(Cell{});

      // Reset current to blank
      Cell blank;
      for (size_t i = 0; i < expected; ++i)
         current_cells[i] = blank;

      // Terrain layer
      for (int y = 1; y < height; ++y)
      {
         for (int x = 0; x < width; ++x)
         {
            Tile t = terrain[y * width + x];
            switch (t)
            {
               case Tile::Wall:   put_cell(x, y, '#', 6); break;
               case Tile::Water:  put_cell(x, y, '~', 9); break;
               case Tile::Bridge: put_cell(x, y, '_', 10); break;
               case Tile::Tree:   put_cell(x, y, '.', 7); break; // overwritten by entity
               case Tile::Floor:  put_cell(x, y, '.', 7); break;
            }
         }
      }

      // Non-player entities
      for (size_t i = 0; i < entities.get_size(); ++i)
      {
         if (!entities[i].in_use)
            continue;
         EntityType t = entities[i].get_type();
         if (t == EntityType::Wall || t == EntityType::Player)
            continue;
         int ex = static_cast<int>(positions[i].first + 0.5f);
         int ey = static_cast<int>(positions[i].second + 0.5f);

         if (t == EntityType::Tree)
         {
            put_cell(ex,     ey,     '/',  5);
            put_cell(ex + 1, ey,     '\\', 5);
            put_cell(ex,     ey + 1, '/',  5);
            put_cell(ex + 1, ey + 1, '\\', 5);
            put_cell(ex,     ey + 2, '|',  5);
            put_cell(ex + 1, ey + 2, '|',  5);
            continue;
         }

         char c = '?';
         int col = -1;
         switch (t)
         {
            case EntityType::Enemy:
               if (speeds[i] > 3.0f) { c = 's'; col = 3; }
               else if (hps[i] > 1)  { c = 'M'; col = 2; }
               else                          { c = '&'; col = 1; }
               break;
            case EntityType::Bullet:
               if (velocities[i].first > 0.0f)       c = '>';
               else if (velocities[i].first < 0.0f)  c = '<';
               else if (velocities[i].second > 0.0f) c = 'v';
               else                                  c = '^';
               col = 4;
               break;
            case EntityType::Grass:  c = '"'; col = 5; break;
            case EntityType::Ammo:   c = '='; col = 3; break;
            default: break;
         }
         put_cell(ex, ey, c, col);
      }

      // Hit effects
      for (size_t i = 0; i < hit_effects.get_size(); ++i)
         put_cell(hit_effects[i].x, hit_effects[i].y, '*', 3);

      // Player on top
      if (player_id >= 0 && entities[player_id].in_use)
      {
         int px = static_cast<int>(positions[player_id].first + 0.5f);
         int py = static_cast<int>(positions[player_id].second + 0.5f);
         put_cell(px, py, '@', 0);
      }

      // HUD row 0: AMMO  HP: ♥♥♡  SCORE
      std::string ammo_text = "AMMO: " + std::to_string(player_ammo) + "/" + std::to_string(max_ammo);
      put_text(1, 0, ammo_text, 8);
      int hx = 1 + static_cast<int>(ammo_text.size());
      put_text(hx, 0, "  HP: ", 8);
      hx += 6;
      for (int i = 0; i < max_health; ++i)
      {
         if (i < player_health)
            put_cell(hx++, 0, 'H', 1);
         else
            put_cell(hx++, 0, 'h', 7);
      }
      put_text(hx, 0, "  ", 8);
      hx += 2;
      put_text(hx, 0, "SCORE: " + std::to_string(score), 8);

      // FPS top-right
      int fps_int = static_cast<int>(fps + 0.5f);
      std::string fps_text = "FPS: ";
      if (fps_int < 100) fps_text += ' ';
      if (fps_int < 10) fps_text += ' ';
      fps_text += std::to_string(fps_int);
      int fps_x = width - static_cast<int>(fps_text.size());
      if (fps_x < 1) fps_x = 1;
      put_text(fps_x, 0, fps_text, 8);

      // Game over overlay
      if (game_state == GameState::GameOver)
      {
         int cy = height / 2;
         auto place = [&](int y, const std::string &text) {
            int x = (width - static_cast<int>(text.size())) / 2;
            put_text(x, y, text, 8);
         };
         place(cy - 2, "+--------------------------+");
         place(cy - 1, "|        GAME OVER         |");
         place(cy,     "|  SCORE: " + std::to_string(score) + std::string(17 - std::to_string(score).size(), ' ') + "|");
         place(cy + 1, "|   R - restart / Q - quit |");
         place(cy + 2, "+--------------------------+");
      }
   }

   void prepare_frame(float fps)
   {
      build_cells(fps);
   }

   RenderFrame get_frame() const
   {
      RenderFrame f;
      f.width = width;
      f.height = height;
      f.cells = current_cells.get_size() > 0 ? &current_cells[0] : nullptr;
      return f;
   }

   int get_width() const { return width; }
   int get_height() const { return height; }
};

#endif // PROJECT_EINZ_ENGINE_HPP
