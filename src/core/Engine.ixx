module;

#include <cstdlib>

export module aspire.core.engine;
import aspire.core.object;

export namespace aspire::core
{
    class Engine : public aspire::core::Object
    {
    public:
        [[nodiscard]] auto run() const -> int
        {
            return EXIT_SUCCESS;
        }
    };
}
