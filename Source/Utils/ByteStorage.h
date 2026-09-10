#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <span>
#include <type_traits>

#include <Utils/Optional.h>

namespace byte_storage {
    template <typename T>
    [[nodiscard]] Optional<T> read(std::span<const std::byte> storage, std::size_t offset) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (offset > storage.size() || sizeof(T) > storage.size() - offset)
            return {};

        std::array<std::byte, sizeof(T)> bytes{};
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes[i] = storage[offset + i];
        return std::bit_cast<T>(bytes);
    }

    template <typename T>
    [[nodiscard]] bool write(std::span<std::byte> storage, std::size_t offset, T value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (offset > storage.size() || sizeof(T) > storage.size() - offset)
            return false;

        const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            storage[offset + i] = bytes[i];
        return true;
    }
}
