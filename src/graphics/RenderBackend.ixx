export module aspire.graphics:renderbackend;

import :color;
import :drawlist;

export namespace aspire::graphics
{
    /// @brief Draws finished frames. Implemented outside `aspire.graphics`, for example on SDL's renderer.
    class RenderBackend
    {
    public:
        RenderBackend() = default;
        virtual ~RenderBackend() = default;

        RenderBackend(const RenderBackend&) = delete;
        auto operator=(const RenderBackend&) -> RenderBackend& = delete;

        RenderBackend(RenderBackend&&) noexcept = delete;
        auto operator=(RenderBackend&&) noexcept -> RenderBackend& = delete;

        /// @brief Draws one frame: clears the target, draws every item in list order with its clip, and presents.
        /// @param list The frame's items, already sorted.
        /// @param clear The color to clear to first.
        /// @return `false` if drawing failed. The backend reports why itself.
        virtual auto submit(const DrawList& list, Color clear) -> bool = 0;
    };
}
