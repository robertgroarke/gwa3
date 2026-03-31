#pragma once

// GWA3 container and basic types matching GWCA memory layouts.
// All sizes are for x86 (32-bit) builds.

#include <cstdint>

namespace GWA3 {

// GWCA Array<T> — { buffer, capacity, size, growth } = 16 bytes
template<typename T>
struct GWArray {
    T*       buffer;
    uint32_t capacity;
    uint32_t size;
    uint32_t growth;
};
static_assert(sizeof(GWArray<void*>) == 16, "GWArray must be 16 bytes");

// GWCA TLink<T> — doubly-linked list node = 8 bytes
struct TLink {
    void* prev;
    void* next;
};
static_assert(sizeof(TLink) == 8, "TLink must be 8 bytes");

// GWCA TList<T> — linked list header = 12 bytes
struct TList {
    TLink link;
    uint32_t count;
};
static_assert(sizeof(TList) == 12, "TList must be 12 bytes");

// 3D float vector
struct Vec3f {
    float x, y, z;
};
static_assert(sizeof(Vec3f) == 12, "Vec3f must be 12 bytes");

// 2D float vector
struct Vec2f {
    float x, y;
};
static_assert(sizeof(Vec2f) == 8, "Vec2f must be 8 bytes");

// Game position
struct GamePos {
    float    x;
    float    y;
    uint32_t plane;
};
static_assert(sizeof(GamePos) == 12, "GamePos must be 12 bytes");

} // namespace GWA3
