#pragma once

#include <cstddef>
#include <cstdint>

namespace decimal_float_parser
{

namespace detail
{

inline void addFloatDigit(std::uint32_t digit, std::size_t digitIndex, std::uint64_t& significand, std::size_t& numberOfSignificantDigits, std::size_t& firstSignificantDigitIndex, bool& hasNonZeroDigit, bool& hasStickyDigit) noexcept
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

}

template <typename Cursor, typename FloatType, typename IsFinite, typename IsValueTerminator>
[[nodiscard]] bool parseFloat(Cursor& cursor, bool negative, FloatType& result, IsFinite isFinite, IsValueTerminator isValueTerminator) noexcept
{
    std::uint64_t significand{};
    std::size_t numberOfSignificantDigits{};
    std::size_t firstSignificantDigitIndex{};
    std::size_t numberOfDigitsBeforeDecimalPoint{};
    std::size_t numberOfFractionalDigits{};
    bool hasNonZeroDigit{};
    bool hasFractionalDigit{};
    bool hasStickyDigit{};

    while (cursor.current() >= '0' && cursor.current() <= '9') {
        detail::addFloatDigit(cursor.current() - '0', numberOfDigitsBeforeDecimalPoint, significand, numberOfSignificantDigits, firstSignificantDigitIndex, hasNonZeroDigit, hasStickyDigit);
        cursor.advance();
        ++numberOfDigitsBeforeDecimalPoint;
    }

    if (numberOfDigitsBeforeDecimalPoint == 0)
        return false;

    if (cursor.current() == '.') {
        cursor.advance();
        while (cursor.current() >= '0' && cursor.current() <= '9') {
            detail::addFloatDigit(cursor.current() - '0', numberOfDigitsBeforeDecimalPoint + numberOfFractionalDigits, significand, numberOfSignificantDigits, firstSignificantDigitIndex, hasNonZeroDigit, hasStickyDigit);
            hasFractionalDigit = true;
            cursor.advance();
            ++numberOfFractionalDigits;
        }
        if (!hasFractionalDigit)
            return false;
    }

    int explicitExponent{};
    if (cursor.current() == 'e' || cursor.current() == 'E') {
        cursor.advance();
        const bool negativeExponent = cursor.current() == '-';
        if (negativeExponent || cursor.current() == '+')
            cursor.advance();

        bool hasExponentDigit{};
        while (cursor.current() >= '0' && cursor.current() <= '9') {
            hasExponentDigit = true;
            if (explicitExponent <= 1000)
                explicitExponent = explicitExponent * 10 + cursor.current() - '0';
            cursor.advance();
        }
        if (!hasExponentDigit)
            return false;
        if (negativeExponent)
            explicitExponent = -explicitExponent;
    }

    if (!isValueTerminator())
        return false;
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

    result = negative ? -parsedValue : parsedValue;
    return isFinite(result);
}

}
