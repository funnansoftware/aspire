export module scope.glow;

import std;
import scope.mesh;

// How the glow is drawn, for every painter alike: the glow is a Gaussian blur of the glowing shapes, added to what's
// there. The shapes are drawn into smaller textures, blurred across and then down, and stretched back over the frame.
namespace scope
{
    namespace
    {
        constexpr float GlowSigma = 6.0F;
        // The blur's cut off two sigmas out, so the halo is a little narrower than a whole Gaussian's, and ends
        // sooner.
        constexpr float GlowReach = 2.0F;
        // How many of the smaller texture's pixels the blur's sigma spans: enough that the blurred texture stretches
        // back up smoothly, and few enough that the blur needs only a few taps.
        constexpr float GlowTexelSigma = 3.0F;
        // Drawing into the smaller texture and stretching it back blur a little themselves, by a pixel-wide box and a
        // two-pixel tent: a variance of 1/12 + 1/6 of a pixel squared, which the taps leave out.
        constexpr float StretchVariance = 0.25F;
    }
}

export namespace scope
{
    /// The glow's strength, as an opacity its shapes are drawn with.
    inline constexpr float GlowOpacity = 1.0F;

    /// The glow textures grow in steps this many pixels wide, and never shrink, so dragging a window's edge doesn't
    /// make new ones every frame.
    inline constexpr int GlowTextureStep = 128;

    /// @brief How a frame's glow is drawn: the part of the smaller textures it uses, and the blur.
    struct GlowPlan
    {
        /// The smaller textures' resolution, as a share of the frame's.
        float resolution{};
        /// Texture pixels per logical unit, to draw the glowing shapes at.
        float scale{};
        /// The part of the glow textures used, from their top left, in their pixels. At least 1 by 1.
        int width{};
        int height{};
        /// The blur along each axis.
        std::vector<Tap> taps;
    };

    /// @brief Plans a frame's glow.
    /// @param scale The frame's pixels per logical unit.
    /// @param width The frame's width in pixels.
    /// @param height The frame's height in pixels.
    /// @return The plan: the textures' resolution is chosen so the blur spans a few of their pixels.
    [[nodiscard]] auto PlanGlow(float scale, int width, int height) -> GlowPlan
    {
        const auto sigma = GlowSigma * scale;
        const auto resolution = std::min(1.0F, GlowTexelSigma / sigma);
        const auto texelSigma = sigma * resolution;
        const auto used = [resolution](int x) { return std::max(static_cast<int>(std::ceil(static_cast<float>(x) * resolution)), 1); };

        return {.resolution = resolution,
                .scale = scale * resolution,
                .width = used(width),
                .height = used(height),
                .taps = GaussianTaps(std::sqrt(std::max((texelSigma * texelSigma) - StretchVariance, StretchVariance)), GlowReach)};
    }

    /// @brief Reports how big a glow texture should be to hold a size.
    /// @param used The size the glow uses, in pixels.
    /// @param current The texture's size now, or 0 if there's none.
    /// @return The size, a whole number of `GlowTextureStep`s, never smaller than `current`.
    [[nodiscard]] constexpr auto GlowTextureSize(int used, int current) -> int
    {
        const auto size = std::max(used, current);
        return ((size + GlowTextureStep - 1) / GlowTextureStep) * GlowTextureStep;
    }
}
