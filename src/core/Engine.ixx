module;

#include <cstdlib>

export module aspire.core.engine;

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