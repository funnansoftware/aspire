module;

#include <cstdlib>

export module aspire.core.engine;
import aspire.core.object;

export namespace aspire::core
{
    class Engine : public aspire::core::Object
    {
    public:
        [[nodiscard]] static auto run() -> int
        {
            return EXIT_SUCCESS;
        }
    };
}
