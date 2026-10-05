#include "core/Homography.h"

// Heckbert, "Fundamentals of Texture Mapping and Image Warping" (1989), 2.2.3
Mat3 squareToQuad(const std::array<Vec2, 4>& q)
{
    const float sx = q[0].x - q[1].x + q[2].x - q[3].x;
    const float sy = q[0].y - q[1].y + q[2].y - q[3].y;
    const float dx1 = q[1].x - q[2].x, dx2 = q[3].x - q[2].x;
    const float dy1 = q[1].y - q[2].y, dy2 = q[3].y - q[2].y;
    const float den = dx1 * dy2 - dx2 * dy1;
    const float g = (sx * dy2 - dx2 * sy) / den;
    const float h = (dx1 * sy - sx * dy1) / den;
    return {q[1].x - q[0].x + g * q[1].x, q[3].x - q[0].x + h * q[3].x, q[0].x,
            q[1].y - q[0].y + g * q[1].y, q[3].y - q[0].y + h * q[3].y, q[0].y,
            g, h, 1.0f};
}

Mat3 inverse(const Mat3& m)
{
    const float a = m[4] * m[8] - m[5] * m[7];
    const float b = m[5] * m[6] - m[3] * m[8];
    const float c = m[3] * m[7] - m[4] * m[6];
    const float inv = 1.0f / (m[0] * a + m[1] * b + m[2] * c);
    return {a * inv, (m[2] * m[7] - m[1] * m[8]) * inv, (m[1] * m[5] - m[2] * m[4]) * inv,
            b * inv, (m[0] * m[8] - m[2] * m[6]) * inv, (m[2] * m[3] - m[0] * m[5]) * inv,
            c * inv, (m[1] * m[6] - m[0] * m[7]) * inv, (m[0] * m[4] - m[1] * m[3]) * inv};
}

Vec2 transform(const Mat3& m, Vec2 p)
{
    const float w = m[6] * p.x + m[7] * p.y + m[8];
    return {(m[0] * p.x + m[1] * p.y + m[2]) / w, (m[3] * p.x + m[4] * p.y + m[5]) / w};
}
