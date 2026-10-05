#pragma once
#include <array>

struct Vec2 {
    float x, y;
};

// Row-major 3x3 matrix.
using Mat3 = std::array<float, 9>;

// Maps the unit square (0,0),(1,0),(1,1),(0,1) to quad q[0..3].
Mat3 squareToQuad(const std::array<Vec2, 4>& q);
Mat3 inverse(const Mat3& m);
Vec2 transform(const Mat3& m, Vec2 p);
