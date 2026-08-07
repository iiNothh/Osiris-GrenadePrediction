#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <string_view>

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

        std::uint32_t significand{};
        std::size_t numberOfSignificantDigits{};
        std::size_t firstSignificantDigitIndex{};
        std::size_t numberOfDigitsBeforeDecimalPoint{};
        std::size_t numberOfFractionalDigits{};
        bool hasNonZeroDigit{};
        bool hasFractionalDigit{};
        bool hasStickyDigit{};

        while (*string >= '0' && *string <= '9') {
            addFloatDigit(*string++ - '0', numberOfDigitsBeforeDecimalPoint, significand, numberOfSignificantDigits, firstSignificantDigitIndex, hasNonZeroDigit, hasStickyDigit);
            ++numberOfDigitsBeforeDecimalPoint;
        }

        if (numberOfDigitsBeforeDecimalPoint == 0)
            return false;

        if (*string == '.') {
            ++string;
            while (*string >= '0' && *string <= '9') {
                addFloatDigit(*string++ - '0', numberOfDigitsBeforeDecimalPoint + numberOfFractionalDigits, significand, numberOfSignificantDigits, firstSignificantDigitIndex, hasNonZeroDigit, hasStickyDigit);
                hasFractionalDigit = true;
                ++numberOfFractionalDigits;
            }
            if (!hasFractionalDigit)
                return false;
        }

        int explicitExponent{};
        if (*string == 'e' || *string == 'E') {
            ++string;
            const bool negativeExponent = *string == '-';
            if (negativeExponent || *string == '+')
                ++string;

            bool hasExponentDigit{};
            while (*string >= '0' && *string <= '9') {
                hasExponentDigit = true;
                if (explicitExponent <= 1000)
                    explicitExponent = explicitExponent * 10 + *string - '0';
                ++string;
            }
            if (!hasExponentDigit)
                return false;
            if (negativeExponent)
                explicitExponent = -explicitExponent;
        }

        if (!hasNonZeroDigit) {
            result = negative ? -0.0f : 0.0f;
            return true;
        }
        if (explicitExponent > 1000)
            return false;
        if (explicitExponent < -1000) {
            result = negative ? -0.0f : 0.0f;
            return true;
        }

        if (numberOfSignificantDigits == 10) {
            const auto guardDigit = significand % 10;
            significand /= 10;
            if (guardDigit > 5 || (guardDigit == 5 && (hasStickyDigit || significand % 2 != 0)))
                ++significand;
            numberOfSignificantDigits = 9;
        }

        int decimalExponent = static_cast<int>(numberOfDigitsBeforeDecimalPoint) - static_cast<int>(firstSignificantDigitIndex) - 1 + explicitExponent;
        if (significand == 1'000'000'000) {
            significand = 100'000'000;
            ++decimalExponent;
        }

        const int scale = decimalExponent - static_cast<int>(numberOfSignificantDigits) + 1;
        FloatType parsedValue = static_cast<FloatType>(significand);
        if (scale > 0) {
            for (int i = 0; i < scale; ++i) {
                parsedValue *= 10.0f;
                if (!isFinite(parsedValue))
                    return false;
            }
        } else {
            for (int i = 0; i > scale; --i)
                parsedValue /= 10.0f;
        }

        parsedValue = negative ? -parsedValue : parsedValue;
        if (!isFinite(parsedValue))
            return false;
        result = parsedValue;
        return true;
    }

private:
    [[nodiscard]] static bool isFinite(std::floating_point auto value) noexcept
    {
        return value == value && value >= -(std::numeric_limits<decltype(value)>::max)() && value <= (std::numeric_limits<decltype(value)>::max)();
    }

    static void addFloatDigit(std::uint32_t digit, std::size_t digitIndex, std::uint32_t& significand, std::size_t& numberOfSignificantDigits, std::size_t& firstSignificantDigitIndex, bool& hasNonZeroDigit, bool& hasStickyDigit) noexcept
    {
        if (!hasNonZeroDigit) {
            if (digit == 0)
                return;
            hasNonZeroDigit = true;
            firstSignificantDigitIndex = digitIndex;
        }

        if (numberOfSignificantDigits < 10) {
            significand = significand * 10 + digit;
            ++numberOfSignificantDigits;
        } else if (digit != 0) {
            hasStickyDigit = true;
        }
    }

    const char* string;
};
