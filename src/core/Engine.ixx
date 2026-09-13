module;

#include <cstdlib>

export module aspire.core.engine;

import std;

export namespace aspire::core
{
    class Engine
    {
    public:
        [[nodiscard]] auto run() const -> int
        {
            return EXIT_SUCCESS;
        }

    private:
    };
}