module;

#include <raylib.h>

export module sl.viewworld;

import std;
import aspire.core.object;
import aspire.core.overloaded;
import aspire.core.event;
import aspire.core.vec2;
import aspire.raylib.node;
import aspire.raylib.tilemap;

export namespace sl
{
    class ViewWorld : public aspire::raylib::Node
    {
    protected:
        auto onStartup() -> void override
        {
            tileMap_ = getChild<aspire::raylib::TileMap>();
        }

        auto onEvent(aspire::core::Event& x) -> void override
        {
            using aspire::core::EventMouse;

            std::visit(aspire::core::Overloaded{[this](EventMouse& e)
                                                {
                                                    switch (e.type)
                                                    {
                                                        case EventMouse::Type::Moved:
                                                            mousePosition_ = mapFromGlobal({.x = e.position.x, .y = e.position.y});
                                                            break;
                                                        default:
                                                            break;
                                                    }
                                                },
                                                [](auto&&) {}},
                       x);
        }

        auto onRenderPre() const -> void override
        {
            Node::onRenderPre();

            const auto x = static_cast<int>(getPosition().x);
            const auto y = static_cast<int>(getPosition().y);
            BeginScissorMode(x, y, tileMap_->width(), tileMap_->height());
        }

        auto onRender() const -> void override
        {
            if (tileMap_ == nullptr)
            {
                return;
            }

            const auto tile = tileMap_->positionToTile(mousePosition_);
            constexpr auto transparent = 125;
            auto white = WHITE;
            white.a = transparent;
            DrawRectangle(static_cast<int>(tile.x), static_cast<int>(tile.y), static_cast<int>(tile.width), static_cast<int>(tile.height), white);
        }

        auto onRenderPost() const -> void override
        {
            EndScissorMode();
            Node::onRenderPost();
        }

    private:
        std::shared_ptr<aspire::raylib::TileMap> tileMap_;
        Vector2 mousePosition_{};
    };
}