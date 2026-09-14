export module aspire.parser.json;

import std;
import aspire.core.object;

export namespace aspire::parser::json
{
    auto ReadFile(const std::filesystem::path& x) -> std::shared_ptr<aspire::core::Object>
    {
        std::ignore = x;
        return nullptr;
    }
}