#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <Utils/DecimalFloatParser.h>

class StringParser {
public:
    explicit StringParser(const char* string) noexcept
        : string{ string }
    {
    }

    [[nodiscard]] std::string_view getLine(char delimiter) noexcept
    {
        const auto begin = string;
        std::size_t length = 0;
        while (*string != '\0' && *string++ != delimiter)
            ++length;
        return { begin, length };
    }

    [[nodiscard]] char getChar() noexcept
    {
        if (*string != '\0')
            return *string++;
        return '\0';
    }

    void skipWhitespace() noexcept
    {
        while (isWhitespace(*string))
            ++string;
    }

    template <std::integral IntegralType>
    bool parseInt(IntegralType& result) noexcept
    {
        static_assert(std::is_unsigned_v<IntegralType>);

        IntegralType parsedInteger{};
        bool parseSuccessful = false;

        std::size_t currentDigit = 0;
        while (*string >= '0' && *string <= '9') {
            if (currentDigit < std::numeric_limits<IntegralType>::digits10) {
                parsedInteger *= 10;
                parsedInteger += (*string - '0');
                ++currentDigit;
                parseSuccessful = true;
            } else {
                parseSuccessful = false;
            }

            ++string;
        }

        if (parseSuccessful)
            result = parsedInteger;

        return parseSuccessful;
    }

    template <std::floating_point FloatType>
    bool parseFloat(FloatType& result) noexcept
    {
        const bool negative = *string == '-';
        if (negative)
            ++string;
        struct FloatCursor {
            const char*& string;

            [[nodiscard]] char current() const noexcept
            {
                return *string;
            }

            void advance() noexcept
            {
                ++string;
            }
        } cursor{ string };

        return decimal_float_parser::parseFloat(cursor, negative, result, [](FloatType value) { return isFinite(value); }, [&cursor] { return cursor.current() == '\0' || isWhitespace(cursor.current()); });
    }

private:
    [[nodiscard]] static constexpr bool isWhitespace(char c) noexcept
    {
        switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] static bool isFinite(std::floating_point auto value) noexcept
    {
        return value == value && value >= -(std::numeric_limits<decltype(value)>::max)() && value <= (std::numeric_limits<decltype(value)>::max)();
    }

    const char* string;
};
