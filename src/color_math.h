#pragma once

#include <cstdint>

namespace typestatus {

struct RgbColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend constexpr bool operator==(
        const RgbColor& left,
        const RgbColor& right) noexcept = default;
};

[[nodiscard]] constexpr int CalculateLuminance(
    const RgbColor& color) noexcept {
    return (color.red * 2126 +
            color.green * 7152 +
            color.blue * 722) /
        10000;
}

[[nodiscard]] constexpr RgbColor TintByLuminance(
    const RgbColor& source,
    const RgbColor& target) noexcept {
    const int luminance = CalculateLuminance(source);
    const auto tint_channel = [luminance](std::uint8_t target_channel) {
        const int distance_to_white = 255 - target_channel;
        return static_cast<std::uint8_t>(
            target_channel + (distance_to_white * luminance + 127) / 255);
    };

    return {
        tint_channel(target.red),
        tint_channel(target.green),
        tint_channel(target.blue),
    };
}

}  // namespace typestatus
