module;

#include <raylib.h>
#include <magic_enum/magic_enum.hpp>

export module aspire.raylib.text;

import std;
import aspire.core.object;
import aspire.core.overloaded;
import aspire.core.event;
import aspire.raylib.node;

export namespace aspire::raylib
{
    class Text : public aspire::raylib::Node
    {
    protected:
        auto onRender() const -> void override
        {
            // NOLINTNEXTLINE
            DrawText(text_.c_str(), 0, 0, 12, WHITE);
        }

        auto onEvent(aspire::core::Event& e) -> void override
        {
            std::visit(aspire::core::Overloaded{[this](aspire::core::EventKeyboard& e)
                                                {
                                                    text_ = magic_enum::enum_name(e.type);
                                                    text_ += ": ";
                                                    text_ += magic_enum::enum_name(e.key);
                                                },
                                                [this](aspire::core::EventMouse& e)
                                                {
                                                    text_ = std::format("{}: {}\nposition: {{ {}, {} }}", magic_enum::enum_name(e.type),
                                                                        magic_enum::enum_name(e.button), e.position.x, e.position.y);
                                                },
                                                [](auto&&) {}},
                       e);
        }

    private:
        std::string text_;
    };
}