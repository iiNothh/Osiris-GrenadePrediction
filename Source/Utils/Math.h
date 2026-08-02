#pragma once

#include <bit>
#include <cstdint>

namespace Math
{

[[nodiscard]] constexpr bool isFinite(float value) noexcept
{
    return (std::bit_cast<std::uint32_t>(value) & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] constexpr float maximum(float first, float second) noexcept
{
    return first > second ? first : second;
}

}
