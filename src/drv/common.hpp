#ifndef COMMON_HPP_INCLUDED
#define COMMON_HPP_INCLUDED

#define GLM_DEPTH_ZERO_TO_ONE

#include <cinttypes>
#include <vulkan/vulkan.hpp>
#include <cassert>

using u32 = uint32_t;
using i32 = int32_t;

using f32 = float;
using f64 = double;

using u8 = uint8_t;

template <typename T>
inline T min(const T a, const T b) { return (a < b)? a : b; }

template <typename T>
inline T max(const T a, const T b) { return (a > b)? a : b; }

#endif