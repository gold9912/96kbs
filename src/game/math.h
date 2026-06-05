#pragma once

#include <cmath>

namespace rogue {

struct Vec2 {
    float x;
    float y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return Vec2{a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return Vec2{a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return Vec2{a.x * s, a.y * s}; }
inline Vec2 operator/(Vec2 a, float s) { return Vec2{a.x / s, a.y / s}; }

inline float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float LengthSq(Vec2 v) { return Dot(v, v); }
inline float Length(Vec2 v) { return std::sqrt(LengthSq(v)); }

inline Vec2 NormalizeOr(Vec2 v, Vec2 fallback) {
    const float lenSq = LengthSq(v);
    if (lenSq <= 0.000001f) {
        return fallback;
    }
    return v / std::sqrt(lenSq);
}

inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float Saturate(float v) {
    return Clamp(v, 0.0f, 1.0f);
}

}

