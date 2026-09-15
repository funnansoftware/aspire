export module aspire.core.overloaded;

import std;

export namespace aspire::core
{
    template <typename... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };
}