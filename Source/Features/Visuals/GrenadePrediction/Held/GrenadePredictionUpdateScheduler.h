#pragma once

struct GrenadePredictionUpdateScheduler {
    static constexpr float updateInterval{1.0f / 90.0f};

    [[nodiscard]] bool shouldUpdate(bool force, bool hasFrametime, float frametime) noexcept
    {
        if (force || !initialized) {
            initialized = true;
            accumulatedTime = 0.0f;
            return true;
        }
        if (!hasFrametime || !(frametime > 0.0f) || frametime > 0.25f) {
            accumulatedTime = 0.0f;
            return true;
        }
        if (frametime >= updateInterval) {
            accumulatedTime = 0.0f;
            return true;
        }
        accumulatedTime += frametime;
        if (accumulatedTime < updateInterval)
            return false;
        accumulatedTime -= updateInterval;
        return true;
    }

    void reset() noexcept { initialized = false; accumulatedTime = 0.0f; }

    bool initialized{};
    float accumulatedTime{};
};
