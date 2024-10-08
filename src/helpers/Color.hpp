#pragma once

#include <cstdint>

class CColor {
  public:
    CColor();
    CColor(float r, float g, float b, float a);
    CColor(uint64_t);

    float    r = 0, g = 0, b = 0, a = 1.f;

    uint32_t getAsHex() const;

    CColor   operator-(const CColor& c2) const {
        return CColor(r - c2.r, g - c2.g, b - c2.b, a - c2.a);
    }

    CColor operator+(const CColor& c2) const {
        return CColor(r + c2.r, g + c2.g, b + c2.b, a + c2.a);
    }

    CColor operator*(const float& v) const {
        return CColor(r * v, g * v, b * v, a * v);
    }

    bool operator==(const CColor& c2) const {
        return r == c2.r && g == c2.g && b == c2.b && a == c2.a;
    }

    CColor stripA() const {
        return {r, g, b, 1};
    }
};
