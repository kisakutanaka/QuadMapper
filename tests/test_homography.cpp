#include <cmath>
#include <cstdio>

#include "core/Homography.h"

static int failures = 0;

static void expectNear(Vec2 a, Vec2 b)
{
    if (std::fabs(a.x - b.x) > 1e-4f || std::fabs(a.y - b.y) > 1e-4f) {
        std::printf("FAIL: (%f, %f) != (%f, %f)\n", a.x, a.y, b.x, b.y);
        failures++;
    }
}

int main()
{
    const std::array<Vec2, 4> unit{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    const std::array<Vec2, 4> quads[] = {
        unit,
        {{{0.1f, 0.2f}, {0.9f, 0.1f}, {0.8f, 0.95f}, {0.15f, 0.7f}}},
        {{{0.2f, 0.2f}, {0.7f, 0.2f}, {0.8f, 0.6f}, {0.3f, 0.6f}}},  // parallelogram
    };
    for (const auto& q : quads) {
        const Mat3 h = squareToQuad(q);
        const Mat3 inv = inverse(h);
        for (int i = 0; i < 4; i++) {
            expectNear(transform(h, unit[i]), q[i]);
            expectNear(transform(inv, q[i]), unit[i]);
        }
        const Vec2 p{0.37f, 0.61f};
        expectNear(transform(inv, transform(h, p)), p);
    }
    std::printf(failures ? "%d failures\n" : "ok\n", failures);
    return failures != 0;
}
