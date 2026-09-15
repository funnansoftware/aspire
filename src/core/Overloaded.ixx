export module aspire.core.overloaded;

import std;

export namespace aspire::core
{
    template <typename... Ts>
    // NOLINTNEXTLINE(misc-multiple-inheritance)
    struct Overloaded : Ts...
    {
        // NOLINTNEXTLINE(misc-multiple-inheritance)
        using Ts::operator()...;
    };
}