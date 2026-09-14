export module aspire.raylib.drawable;

import aspire.core.object;

export namespace aspire::raylib
{
    class Drawable : public aspire::core::Object
    {
    public:
        virtual auto draw() const -> void = 0;
    };
}