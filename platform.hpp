#ifndef PROJECT_EINZ_PLATFORM_HPP
#define PROJECT_EINZ_PLATFORM_HPP

// Shared platform-independent ABI: the contract between the engine
// and any platform backend (terminal, web, SDL, ...).
//
// Both the engine (project-einz.cpp) and every backend (terminal.hpp,
// ...) include this header. Nothing here depends on POSIX or any
// specific OS.

#include <iostream>

// ----- DynamicArray -----

template <typename T>
class DynamicArray
{
private:
   T *data;
   size_t size;
   size_t capacity;

   void resize(size_t new_capacity)
   {
      if (new_capacity == 0)
         new_capacity = 1;
      T *new_data = new T[new_capacity];
      std::copy(data, data + size, new_data);
      delete[] data;
      data = new_data;
      capacity = new_capacity;
   }

public:
   DynamicArray() : data(new T[8]), size(0), capacity(8) {}

   DynamicArray(size_t initial_capacity) : data(new T[initial_capacity]), size(0), capacity(initial_capacity) {}

   DynamicArray(const DynamicArray &other) : data(new T[other.capacity]), size(other.size), capacity(other.capacity)
   {
      std::copy(other.data, other.data + other.size, data);
   }

   DynamicArray(DynamicArray &&other) noexcept : data(other.data), size(other.size), capacity(other.capacity)
   {
      other.data = nullptr;
      other.size = 0;
      other.capacity = 0;
   }

   DynamicArray &operator=(DynamicArray other) noexcept
   {
      std::swap(data, other.data);
      std::swap(size, other.size);
      std::swap(capacity, other.capacity);
      return *this;
   }

   ~DynamicArray() { delete[] data; }

   void push_back(const T &value)
   {
      if (size == capacity)
         resize(capacity * 2);
      data[size++] = value;
   }

   void pop_back()
   {
      if (size > 0)
         --size;
   }

   void clear() { size = 0; }

   T &operator[](size_t index)
   {
      if (index >= size)
         throw std::out_of_range("Index out of range");
      return data[index];
   }

   const T &operator[](size_t index) const
   {
      if (index >= size)
         throw std::out_of_range("Index out of range");
      return data[index];
   }

   size_t get_size() const { return size; }

   class Iterator
   {
   private:
      T *ptr;

   public:
      Iterator(T *p) : ptr(p) {}

      T &operator*() { return *ptr; }

      Iterator &operator++()
      {
         ptr++;
         return *this;
      }

      Iterator operator++(int)
      {
         Iterator temp = *this;
         ptr++;
         return temp;
      }

      bool operator!=(const Iterator &other) const { return ptr != other.ptr; }
   };

   Iterator begin() { return Iterator(data); }
   Iterator end() { return Iterator(data + size); }
};

// ----- Input key codes -----

enum class Key
{
   None,
   Up,
   Down,
   Left,
   Right,
   Shoot,
   Restart,
   Quit
};

// ----- Render IR + abstract Renderer/InputSource -----

struct Cell
{
   char ch = ' ';
   signed char color = -1;
};

// Platform-independent snapshot of what should be on screen this frame.
// World produces; Renderer consumes.
struct RenderFrame
{
   int width = 0;
   int height = 0;
   const Cell *cells = nullptr;  // pointer into World's current_cells, size = width*height
};

// Abstract renderer interface. Concrete impls (terminal, web, SDL...)
// translate the cell-grid IR into native draw commands.
class Renderer
{
public:
   virtual ~Renderer() = default;
   virtual void init() = 0;
   virtual void shutdown() = 0;
   virtual void present(const RenderFrame &frame) = 0;
};

// Abstract non-blocking input source. Returns Key::None if no key pressed.
class InputSource
{
public:
   virtual ~InputSource() = default;
   virtual Key poll() = 0;
};

#endif // PROJECT_EINZ_PLATFORM_HPP
