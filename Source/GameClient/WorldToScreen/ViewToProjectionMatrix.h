#pragma once

#include <CS2/Classes/VMatrix.h>
#include <CS2/Constants/AspectRatio.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <Utils/Math.h>

template <typename HookContext>
class ViewToProjectionMatrix {
public:
    explicit ViewToProjectionMatrix(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    [[nodiscard]] float getFovScale() const noexcept
    {
        if (matrix())
            return matrix()->m[1][1] / cs2::kDefaultAspectRatio;
        return 1.0f;
    }

    [[nodiscard]] std::optional<float> transformY(float yInViewSpace) const noexcept
    {
        if (matrix())
            return yInViewSpace * matrix()->m[1][1];
        return {};
    }

    [[nodiscard]] float getAspectRatio() const noexcept
    {
        if (matrix() && Math::isFinite(matrix()->m[0][0]) && Math::isFinite(matrix()->m[1][1]) && matrix()->m[0][0] != 0.0f) {
            const float aspectRatio = matrix()->m[1][1] / matrix()->m[0][0];
            if (Math::isFinite(aspectRatio) && aspectRatio > 0.0f)
                return aspectRatio;
        }
        return cs2::kDefaultAspectRatio;
    }

private:
    [[nodiscard]] decltype(auto) matrix() const
    {
        return hookContext.patternSearchResults().template get<ViewToProjectionMatrixPointer>();
    }

    HookContext& hookContext;
};
