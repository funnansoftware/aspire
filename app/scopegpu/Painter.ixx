module;

#include <SDL3/SDL.h>

export module scopegpu.painter;

import std;
import aspire.core;
import scope.glow;
import scope.mesh;
import scope.picture;
import scope.world;
import scopegpu.frame;
import scopegpu.glyphs;
import scopegpu.gpu;
import scopegpu.shadercode;
import scopegpu.shaders;
import scopegpu.shelves;

// The scope's picture, drawn with SDL's GPU API and scopegpu's own shaders.
namespace scopegpu
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        // Buffers grow to the next power of two that holds a frame, from at least this many bytes.
        constexpr std::uint32_t SmallestBuffer = 64U * 1024U;
        // Each kind of data starts at this alignment in the buffers, which suits every copy the backends make.
        constexpr std::uint32_t RegionAlignment = 512;

        // The glow's textures hold 8-bit channels, as scope's RGBA32 targets do on SDL's renderer.
        constexpr SDL_GPUTextureFormat GlowFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        constexpr SDL_GPUTextureFormat OffscreenFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        constexpr std::uint32_t BytesPerPixel = 4;
        constexpr std::uint32_t FullscreenVertices = 3;

        // The glow's textures: its full and fainter channels, and each blurred across on the way.
        constexpr std::size_t FullGlow = 0;
        constexpr std::size_t FainterGlow = 1;
        constexpr std::size_t FullAcross = 2;
        constexpr std::size_t FainterAcross = 3;

        // SDL_BLENDMODE_BLEND, as SDL's renderer draws shapes and text: straight alpha over what's there.
        constexpr SDL_GPUColorTargetBlendState Blend{.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                                     .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                     .color_blend_op = SDL_GPU_BLENDOP_ADD,
                                                     .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                                                     .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                                     .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                                                     .color_write_mask = 0,
                                                     .enable_blend = true,
                                                     .enable_color_write_mask = false,
                                                     .padding1 = 0,
                                                     .padding2 = 0};

        // Passes that work out every pixel themselves replace what's there.
        constexpr SDL_GPUColorTargetBlendState Replace{.src_color_blendfactor = SDL_GPU_BLENDFACTOR_INVALID,
                                                       .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_INVALID,
                                                       .color_blend_op = SDL_GPU_BLENDOP_INVALID,
                                                       .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_INVALID,
                                                       .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_INVALID,
                                                       .alpha_blend_op = SDL_GPU_BLENDOP_INVALID,
                                                       .color_write_mask = 0,
                                                       .enable_blend = false,
                                                       .enable_color_write_mask = false,
                                                       .padding1 = 0,
                                                       .padding2 = 0};

        // Shapes' corners: a position in pixels and a straight-alpha color, laid out as scope's mesh holds them.
        static_assert(sizeof(scope::Vertex) == sizeof(scope::Vec2) + sizeof(scope::Tint));
        constexpr std::array ShapeAttributes{
            SDL_GPUVertexAttribute{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0},
            SDL_GPUVertexAttribute{.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = sizeof(scope::Vec2)},
        };

        // Glyphs' corners: as shapes', and where they are in the atlas.
        static_assert(sizeof(GlyphVertex) == sizeof(scope::Vec2) + sizeof(scope::Tint) + sizeof(scope::Vec2));
        constexpr std::array TextAttributes{
            SDL_GPUVertexAttribute{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0},
            SDL_GPUVertexAttribute{.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = sizeof(scope::Vec2)},
            SDL_GPUVertexAttribute{
                .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = sizeof(scope::Vec2) + sizeof(scope::Tint)},
        };

        // The marks' mesh's corners: a position in pixels, an alpha and, for a line's end, how far it reaches; and which
        // part of a mark each is.
        constexpr std::uint32_t Vec4Bytes = 16;
        static_assert(sizeof(MarkVertex) == Vec4Bytes + sizeof(std::uint32_t));
        constexpr std::array MarkAttributes{
            SDL_GPUVertexAttribute{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 0},
            SDL_GPUVertexAttribute{.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT, .offset = Vec4Bytes},
        };

        // Each mark's instance: where it is with its heading and its route's clock, its color, its leader and tick, its
        // route and how fast it goes, and its flags.
        constexpr std::uint32_t MarkInstanceVectors = 4;
        static_assert(sizeof(MarkInstance) == (MarkInstanceVectors * Vec4Bytes) + sizeof(float) + sizeof(std::uint32_t));
        constexpr auto MarkInstanceAttributes = []
        {
            std::array<SDL_GPUVertexAttribute, MarkInstanceVectors + 2> x{};
            const auto first = static_cast<std::uint32_t>(std::size(MarkAttributes));

            for (std::uint32_t i = 0; i < MarkInstanceVectors; ++i)
            {
                x.at(i) = {.location = first + i, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = i * Vec4Bytes};
            }

            constexpr auto pace = MarkInstanceVectors * Vec4Bytes;
            x.at(MarkInstanceVectors) = {
                .location = first + MarkInstanceVectors, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, .offset = pace};
            x.at(MarkInstanceVectors + 1) = {.location = first + MarkInstanceVectors + 1,
                                             .buffer_slot = 1,
                                             .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
                                             .offset = pace + static_cast<std::uint32_t>(sizeof(float))};
            return x;
        }();

        // A glyph's quad, which each of a data block's glyphs is an instance of: its corners from top left to bottom
        // right, in two triangles as `Glyphs` draws text's quads.
        constexpr std::array<scope::Vec2, 6> QuadCorners{{
            {.x = 0.0F, .y = 0.0F},
            {.x = 1.0F, .y = 0.0F},
            {.x = 1.0F, .y = 1.0F},
            {.x = 0.0F, .y = 0.0F},
            {.x = 1.0F, .y = 1.0F},
            {.x = 0.0F, .y = 1.0F},
        }};
        constexpr std::array BlockCornerAttributes{
            SDL_GPUVertexAttribute{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0},
        };

        // Each data block's glyph's instance: where its block starts and its column and line, its quad, where it is in
        // the atlas, and its color.
        constexpr std::uint32_t BlockGlyphVectors = 4;
        static_assert(sizeof(BlockGlyph) == BlockGlyphVectors * Vec4Bytes);
        constexpr auto BlockGlyphAttributes = []
        {
            std::array<SDL_GPUVertexAttribute, BlockGlyphVectors> x{};

            for (std::uint32_t i = 0; i < BlockGlyphVectors; ++i)
            {
                x.at(i) = {.location = static_cast<std::uint32_t>(std::size(BlockCornerAttributes)) + i,
                           .buffer_slot = 1,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                           .offset = i * Vec4Bytes};
            }

            return x;
        }();

        // The other shaders' uniform blocks, laid out as std140 lays them out.
        struct TargetUniforms
        {
            std::array<float, 4> projection{};
        };

        // mark.vert's, which starts as shape.vert's does, so shapes drawn after marks find their projection.
        struct MarkUniforms
        {
            std::array<float, 4> projection{};
            // Target pixels per logical unit, and 1 if the scale ring clips the marks.
            std::array<float, 4> view{};
            // Where ownship is in logical units, logical units per nautical mile, and scope seconds between dots.
            std::array<float, 4> world{};
            // The scale ring in target pixels: its center and radius.
            std::array<float, 4> clip{};
        };

        // block.vert's, which starts as text.vert's does.
        struct BlockUniforms
        {
            std::array<float, 4> projection{};
            // How the glyphs step from where their blocks start: by a column, by a line, and up by the font's ascent.
            std::array<float, 4> metrics{};
        };

        struct BackdropUniforms
        {
            std::array<float, 4> inner{};
            std::array<float, 4> outer{};
            std::array<float, 4> disc{};
            std::array<float, 4> glow{};
            std::array<float, 4> rect{};
            std::array<float, 4> background{};
        };

        // Maps a target's pixels to clip space as SDL's renderer does: (0, 0) at the top left, no half-pixel offset.
        auto ProjectionOf(Extent target) -> TargetUniforms
        {
            // Clip space runs from -1 to 1 across the target.
            constexpr float span = 2.0F;
            return {.projection = {span / static_cast<float>(target.width), -span / static_cast<float>(target.height), -1.0F, 1.0F}};
        }

        auto Rgba(scope::Color x) -> std::array<float, 4>
        {
            const auto tint = scope::TintOf(x, 1.0F);
            return {tint.r, tint.g, tint.b, tint.a};
        }

        auto Milliseconds(Clock::duration x) -> double
        {
            return std::chrono::duration<double, std::milli>{x}.count();
        }

        auto BytesOf(const auto& x) -> std::uint32_t
        {
            return static_cast<std::uint32_t>(std::size(x) * sizeof(*std::data(x)));
        }

        // A target cleared to a color, or with `clear` empty, one whose every pixel the pass replaces.
        auto ColorTarget(SDL_GPUTexture* texture, std::optional<SDL_FColor> clear, bool cycle) -> SDL_GPUColorTargetInfo
        {
            return {.texture = texture,
                    .mip_level = 0,
                    .layer_or_depth_plane = 0,
                    .clear_color = clear.value_or(SDL_FColor{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}),
                    .load_op = clear.has_value() ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE,
                    .store_op = SDL_GPU_STOREOP_STORE,
                    .resolve_texture = nullptr,
                    .resolve_mip_level = 0,
                    .resolve_layer = 0,
                    .cycle = cycle,
                    .cycle_resolve_texture = false,
                    .padding1 = 0,
                    .padding2 = 0};
        }

        // Where a mesh's vertices and indices start in the frame's buffers.
        struct MeshBuffers
        {
            SDL_GPUBufferBinding vertices{};
            SDL_GPUBufferBinding indices{};
        };

        // Binds a pipeline and the mesh it draws from.
        auto BindMesh(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline, const MeshBuffers& mesh) -> void
        {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            SDL_BindGPUVertexBuffers(pass, 0, &mesh.vertices, 1);
            SDL_BindGPUIndexBuffer(pass, &mesh.indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        }

        // What a pipeline draws with: its shaders, its vertices and, if it draws instances, theirs, and its targets.
        struct PipelineInfo
        {
            ShaderName vertex{};
            ShaderName fragment{};
            std::span<const SDL_GPUVertexAttribute> attributes;
            std::uint32_t pitch{};
            std::span<const SDL_GPUColorTargetDescription> targets;
            std::span<const SDL_GPUVertexAttribute> instanceAttributes;
            std::uint32_t instancePitch{};
        };

        // Makes a pipeline that draws triangle lists as SDL's renderer does: no culling, one sample, no depth.
        auto CreatePipeline(SDL_GPUDevice* device, const PipelineInfo& x) -> Pipeline
        {
            const auto vertex = CreateShader(device, x.vertex);
            const auto fragment = CreateShader(device, x.fragment);

            if (!vertex || !fragment)
            {
                return {};
            }

            // Direct3D 12 finds a vertex buffer's description by its index, so each buffer's at the slot of its index:
            // the vertices' at 0, and any instances' at 1.
            const std::array buffers{
                SDL_GPUVertexBufferDescription{.slot = 0, .pitch = x.pitch, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0},
                SDL_GPUVertexBufferDescription{
                    .slot = 1, .pitch = x.instancePitch, .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE, .instance_step_rate = 0},
            };
            const auto vertices = !std::empty(x.attributes);
            const auto instances = !std::empty(x.instanceAttributes);
            std::vector<SDL_GPUVertexAttribute> attributes{std::begin(x.attributes), std::end(x.attributes)};
            attributes.insert(std::end(attributes), std::begin(x.instanceAttributes), std::end(x.instanceAttributes));
            SDL_GPUGraphicsPipelineCreateInfo info{};
            info.vertex_shader = vertex.get();
            info.fragment_shader = fragment.get();
            info.vertex_input_state.vertex_buffer_descriptions = vertices ? std::data(buffers) : nullptr;
            info.vertex_input_state.num_vertex_buffers = instances ? 2 : (vertices ? 1 : 0);
            info.vertex_input_state.vertex_attributes = std::data(attributes);
            info.vertex_input_state.num_vertex_attributes = static_cast<std::uint32_t>(std::size(attributes));
            info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            info.rasterizer_state.enable_depth_clip = true;
            info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
            info.target_info.color_target_descriptions = std::data(x.targets);
            info.target_info.num_color_targets = static_cast<std::uint32_t>(std::size(x.targets));
            return {device, SDL_CreateGPUGraphicsPipeline(device, &info)};
        }
    }
}

export namespace scopegpu
{
    /// @brief How a frame was drawn, for the readouts and the benchmark.
    struct FrameStats
    {
        /// Render passes begun, the copy pass aside.
        std::uint32_t renderPasses{};
        std::uint32_t draws{};
        /// Bytes copied to the GPU: vertices, indices and new glyphs.
        std::uint32_t uploadBytes{};
        /// Time spent waiting for a swapchain texture, or offscreen for a frame in flight to finish, which
        /// `Painter::getMilliseconds()` leaves out.
        double waitMilliseconds{};
        /// Time spent waiting for a captured frame and saving it, which `Painter::getMilliseconds()` also leaves out.
        double captureMilliseconds{};
    };

    /// @brief Draws the scope with SDL's GPU API.
    ///
    /// The picture's shapes are tessellated as scope tessellates them and drawn in one render pass, in the
    /// picture's order with its text. The glow is drawn into two small textures at once, its full channels and its
    /// fainter ones, blurred across and then down by a shader that takes every tap in one pass, and composited
    /// over the backdrop by a shader that shades the backdrop itself. Every frame's data goes up in one copy pass.
    ///
    /// Retained, the tracks' marks are drawn instead as instances of one mesh kept on the GPU, in one draw for the
    /// main pass and one for the glow; each frame uploads only where each mark is.
    class Painter
    {
    public:
        /// @brief Makes a painter that draws into a window, or offscreen.
        /// @param device The device. It must outlive the painter.
        /// @param window The window, already claimed for the device, or null to draw into a texture of its own.
        /// @param offscreen The size of that texture, if there's no window.
        /// @return The painter, or null if SDL failed; `SDL_GetError()` says why.
        [[nodiscard]] static auto create(SDL_GPUDevice* device, SDL_Window* window, Extent offscreen) -> std::unique_ptr<Painter>
        {
            auto painter = std::unique_ptr<Painter>(new (std::nothrow) Painter(device, window, offscreen));
            return painter != nullptr && painter->initialize() ? std::move(painter) : nullptr;
        }

        ~Painter()
        {
            for (auto* fence : fences_)
            {
                if (fence != nullptr)
                {
                    SDL_WaitForGPUFences(device_, true, &fence, 1);
                    SDL_ReleaseGPUFence(device_, fence);
                }
            }
        }

        Painter(const Painter&) = delete;
        auto operator=(const Painter&) -> Painter& = delete;
        Painter(Painter&&) noexcept = delete;
        auto operator=(Painter&&) noexcept -> Painter& = delete;

        /// @brief Loads the font the labels are drawn in. Without one, they're left out.
        /// @param data A TrueType font. The painter keeps a copy.
        /// @return `true` if it loaded.
        auto loadFont(std::span<const char> data) -> bool
        {
            return glyphs_.loadFont(data);
        }

        /// @brief Reports how long the last frame took to paint, waits for the swapchain aside.
        /// @return Milliseconds, from acquiring the frame's command buffer to submitting it.
        [[nodiscard]] auto getMilliseconds() const -> double
        {
            return milliseconds_;
        }

        /// @brief Reports how the last frame was drawn.
        /// @return The counts and times, until the next frame's painted.
        [[nodiscard]] auto getStats() const -> const FrameStats&
        {
            return stats_;
        }

        /// @brief Sets the line under the title, saying what draws the scope.
        /// @param x The caption.
        auto setCaption(std::string x) -> void
        {
            caption_ = std::move(x);
        }

        /// @brief Sets the frame rate the readouts show.
        /// @param x Frames per second.
        auto setFramesPerSecond(double x) -> void
        {
            framesPerSecond_ = x;
        }

        /// @brief Sets whether the tracks' marks are drawn from a mesh kept on the GPU, an instance for each track,
        /// rather than tessellated afresh each frame as scope tessellates them.
        ///
        /// Each frame then uploads only where each track is, rather than every triangle of every mark. The marks look
        /// the same, but their corners are placed by the GPU, so they can differ from scope's in their last bits.
        ///
        /// @param x `true` to keep the marks' mesh on the GPU; `false`, the default, to draw scope's triangles.
        auto setRetained(bool x) -> void
        {
            retained_ = x;
        }

        /// @brief Draws and submits a frame of the scope, presenting it if there's a window.
        /// @param scope The scope to draw.
        /// @param capture A PNG file to save the frame to, or empty not to. Saving waits for the GPU.
        /// @return `false` if SDL failed; `SDL_GetError()` says why.
        auto paint(const scope::Scope& scope, std::string_view capture) -> bool
        {
            const auto started = Clock::now();
            stats_ = {};
            auto* commands = SDL_AcquireGPUCommandBuffer(device_);

            if (commands == nullptr)
            {
                return false;
            }

            const auto drawn = draw(commands, scope, capture);
            milliseconds_ = Milliseconds(Clock::now() - started) - stats_.waitMilliseconds - stats_.captureMilliseconds;
            return drawn;
        }

    private:
        // Enough for the atlas to grow from its smallest to its largest; past that, glyphs that don't fit are left out.
        static constexpr int MaxTextAttempts = 4;

        // As blur.frag was compiled with.
        static constexpr std::size_t MaxTaps = shadercode::MaxTaps;

        // Offscreen frames are paced as a swapchain paces them: no more than this many in flight.
        static constexpr std::size_t FramesInFlight = 2;

        // blur.frag's uniform block, laid out as std140 lays it out.
        struct BlurUniforms
        {
            std::array<float, 4> axis{};
            std::array<float, 4> used{};
            std::array<std::array<float, 4>, MaxTaps> taps{};
        };

        // One axis of the blur: its uniforms, and the part of the target it can reach.
        struct BlurStep
        {
            BlurUniforms blur;
            SDL_Rect area{};
        };

        // The texture a frame's drawn into, and its size.
        struct Target
        {
            SDL_GPUTexture* texture{};
            Extent size;
        };

        // What a run draws from the text: the first of the glyphs' indices or, for data blocks, of their glyphs'
        // instances, how many, and for data blocks, how their glyphs step.
        struct TextRange
        {
            std::uint32_t first{};
            std::uint32_t count{};
            BlockMetrics metrics;
        };

        // A run of marks to draw, into the main pass's target or the glow's.
        struct MarksDraw
        {
            MarksRun run;
            bool glow{};
            Extent target;
        };

        // Where the marks' meshes are in the buffers kept for them, in bytes from their starts: the picture's scale's
        // first, then the glow's.
        struct MarkLayout
        {
            std::uint32_t glowVertices{};
            std::uint32_t vertexBytes{};
            std::uint32_t glowIndices{};
            std::uint32_t indexBytes{};
        };

        // Where a frame's data is in the buffers, in bytes from their starts. The main mesh starts each buffer.
        struct Layout
        {
            std::uint32_t glowVertices{};
            std::uint32_t glyphVertices{};
            std::uint32_t markInstances{};
            std::uint32_t blockCorners{};
            std::uint32_t blockGlyphs{};
            std::uint32_t vertexBytes{};
            std::uint32_t glowIndices{};
            std::uint32_t glyphIndices{};
            std::uint32_t indexBytes{};
            // In the transfer buffer: the vertices, then the indices, then the new glyphs, then the marks' meshes if
            // they were made again.
            std::uint32_t indexUpload{};
            std::uint32_t glyphUpload{};
            std::uint32_t markUpload{};
            std::optional<MarkLayout> marks;
            std::uint32_t uploadBytes{};
        };

        Painter(SDL_GPUDevice* device, SDL_Window* window, Extent offscreen)
            : device_{device}, window_{window}, offscreenSize_{offscreen}, glyphs_{device}
        {
        }

        // Makes the pipelines, samplers and textures every frame needs.
        auto initialize() -> bool
        {
            targetFormat_ = window_ != nullptr ? SDL_GetGPUSwapchainTextureFormat(device_, window_) : OffscreenFormat;

            const std::array frame{SDL_GPUColorTargetDescription{.format = targetFormat_, .blend_state = Blend}};
            const std::array frameReplaced{SDL_GPUColorTargetDescription{.format = targetFormat_, .blend_state = Replace}};
            const std::array glow{SDL_GPUColorTargetDescription{.format = GlowFormat, .blend_state = Blend},
                                  SDL_GPUColorTargetDescription{.format = GlowFormat, .blend_state = Blend}};
            const std::array glowReplaced{SDL_GPUColorTargetDescription{.format = GlowFormat, .blend_state = Replace},
                                          SDL_GPUColorTargetDescription{.format = GlowFormat, .blend_state = Replace}};

            shapePipeline_ = CreatePipeline(device_, {.vertex = ShaderName::ShapeVertex,
                                                      .fragment = ShaderName::ShapeFragment,
                                                      .attributes = ShapeAttributes,
                                                      .pitch = sizeof(scope::Vertex),
                                                      .targets = frame});
            glowPipeline_ = CreatePipeline(device_, {.vertex = ShaderName::ShapeVertex,
                                                     .fragment = ShaderName::GlowFragment,
                                                     .attributes = ShapeAttributes,
                                                     .pitch = sizeof(scope::Vertex),
                                                     .targets = glow});
            textPipeline_ = CreatePipeline(device_, {.vertex = ShaderName::TextVertex,
                                                     .fragment = ShaderName::TextFragment,
                                                     .attributes = TextAttributes,
                                                     .pitch = sizeof(GlyphVertex),
                                                     .targets = frame});
            blurPipeline_ = CreatePipeline(device_, {.vertex = ShaderName::FullscreenVertex,
                                                     .fragment = ShaderName::BlurFragment,
                                                     .attributes = {},
                                                     .pitch = 0,
                                                     .targets = glowReplaced});
            compositePipeline_ = CreatePipeline(device_, {.vertex = ShaderName::FullscreenVertex,
                                                          .fragment = ShaderName::CompositeFragment,
                                                          .attributes = {},
                                                          .pitch = 0,
                                                          .targets = frameReplaced});
            markPipeline_ = CreatePipeline(device_, {.vertex = ShaderName::MarkVertex,
                                                     .fragment = ShaderName::ShapeFragment,
                                                     .attributes = MarkAttributes,
                                                     .pitch = sizeof(MarkVertex),
                                                     .targets = frame,
                                                     .instanceAttributes = MarkInstanceAttributes,
                                                     .instancePitch = sizeof(MarkInstance)});
            markGlowPipeline_ = CreatePipeline(device_, {.vertex = ShaderName::MarkVertex,
                                                         .fragment = ShaderName::GlowFragment,
                                                         .attributes = MarkAttributes,
                                                         .pitch = sizeof(MarkVertex),
                                                         .targets = glow,
                                                         .instanceAttributes = MarkInstanceAttributes,
                                                         .instancePitch = sizeof(MarkInstance)});
            linear_ = CreateSampler(device_, SDL_GPU_FILTER_LINEAR);
            nearest_ = CreateSampler(device_, SDL_GPU_FILTER_NEAREST);
            // Bound in the glow's place when nothing glows, since the composite always takes two textures.
            placeholder_ = CreateTexture(device_, GlowFormat, {.width = 1, .height = 1}, SDL_GPU_TEXTUREUSAGE_SAMPLER);

            if (window_ == nullptr)
            {
                offscreen_ =
                    CreateTexture(device_, OffscreenFormat, offscreenSize_, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
            }

            blockPipeline_ = CreatePipeline(device_, {.vertex = ShaderName::BlockVertex,
                                                      .fragment = ShaderName::TextFragment,
                                                      .attributes = BlockCornerAttributes,
                                                      .pitch = sizeof(scope::Vec2),
                                                      .targets = frame,
                                                      .instanceAttributes = BlockGlyphAttributes,
                                                      .instancePitch = sizeof(BlockGlyph)});

            return shapePipeline_ && glowPipeline_ && textPipeline_ && blurPipeline_ && compositePipeline_ && markPipeline_ && markGlowPipeline_
                   && blockPipeline_ && linear_ && nearest_ && placeholder_ && (window_ != nullptr || offscreen_);
        }

        // The texture to draw the frame into: the window's swapchain texture, waited for, or the offscreen one. Empty
        // if SDL failed; holding no texture if the window has none to give.
        auto acquireTarget(SDL_GPUCommandBuffer* commands) -> std::optional<Target>
        {
            const auto waiting = Clock::now();

            if (window_ == nullptr)
            {
                // The frame that last used this slot has to finish first, as a swapchain's would.
                if (auto*& fence = fences_.at(nextFence_); fence != nullptr)
                {
                    const auto finished = SDL_WaitForGPUFences(device_, true, &fence, 1);
                    SDL_ReleaseGPUFence(device_, fence);
                    fence = nullptr;

                    if (!finished)
                    {
                        return std::nullopt;
                    }
                }

                stats_.waitMilliseconds = Milliseconds(Clock::now() - waiting);
                return Target{.texture = offscreen_.get(), .size = offscreenSize_};
            }

            SDL_GPUTexture* texture = nullptr;
            Uint32 width = 0;
            Uint32 height = 0;

            if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &texture, &width, &height))
            {
                return std::nullopt;
            }

            stats_.waitMilliseconds = Milliseconds(Clock::now() - waiting);
            return Target{.texture = texture, .size = {.width = width, .height = height}};
        }

        // The size the frame's target will have: the window's in pixels, which its swapchain's texture takes when it's
        // acquired, or the offscreen texture's.
        [[nodiscard]] auto expectedSize() const -> Extent
        {
            if (window_ == nullptr)
            {
                return offscreenSize_;
            }

            auto width = 0;
            auto height = 0;
            SDL_GetWindowSizeInPixels(window_, &width, &height);
            return {.width = static_cast<std::uint32_t>(std::max(width, 1)), .height = static_cast<std::uint32_t>(std::max(height, 1))};
        }

        // Lays out, uploads and draws a frame, and submits it. All but the main pass is recorded before the target's
        // waited for, as SDL's renderer records a frame before it waits at present, so the GPU still has the frames in
        // flight queued while the CPU lays this one out. Waiting first left it only one, and when another process shares
        // the GPU, its work took the gap and held the frames back for longer: a slower 1% of frames, though no slower a
        // median.
        auto draw(SDL_GPUCommandBuffer* commands, const scope::Scope& scope, std::string_view capture) -> bool
        {
            const auto size = expectedSize();
            auto layout = prepare(commands, scope, size);

            if (!layout.has_value())
            {
                return abandon(commands);
            }

            auto target = acquireTarget(commands);

            if (!target.has_value())
            {
                return abandon(commands);
            }

            // Nothing to draw into, as while a window's minimized: the frame's submitted without its main pass. A frame
            // to capture is still drawn, at the window's size, into the texture it's read back from.
            if (target->texture == nullptr && std::empty(capture))
            {
                return SDL_SubmitGPUCommandBuffer(commands);
            }

            if (target->texture == nullptr)
            {
                target->size = size;
            }

            // The window changed size after it was measured: the frame's laid out again, at the swapchain's size.
            if (target->size != size)
            {
                layout = prepare(commands, scope, target->size);

                if (!layout.has_value())
                {
                    return abandon(commands);
                }
            }

            // A capture's drawn into a texture that can be read back, and copied to the window from there.
            auto* into = target->texture;

            if (!std::empty(capture) && window_ != nullptr)
            {
                if (!reserveCapture(target->size))
                {
                    return abandon(commands);
                }

                into = capture_.get();
            }

            drawMain(commands, *layout, {.texture = into, .size = target->size});

            if (std::empty(capture))
            {
                return submit(commands);
            }

            return saveCapture(commands, {.texture = into, .size = target->size}, target->texture, capture);
        }

        // Lays out a frame at a size, uploads it and draws its glow: everything but the main pass, which needs the
        // target. Empty if SDL failed.
        auto prepare(SDL_GPUCommandBuffer* commands, const scope::Scope& scope, Extent size) -> std::optional<Layout>
        {
            picture_ = Compose(scope, {.width = size.width,
                                       .height = size.height,
                                       .text = glyphs_.ready(),
                                       .advance = glyphs_.advance(),
                                       .caption = caption_,
                                       .milliseconds = milliseconds_,
                                       .framesPerSecond = framesPerSecond_,
                                       .marks = retained_});
            frame_.build(picture_, static_cast<int>(size.width), static_cast<int>(size.height));

            if (!layOutText())
            {
                return std::nullopt;
            }

            const auto layout = layOut();

            if (!reserve(layout) || !upload(commands, layout) || !drawGlow(commands, layout))
            {
                return std::nullopt;
            }

            return layout;
        }

        // Submits a frame. Offscreen, it keeps the frame's fence, to wait on before the frame that next takes its slot.
        auto submit(SDL_GPUCommandBuffer* commands) -> bool
        {
            if (window_ != nullptr)
            {
                return SDL_SubmitGPUCommandBuffer(commands);
            }

            auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
            fences_.at(nextFence_) = fence;
            nextFence_ = (nextFence_ + 1) % FramesInFlight;
            return fence != nullptr;
        }

        // Gives up on a frame. Once a swapchain texture's been acquired, SDL won't cancel its command buffer, so it's
        // submitted as it is. Reports `false`, keeping SDL's error.
        auto abandon(SDL_GPUCommandBuffer* commands) -> bool
        {
            const std::string error{SDL_GetError()};

            if (window_ != nullptr)
            {
                SDL_SubmitGPUCommandBuffer(commands);
            }
            else
            {
                SDL_CancelGPUCommandBuffer(commands);
            }

            // SDL's errors are printf-style, so the saved message goes through as one.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            SDL_SetError("%s", error.c_str());
            return false;
        }

        // Lays out the frame's text, in order, noting which of the glyphs' indices each text's run draws. If the
        // glyphs don't fit in the atlas, it grows and the text's laid out again, so no frame goes without any.
        auto layOutText() -> bool
        {
            for (auto attempt = 0; attempt < MaxTextAttempts; ++attempt)
            {
                textRanges_.clear();

                if (!glyphs_.begin())
                {
                    return false;
                }

                for (const auto& run : frame_.runs())
                {
                    // Data blocks are drawn from their glyphs' instances, with the metrics they're placed by.
                    if (const auto* blocks = std::get_if<BlocksRun>(&run); blocks != nullptr)
                    {
                        const auto first = static_cast<std::uint32_t>(std::size(glyphs_.blockGlyphs()));
                        const auto metrics = glyphs_.addBlocks(*blocks->blocks, frame_.scale());

                        if (!metrics.has_value())
                        {
                            return false;
                        }

                        textRanges_.push_back(
                            {.first = first, .count = static_cast<std::uint32_t>(std::size(glyphs_.blockGlyphs())) - first, .metrics = *metrics});
                        continue;
                    }

                    const auto* text = std::get_if<TextRun>(&run);
                    const auto first = static_cast<std::uint32_t>(std::size(glyphs_.indices()));

                    if (text != nullptr && !glyphs_.add(*text->text, frame_.scale()))
                    {
                        return false;
                    }

                    textRanges_.push_back({.first = first, .count = static_cast<std::uint32_t>(std::size(glyphs_.indices())) - first, .metrics = {}});
                }

                if (!glyphs_.overflowed())
                {
                    break;
                }
            }

            return true;
        }

        // Works out where the frame's data goes in the buffers.
        [[nodiscard]] auto layOut() const -> Layout
        {
            Layout x;
            x.glowVertices = AlignUp(BytesOf(frame_.mesh().vertices()), RegionAlignment);
            x.glyphVertices = x.glowVertices + AlignUp(BytesOf(frame_.glowMesh().vertices()), RegionAlignment);
            x.vertexBytes = x.glyphVertices + BytesOf(glyphs_.vertices());

            // The marks' instances go up with the vertices, which they're drawn from beside the marks' mesh.
            if (!std::empty(frame_.markInstances()))
            {
                x.markInstances = AlignUp(x.vertexBytes, RegionAlignment);
                x.vertexBytes = x.markInstances + BytesOf(frame_.markInstances());
            }

            // So do the data blocks' glyphs' instances, after the corners of the quad they're each drawn as.
            if (!std::empty(glyphs_.blockGlyphs()))
            {
                x.blockCorners = AlignUp(x.vertexBytes, RegionAlignment);
                x.blockGlyphs = AlignUp(x.blockCorners + BytesOf(QuadCorners), RegionAlignment);
                x.vertexBytes = x.blockGlyphs + BytesOf(glyphs_.blockGlyphs());
            }

            x.glowIndices = AlignUp(BytesOf(frame_.mesh().indices()), RegionAlignment);
            x.glyphIndices = x.glowIndices + AlignUp(BytesOf(frame_.glowMesh().indices()), RegionAlignment);
            x.indexBytes = x.glyphIndices + BytesOf(glyphs_.indices());
            x.indexUpload = AlignUp(x.vertexBytes, RegionAlignment);
            x.glyphUpload = AlignUp(x.indexUpload + x.indexBytes, RegionAlignment);
            x.uploadBytes = x.glyphUpload + static_cast<std::uint32_t>(std::size(glyphs_.staged()));

            // The marks' meshes go up only when they've been made again.
            if (frame_.markGeneration() != markGeneration_)
            {
                MarkLayout marks;
                marks.glowVertices = AlignUp(BytesOf(frame_.markMesh().vertices), RegionAlignment);
                marks.vertexBytes = marks.glowVertices + BytesOf(frame_.markGlowMesh().vertices);
                marks.glowIndices = AlignUp(BytesOf(frame_.markMesh().indices), RegionAlignment);
                marks.indexBytes = marks.glowIndices + BytesOf(frame_.markGlowMesh().indices);
                x.markUpload = AlignUp(x.uploadBytes, RegionAlignment);
                x.uploadBytes = x.markUpload + AlignUp(marks.vertexBytes, RegionAlignment) + marks.indexBytes;
                x.marks = marks;
            }

            return x;
        }

        // Grows the buffers to hold a frame.
        auto reserve(const Layout& layout) -> bool
        {
            const auto grown = [](std::uint32_t needed) { return std::bit_ceil(std::max(needed, SmallestBuffer)); };

            if (!vertices_ || vertexCapacity_ < layout.vertexBytes)
            {
                const auto size = grown(layout.vertexBytes);
                vertices_ = CreateBuffer(device_, SDL_GPU_BUFFERUSAGE_VERTEX, size);
                vertexCapacity_ = vertices_ ? size : 0;
            }

            if (!indices_ || indexCapacity_ < layout.indexBytes)
            {
                const auto size = grown(layout.indexBytes);
                indices_ = CreateBuffer(device_, SDL_GPU_BUFFERUSAGE_INDEX, size);
                indexCapacity_ = indices_ ? size : 0;
            }

            if (!upload_ || uploadCapacity_ < layout.uploadBytes)
            {
                const auto size = grown(layout.uploadBytes);
                upload_ = CreateTransferBuffer(device_, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size);
                uploadCapacity_ = upload_ ? size : 0;
            }

            if (!layout.marks.has_value())
            {
                return vertices_ && indices_ && upload_;
            }

            if (!markVertices_ || markVertexCapacity_ < layout.marks->vertexBytes)
            {
                const auto size = grown(layout.marks->vertexBytes);
                markVertices_ = CreateBuffer(device_, SDL_GPU_BUFFERUSAGE_VERTEX, size);
                markVertexCapacity_ = markVertices_ ? size : 0;
            }

            if (!markIndices_ || markIndexCapacity_ < layout.marks->indexBytes)
            {
                const auto size = grown(layout.marks->indexBytes);
                markIndices_ = CreateBuffer(device_, SDL_GPU_BUFFERUSAGE_INDEX, size);
                markIndexCapacity_ = markIndices_ ? size : 0;
            }

            return vertices_ && indices_ && upload_ && markVertices_ && markIndices_;
        }

        // Copies the frame's vertices, indices and new glyphs to the GPU, in one copy pass.
        auto upload(SDL_GPUCommandBuffer* commands, const Layout& layout) -> bool
        {
            // The frames in flight may still be reading their uploads, so SDL hands over another buffer if they are.
            auto* mapped = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device_, upload_.get(), true));

            if (mapped == nullptr)
            {
                return false;
            }

            const std::span bytes{mapped, uploadCapacity_};
            const auto put = [&bytes](std::uint32_t offset, std::span<const std::byte> x)
            { std::ranges::copy(x, std::begin(bytes.subspan(offset, std::size(x)))); };

            put(0, std::as_bytes(std::span{frame_.mesh().vertices()}));
            put(layout.glowVertices, std::as_bytes(std::span{frame_.glowMesh().vertices()}));
            put(layout.glyphVertices, std::as_bytes(std::span{glyphs_.vertices()}));
            put(layout.indexUpload, std::as_bytes(std::span{frame_.mesh().indices()}));
            put(layout.indexUpload + layout.glowIndices, std::as_bytes(std::span{frame_.glowMesh().indices()}));
            put(layout.indexUpload + layout.glyphIndices, std::as_bytes(std::span{glyphs_.indices()}));
            put(layout.glyphUpload, glyphs_.staged());
            put(layout.markInstances, std::as_bytes(std::span{frame_.markInstances()}));

            if (!std::empty(glyphs_.blockGlyphs()))
            {
                put(layout.blockCorners, std::as_bytes(std::span{QuadCorners}));
                put(layout.blockGlyphs, std::as_bytes(std::span{glyphs_.blockGlyphs()}));
            }

            const auto markIndexUpload = layout.marks.has_value() ? layout.markUpload + AlignUp(layout.marks->vertexBytes, RegionAlignment) : 0;

            if (layout.marks.has_value())
            {
                put(layout.markUpload, std::as_bytes(std::span{frame_.markMesh().vertices}));
                put(layout.markUpload + layout.marks->glowVertices, std::as_bytes(std::span{frame_.markGlowMesh().vertices}));
                put(markIndexUpload, std::as_bytes(std::span{frame_.markMesh().indices}));
                put(markIndexUpload + layout.marks->glowIndices, std::as_bytes(std::span{frame_.markGlowMesh().indices}));
            }

            SDL_UnmapGPUTransferBuffer(device_, upload_.get());

            auto* copy = SDL_BeginGPUCopyPass(commands);

            if (copy == nullptr)
            {
                return false;
            }

            // The buffers may still be drawn from by the frames in flight, so they're cycled rather than overwritten.
            if (layout.vertexBytes > 0)
            {
                const SDL_GPUTransferBufferLocation from{.transfer_buffer = upload_.get(), .offset = 0};
                const SDL_GPUBufferRegion to{.buffer = vertices_.get(), .offset = 0, .size = layout.vertexBytes};
                SDL_UploadToGPUBuffer(copy, &from, &to, true);
            }

            if (layout.indexBytes > 0)
            {
                const SDL_GPUTransferBufferLocation from{.transfer_buffer = upload_.get(), .offset = layout.indexUpload};
                const SDL_GPUBufferRegion to{.buffer = indices_.get(), .offset = 0, .size = layout.indexBytes};
                SDL_UploadToGPUBuffer(copy, &from, &to, true);
            }

            // New glyphs go where nothing's been drawn from since the atlas was last cleared.
            for (const auto& glyph : glyphs_.uploads())
            {
                const SDL_GPUTextureTransferInfo from{.transfer_buffer = upload_.get(),
                                                      .offset = layout.glyphUpload + glyph.offset,
                                                      .pixels_per_row = glyph.pitch / BytesPerPixel,
                                                      .rows_per_layer = glyph.slot.height};
                const SDL_GPUTextureRegion to{.texture = glyphs_.atlas(),
                                              .mip_level = 0,
                                              .layer = 0,
                                              .x = glyph.slot.x,
                                              .y = glyph.slot.y,
                                              .z = 0,
                                              .w = glyph.slot.width,
                                              .h = glyph.slot.height,
                                              .d = 1};
                SDL_UploadToGPUTexture(copy, &from, &to, false);
            }

            // The marks' meshes stay where they are from frame to frame. When they're made again, the frames in flight
            // may still be drawing the old ones, so the buffers are cycled.
            if (layout.marks.has_value())
            {
                const SDL_GPUTransferBufferLocation vertices{.transfer_buffer = upload_.get(), .offset = layout.markUpload};
                const SDL_GPUBufferRegion intoVertices{.buffer = markVertices_.get(), .offset = 0, .size = layout.marks->vertexBytes};
                SDL_UploadToGPUBuffer(copy, &vertices, &intoVertices, true);
                const SDL_GPUTransferBufferLocation indices{.transfer_buffer = upload_.get(), .offset = markIndexUpload};
                const SDL_GPUBufferRegion intoIndices{.buffer = markIndices_.get(), .offset = 0, .size = layout.marks->indexBytes};
                SDL_UploadToGPUBuffer(copy, &indices, &intoIndices, true);
                markLayout_ = *layout.marks;
                markGeneration_ = frame_.markGeneration();
            }

            SDL_EndGPUCopyPass(copy);
            stats_.uploadBytes = layout.uploadBytes;
            return true;
        }

        // Grows the glow's textures to hold its plan. They never shrink, so a window being resized doesn't remake them.
        auto reserveGlow(const scope::GlowPlan& plan) -> bool
        {
            const Extent size{.width = static_cast<std::uint32_t>(scope::GlowTextureSize(plan.width, static_cast<int>(glowSize_.width))),
                              .height = static_cast<std::uint32_t>(scope::GlowTextureSize(plan.height, static_cast<int>(glowSize_.height)))};

            if (size == glowSize_ && std::ranges::all_of(glow_, [](const Texture& x) { return static_cast<bool>(x); }))
            {
                return true;
            }

            glowSize_ = size;

            for (auto& texture : glow_)
            {
                texture = CreateTexture(device_, GlowFormat, size, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);

                if (!texture)
                {
                    return false;
                }
            }

            return true;
        }

        // Draws the glowing shapes into the glow's textures, split between them, then blurs them across and back down.
        auto drawGlow(SDL_GPUCommandBuffer* commands, const Layout& layout) -> bool
        {
            const auto& plan = frame_.glow();

            if (!plan.has_value())
            {
                return true;
            }

            if (!reserveGlow(*plan))
            {
                return false;
            }

            constexpr SDL_FColor clear{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F};
            auto* full = glow_.at(FullGlow).get();
            auto* fainter = glow_.at(FainterGlow).get();
            auto* fullAcross = glow_.at(FullAcross).get();
            auto* fainterAcross = glow_.at(FainterAcross).get();

            // The glowing shapes, over clear textures, which leaves them holding premultiplied color.
            const std::array shapes{ColorTarget(full, clear, true), ColorTarget(fainter, clear, true)};
            auto* pass = beginPass(commands, shapes);
            const auto projection = ProjectionOf(glowSize_);

            // The tracks' marks go first, as their groups come before ownship's.
            if (frame_.marksGlow())
            {
                drawMarks(commands, pass, layout,
                          {.run = {.firstInstance = 0, .instanceCount = static_cast<std::uint32_t>(std::size(frame_.markInstances()))},
                           .glow = true,
                           .target = glowSize_});
            }

            BindMesh(pass, glowPipeline_.get(),
                     {.vertices = {.buffer = vertices_.get(), .offset = layout.glowVertices},
                      .indices = {.buffer = indices_.get(), .offset = layout.glowIndices}});
            SDL_PushGPUVertexUniformData(commands, 0, &projection, sizeof(projection));
            drawIndexed(pass, static_cast<std::uint32_t>(std::size(frame_.glowMesh().indices())), 0);
            SDL_EndGPURenderPass(pass);

            const auto& taps = plan->taps;

            if (std::size(taps) > MaxTaps)
            {
                // SDL's errors are printf-style, so the count goes through as an argument.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                SDL_SetError("The glow's blur has %zu taps, more than the %zu its shader takes", std::size(taps), MaxTaps);
                return false;
            }

            BlurUniforms blur;

            for (std::size_t i = 0; i < std::min(std::size(taps), MaxTaps); ++i)
            {
                blur.taps.at(i) = {taps.at(i).offset, taps.at(i).weight, 0.0F, 0.0F};
            }

            blur.used = {static_cast<float>(plan->width), static_cast<float>(plan->height), static_cast<float>(std::min(std::size(taps), MaxTaps)),
                         0.0F};
            const auto texel = std::array{1.0F / static_cast<float>(glowSize_.width), 1.0F / static_cast<float>(glowSize_.height)};

            // A tap's copy reaches no further than its shift past the used part, so the blur shades only that far, and
            // the clear leaves the rest of the texture empty, as scope's copies do.
            const auto reach = std::ranges::max(taps | std::views::transform([](const scope::Tap& x) { return std::abs(x.offset); }));
            const auto beyond = [reach](int used, std::uint32_t size)
            { return static_cast<int>(std::min(static_cast<float>(size), std::ceil(static_cast<float>(used) + reach))); };

            blur.axis = {1.0F, 0.0F, texel.at(0), texel.at(1)};
            blurPass(commands, {ColorTarget(fullAcross, clear, true), ColorTarget(fainterAcross, clear, true)}, {full, fainter},
                     {.blur = blur, .area = {.x = 0, .y = 0, .w = beyond(plan->width, glowSize_.width), .h = plan->height}});
            blur.axis = {0.0F, 1.0F, texel.at(0), texel.at(1)};
            blurPass(commands, {ColorTarget(full, clear, false), ColorTarget(fainter, clear, false)}, {fullAcross, fainterAcross},
                     {.blur = blur, .area = {.x = 0, .y = 0, .w = plan->width, .h = beyond(plan->height, glowSize_.height)}});
            return true;
        }

        // Blurs a pair of textures along an axis into another pair.
        auto blurPass(SDL_GPUCommandBuffer* commands, const std::array<SDL_GPUColorTargetInfo, 2>& into, const std::array<SDL_GPUTexture*, 2>& from,
                      const BlurStep& step) -> void
        {
            auto* pass = beginPass(commands, into);
            const auto& blur = step.blur;
            SDL_SetGPUScissor(pass, &step.area);
            const std::array samplers{SDL_GPUTextureSamplerBinding{.texture = from.at(0), .sampler = linear_.get()},
                                      SDL_GPUTextureSamplerBinding{.texture = from.at(1), .sampler = linear_.get()}};
            SDL_BindGPUGraphicsPipeline(pass, blurPipeline_.get());
            SDL_BindGPUFragmentSamplers(pass, 0, std::data(samplers), static_cast<std::uint32_t>(std::size(samplers)));
            SDL_PushGPUFragmentUniformData(commands, 0, &blur, sizeof(blur));
            SDL_DrawGPUPrimitives(pass, FullscreenVertices, 1, 0, 0);
            ++stats_.draws;
            SDL_EndGPURenderPass(pass);
        }

        // Draws the frame's runs in order: the backdrop with the glow, shapes, and text.
        auto drawMain(SDL_GPUCommandBuffer* commands, const Layout& layout, const Target& target) -> void
        {
            const auto background = scope::TintOf(scope::BackgroundColor, 1.0F);
            const auto replaced = !std::empty(frame_.runs()) && std::holds_alternative<BackdropRun>(frame_.runs().front());
            const std::array into{ColorTarget(
                target.texture,
                replaced ? std::nullopt : std::optional{SDL_FColor{.r = background.r, .g = background.g, .b = background.b, .a = background.a}},
                false)};
            auto* pass = beginPass(commands, into);
            const auto projection = ProjectionOf(target.size);
            SDL_PushGPUVertexUniformData(commands, 0, &projection, sizeof(projection));

            // What's bound, so a run only binds what changed since the one before.
            enum class Bound : std::uint8_t
            {
                Nothing,
                Shapes,
                Text,
                Backdrop,
                Marks,
                Blocks,
            };
            auto bound = Bound::Nothing;

            for (std::size_t i = 0; i < std::size(frame_.runs()); ++i)
            {
                std::visit(
                    aspire::core::Overloaded{
                        [&](const BackdropRun& x)
                        {
                            drawBackdrop(commands, pass, *x.backdrop, target.size);
                            bound = Bound::Backdrop;
                        },
                        [&](const ShapeRun& x)
                        {
                            if (bound != Bound::Shapes)
                            {
                                BindMesh(pass, shapePipeline_.get(),
                                         {.vertices = {.buffer = vertices_.get(), .offset = 0}, .indices = {.buffer = indices_.get(), .offset = 0}});
                                bound = Bound::Shapes;
                            }

                            drawIndexed(pass, x.indexCount, x.firstIndex);
                        },
                        [&](const TextRun& /*x*/)
                        {
                            const auto& range = textRanges_.at(i);

                            if (range.count == 0)
                            {
                                return;
                            }

                            if (bound != Bound::Text)
                            {
                                BindMesh(pass, textPipeline_.get(),
                                         {.vertices = {.buffer = vertices_.get(), .offset = layout.glyphVertices},
                                          .indices = {.buffer = indices_.get(), .offset = layout.glyphIndices}});
                                const SDL_GPUTextureSamplerBinding atlas{.texture = glyphs_.atlas(), .sampler = nearest_.get()};
                                SDL_BindGPUFragmentSamplers(pass, 0, &atlas, 1);
                                bound = Bound::Text;
                            }

                            drawIndexed(pass, range.count, range.first);
                        },
                        [&](const MarksRun& x)
                        {
                            drawMarks(commands, pass, layout, {.run = x, .glow = false, .target = target.size});
                            bound = Bound::Marks;
                        },
                        [&](const BlocksRun& /*x*/)
                        {
                            if (textRanges_.at(i).count == 0)
                            {
                                return;
                            }

                            drawBlocks(commands, pass, layout, textRanges_.at(i), target.size);
                            bound = Bound::Blocks;
                        },
                    },
                    frame_.runs().at(i));
            }

            SDL_EndGPURenderPass(pass);
        }

        // Draws a run of data blocks' glyphs, each an instance of a glyph's quad that the GPU places. The uniforms they
        // push start with the target's projection, which shapes and text drawn after them read.
        auto drawBlocks(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const Layout& layout, const TextRange& range, Extent target) -> void
        {
            // The instances are bound from the run's first, so each backend starts its instances at 0.
            const std::array buffers{
                SDL_GPUBufferBinding{.buffer = vertices_.get(), .offset = layout.blockCorners},
                SDL_GPUBufferBinding{.buffer = vertices_.get(),
                                     .offset = layout.blockGlyphs + (range.first * static_cast<std::uint32_t>(sizeof(BlockGlyph)))},
            };
            const SDL_GPUTextureSamplerBinding atlas{.texture = glyphs_.atlas(), .sampler = nearest_.get()};
            const BlockUniforms uniforms{.projection = ProjectionOf(target).projection,
                                         .metrics = {range.metrics.advance, range.metrics.lineSkip, range.metrics.ascent, 0.0F}};
            SDL_BindGPUGraphicsPipeline(pass, blockPipeline_.get());
            SDL_BindGPUVertexBuffers(pass, 0, std::data(buffers), static_cast<std::uint32_t>(std::size(buffers)));
            SDL_BindGPUFragmentSamplers(pass, 0, &atlas, 1);
            SDL_PushGPUVertexUniformData(commands, 0, &uniforms, sizeof(uniforms));
            SDL_DrawGPUPrimitives(pass, static_cast<std::uint32_t>(std::size(QuadCorners)), range.count, 0, 0);
            ++stats_.draws;
        }

        // Draws a run of the tracks' marks as instances of their mesh: at the picture's scale, where the scale ring
        // keeps and cuts them, or at the glow's, whole. The uniforms they push start with the target's projection,
        // which shapes and text drawn after them read.
        auto drawMarks(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const Layout& layout, const MarksDraw& x) -> void
        {
            const auto& mesh = x.glow ? frame_.markGlowMesh() : frame_.markMesh();

            if (x.run.instanceCount == 0 || std::empty(mesh.indices))
            {
                return;
            }

            // The instances are bound from the run's first, so each backend starts its instances at 0.
            const std::array buffers{
                SDL_GPUBufferBinding{.buffer = markVertices_.get(), .offset = x.glow ? markLayout_.glowVertices : 0},
                SDL_GPUBufferBinding{.buffer = vertices_.get(),
                                     .offset = layout.markInstances + (x.run.firstInstance * static_cast<std::uint32_t>(sizeof(MarkInstance)))},
            };
            const SDL_GPUBufferBinding indices{.buffer = markIndices_.get(), .offset = x.glow ? markLayout_.glowIndices : 0};
            // The glow's marks are whole: the scale ring only clips the main pass's.
            const auto& view = frame_.markView();
            const auto clipped = !x.glow && view.clip.has_value();
            const auto clip = view.clip.value_or(scope::Clip{});
            const MarkUniforms uniforms{.projection = ProjectionOf(x.target).projection,
                                        .view = {x.glow ? frame_.glow()->scale : frame_.scale(), clipped ? 1.0F : 0.0F, 0.0F, 0.0F},
                                        .world = {view.center.x, view.center.y, view.unitsPerNm, view.interval},
                                        .clip = {clip.center.x, clip.center.y, clip.radius, 0.0F}};
            SDL_BindGPUGraphicsPipeline(pass, x.glow ? markGlowPipeline_.get() : markPipeline_.get());
            SDL_BindGPUVertexBuffers(pass, 0, std::data(buffers), static_cast<std::uint32_t>(std::size(buffers)));
            SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            SDL_PushGPUVertexUniformData(commands, 0, &uniforms, sizeof(uniforms));
            SDL_DrawGPUIndexedPrimitives(pass, static_cast<std::uint32_t>(std::size(mesh.indices)), x.run.instanceCount, 0, 0, 0);
            ++stats_.draws;
        }

        // Shades the backdrop over the whole target, with the glow over it if anything glows.
        auto drawBackdrop(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const scope::Backdrop& x, Extent target) -> void
        {
            const auto scale = frame_.scale();
            const auto& plan = frame_.glow();
            const auto glowing = plan.has_value();
            // The glow's stretched from the part of its textures it uses over the whole target, as scope draws it.
            const auto stretch = [&](int used, std::uint32_t length, std::uint32_t texture)
            { return glowing ? static_cast<float>(used) / static_cast<float>(length) / static_cast<float>(texture) : 0.0F; };

            BackdropUniforms uniforms;
            uniforms.inner = Rgba(x.inner);
            uniforms.outer = Rgba(x.outer);
            uniforms.disc = {x.center.x * scale, x.center.y * scale, x.radius * scale, glowing ? 1.0F : 0.0F};
            uniforms.glow = {stretch(glowing ? plan->width : 0, target.width, glowSize_.width),
                             stretch(glowing ? plan->height : 0, target.height, glowSize_.height), 0.0F, 0.0F};
            uniforms.rect = {x.corner.x * scale, x.corner.y * scale, (x.corner.x + x.width) * scale, (x.corner.y + x.height) * scale};
            uniforms.background = Rgba(scope::BackgroundColor);

            const std::array samplers{
                SDL_GPUTextureSamplerBinding{.texture = glowing ? glow_.at(FullGlow).get() : placeholder_.get(), .sampler = linear_.get()},
                SDL_GPUTextureSamplerBinding{.texture = glowing ? glow_.at(FainterGlow).get() : placeholder_.get(), .sampler = linear_.get()}};
            SDL_BindGPUGraphicsPipeline(pass, compositePipeline_.get());
            SDL_BindGPUFragmentSamplers(pass, 0, std::data(samplers), static_cast<std::uint32_t>(std::size(samplers)));
            SDL_PushGPUFragmentUniformData(commands, 0, &uniforms, sizeof(uniforms));
            SDL_DrawGPUPrimitives(pass, FullscreenVertices, 1, 0, 0);
            ++stats_.draws;
        }

        auto drawIndexed(SDL_GPURenderPass* pass, std::uint32_t count, std::uint32_t first) -> void
        {
            if (count > 0)
            {
                SDL_DrawGPUIndexedPrimitives(pass, count, 1, first, 0, 0);
                ++stats_.draws;
            }
        }

        template <std::size_t N>
        auto beginPass(SDL_GPUCommandBuffer* commands, const std::array<SDL_GPUColorTargetInfo, N>& targets) -> SDL_GPURenderPass*
        {
            ++stats_.renderPasses;
            return SDL_BeginGPURenderPass(commands, std::data(targets), static_cast<std::uint32_t>(N), nullptr);
        }

        // Grows the texture a captured frame is drawn into, which the swapchain's can't be read back from.
        auto reserveCapture(Extent size) -> bool
        {
            if (!capture_ || captureSize_ != size)
            {
                captureSize_ = size;
                capture_ = CreateTexture(device_, targetFormat_, size, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
            }

            return static_cast<bool>(capture_);
        }

        // Copies a drawn frame to the window, if there is one, submits it, and saves it as a PNG once the GPU's done.
        auto saveCapture(SDL_GPUCommandBuffer* commands, const Target& drawn, SDL_GPUTexture* swapchain, std::string_view path) -> bool
        {
            const auto [width, height] = drawn.size;

            if (swapchain != nullptr && swapchain != drawn.texture)
            {
                SDL_GPUBlitInfo blit{};
                blit.source = {.texture = drawn.texture, .mip_level = 0, .layer_or_depth_plane = 0, .x = 0, .y = 0, .w = width, .h = height};
                blit.destination = {.texture = swapchain, .mip_level = 0, .layer_or_depth_plane = 0, .x = 0, .y = 0, .w = width, .h = height};
                blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
                blit.filter = SDL_GPU_FILTER_NEAREST;
                SDL_BlitGPUTexture(commands, &blit);
            }

            const auto bytes = width * height * BytesPerPixel;
            auto download = CreateTransferBuffer(device_, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, bytes);
            auto* copy = download ? SDL_BeginGPUCopyPass(commands) : nullptr;

            if (copy == nullptr)
            {
                SDL_SubmitGPUCommandBuffer(commands);
                return false;
            }

            const SDL_GPUTextureRegion from{
                .texture = drawn.texture, .mip_level = 0, .layer = 0, .x = 0, .y = 0, .z = 0, .w = width, .h = height, .d = 1};
            const SDL_GPUTextureTransferInfo to{.transfer_buffer = download.get(), .offset = 0, .pixels_per_row = width, .rows_per_layer = height};
            SDL_DownloadFromGPUTexture(copy, &from, &to);
            SDL_EndGPUCopyPass(copy);

            auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);

            if (fence == nullptr)
            {
                return false;
            }

            // Waiting for the GPU and saving the file aren't painting, so they're timed apart.
            const auto saving = Clock::now();

            const auto finished = SDL_WaitForGPUFences(device_, true, &fence, 1);
            SDL_ReleaseGPUFence(device_, fence);
            auto* pixels = finished ? SDL_MapGPUTransferBuffer(device_, download.get(), false) : nullptr;

            if (pixels == nullptr)
            {
                return false;
            }

            auto* surface =
                SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height), SDL_GetPixelFormatFromGPUTextureFormat(targetFormat_),
                                      pixels, static_cast<int>(width * BytesPerPixel));
            const auto saved = surface != nullptr && SDL_SavePNG(surface, std::string{path}.c_str());
            SDL_DestroySurface(surface);
            SDL_UnmapGPUTransferBuffer(device_, download.get());
            stats_.captureMilliseconds = Milliseconds(Clock::now() - saving);
            return saved;
        }

        SDL_GPUDevice* device_;
        SDL_Window* window_;
        Extent offscreenSize_;
        SDL_GPUTextureFormat targetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
        Pipeline shapePipeline_;
        Pipeline glowPipeline_;
        Pipeline textPipeline_;
        Pipeline blurPipeline_;
        Pipeline compositePipeline_;
        Pipeline markPipeline_;
        Pipeline markGlowPipeline_;
        Pipeline blockPipeline_;
        Sampler linear_;
        Sampler nearest_;
        Texture placeholder_;
        Texture offscreen_;
        // The glow's textures, by `FullGlow` and the rest.
        std::array<Texture, 4> glow_;
        Extent glowSize_;
        Texture capture_;
        Extent captureSize_;
        Buffer vertices_;
        Buffer indices_;
        TransferBuffer upload_;
        std::uint32_t vertexCapacity_{};
        std::uint32_t indexCapacity_{};
        std::uint32_t uploadCapacity_{};
        // The marks' meshes, kept from frame to frame: where each is, and which make of them the frame's was uploaded.
        Buffer markVertices_;
        Buffer markIndices_;
        std::uint32_t markVertexCapacity_{};
        std::uint32_t markIndexCapacity_{};
        MarkLayout markLayout_;
        std::uint32_t markGeneration_{};
        bool retained_{};
        Glyphs glyphs_;
        // The picture being drawn, which the frame's runs point into, so it's kept until the frame's main pass is recorded.
        scope::Picture picture_;
        Frame frame_;
        // What each of the frame's runs draws from the text.
        std::vector<TextRange> textRanges_;
        FrameStats stats_;
        // Offscreen, the fences of the frames in flight, and the slot the next frame takes.
        std::array<SDL_GPUFence*, FramesInFlight> fences_{};
        std::size_t nextFence_{};
        std::string caption_;
        double milliseconds_{};
        double framesPerSecond_{};
    };
}
