export module scopegpu.frame;

import std;
import aspire.core;
import scope.glow;
import scope.mesh;
import scope.picture;
import scope.tessellation;
import scope.world;

// A frame of the scope laid out for the GPU: the same triangles scope draws, and what's drawn in what order.
export namespace scopegpu
{
    /// @brief Triangles from the frame's mesh, drawn in the main pass.
    struct ShapeRun
    {
        /// The first of the mesh's indices drawn.
        std::uint32_t firstIndex{};
        std::uint32_t indexCount{};
    };

    /// @brief A text, drawn in the main pass at the picture's scale.
    struct TextRun
    {
        const scope::Text* text{};
    };

    /// @brief The backdrop with the glow over it, drawn over the whole target.
    struct BackdropRun
    {
        const scope::Backdrop* backdrop{};
    };

    /// @brief Something drawn in the main pass.
    using Run = std::variant<ShapeRun, TextRun, BackdropRun>;

    /// @brief A frame of the scope, laid out for the GPU.
    ///
    /// The shapes are tessellated as scope tessellates them, into one mesh for the frame and one for the glow. The
    /// main pass draws the frame's runs in the picture's order, so shapes and text overlap as they do in scope. The
    /// glow is tessellated once with all its channels, which the GPU splits into full and fainter as it draws them,
    /// and on a thread of its own while the frame's mesh is tessellated, since every glowing shape is tessellated twice.
    ///
    /// A frame is reused from one to the next, so its meshes keep their memory.
    class Frame
    {
    public:
        /// @brief Lays out a picture.
        /// @param picture The picture. The frame's runs point into it, so it must outlive their use.
        /// @param width The target's width in pixels.
        /// @param height The target's height in pixels.
        auto build(const scope::Picture& picture, int width, int height) -> void
        {
            mesh_.clear();
            glowMesh_.clear();
            runs_.clear();
            glowShapes_.clear();
            glow_.reset();
            runStart_ = 0;
            scale_ = picture.scale;

            if (!(picture.scale > 0.0F))
            {
                return;
            }

            scope::CollectGlow(picture.root, glowShapes_);

            // The glow's mesh is tessellated alongside the frame's. The two only read the picture, and each fills a
            // mesh of its own.
            std::future<void> glowing;

            if (!std::empty(glowShapes_))
            {
                glow_ = scope::PlanGlow(picture.scale, width, height);
                const scope::Style style{.scale = glow_->scale, .clip = std::nullopt, .widen = 0.0F, .opacity = scope::GlowOpacity};
                glowing = std::async(std::launch::async, [this, style] { tessellateGlow(style); });
            }

            addGroup(picture.root, {.scale = picture.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F});
            closeShapes();

            // Rethrows anything the glow's thread threw, once it's done.
            if (glowing.valid())
            {
                glowing.get();
            }
        }

        /// @brief The main pass's triangles, in target pixels.
        /// @return The mesh, until the next `build()`.
        [[nodiscard]] auto mesh() const -> const scope::Mesh&
        {
            return mesh_;
        }

        /// @brief The glowing shapes' triangles, in the glow textures' pixels, with all their channels.
        /// @return The mesh, until the next `build()`.
        [[nodiscard]] auto glowMesh() const -> const scope::Mesh&
        {
            return glowMesh_;
        }

        /// @brief What the main pass draws, in order.
        /// @return The runs, until the next `build()`. They point into the picture.
        [[nodiscard]] auto runs() const -> const std::vector<Run>&
        {
            return runs_;
        }

        /// @brief How the glow is drawn.
        /// @return The plan, empty if nothing glows.
        [[nodiscard]] auto glow() const -> const std::optional<scope::GlowPlan>&
        {
            return glow_;
        }

        /// @brief Reports the picture's scale.
        /// @return Target pixels per logical unit.
        [[nodiscard]] auto scale() const -> float
        {
            return scale_;
        }

    private:
        // Adds the glowing shapes to the glow's mesh, in the order they're drawn.
        auto tessellateGlow(const scope::Style& style) -> void
        {
            for (const auto* shape : glowShapes_)
            {
                scope::Tessellate(glowMesh_, *shape, style);
            }
        }

        // Adds a group's items in order, as scope's painter draws them.
        // NOLINTNEXTLINE(misc-no-recursion)
        auto addGroup(const scope::Group& group, scope::Style style) -> void
        {
            if (group.clip.has_value())
            {
                style.clip = scope::ClipOf(*group.clip, style.scale);
            }

            for (const auto& item : group.items)
            {
                std::visit(
                    aspire::core::Overloaded{
                        [&](const scope::Shape& x) { scope::Tessellate(mesh_, x, style); },
                        [&](const scope::Text& x)
                        {
                            closeShapes();
                            runs_.emplace_back(TextRun{.text = &x});
                        },
                        [&](const scope::Backdrop& x)
                        {
                            closeShapes();
                            runs_.emplace_back(BackdropRun{.backdrop = &x});
                        },
                        [&](const scope::Rule& x) { scope::TessellateRule(mesh_, x, style.scale); },
                        // NOLINTNEXTLINE(misc-no-recursion)
                        [&](const scope::Group& x) { addGroup(x, style); },
                    },
                    item.value);
            }
        }

        // Ends the run of shapes added since the last text or backdrop, if there are any.
        auto closeShapes() -> void
        {
            const auto end = std::size(mesh_.indices());

            if (end > runStart_)
            {
                runs_.emplace_back(
                    ShapeRun{.firstIndex = static_cast<std::uint32_t>(runStart_), .indexCount = static_cast<std::uint32_t>(end - runStart_)});
            }

            runStart_ = end;
        }

        scope::Mesh mesh_;
        scope::Mesh glowMesh_;
        std::vector<Run> runs_;
        std::vector<const scope::Shape*> glowShapes_;
        std::optional<scope::GlowPlan> glow_;
        float scale_{};
        // Where the run of shapes being added starts, in the mesh's indices.
        std::size_t runStart_{};
    };
}
