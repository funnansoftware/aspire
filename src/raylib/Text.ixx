module;

#include <raylib.h>
#include <magic_enum/magic_enum.hpp>

export module aspire.raylib.text;

import std;
import aspire.core.object;
import aspire.core.overloaded;
import aspire.core.event;
import aspire.raylib.drawable;

export namespace aspire::raylib
{
    class Text : public Drawable
    {
    public:
        auto draw() const -> void override
        {
            // NOLINTNEXTLINE
            DrawText(text_.c_str(), 0, 0, 14, WHITE);
        }

    protected:
        auto onEvent(aspire::core::Event& e) -> void override
        {
            std::visit(aspire::core::Overloaded{[this](aspire::core::EventKeyboard& e)
                                                {
                                                    text_ = magic_enum::enum_name(e.type);
                                                    text_ += ": ";
                                                    text_ += magic_enum::enum_name(e.key);
                                                },
                                                [](auto&&) {}},
                       e);
        }

    private:
        std::string text_;
    };
}