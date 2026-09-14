module;

#include <nlohmann/json.hpp>

export module aspire.parser.json;

import std;
import aspire.core.object;
import aspire.core.objectfactory;

export namespace aspire::parser::json
{
    auto ReadFile(const aspire::core::ObjectFactory& factory, const std::filesystem::path& x) -> std::shared_ptr<aspire::core::Object>
    {
        std::ignore = factory;
        auto json = nlohmann::json::parse(std::ifstream{x});

        auto typeIt = json.find("type");

        if (typeIt == std::end(json))
        {
            return nullptr;
        }

        auto object = factory.create(typeIt->get<std::string>());

        json.erase(typeIt);

        auto nameIt = json.find("name");

        if (nameIt != std::end(json))
        {
            object->setName(nameIt->get<std::string>());
            json.erase(nameIt);
        }

        for (const auto& item : json.items())
        {
            auto* property = object->getProperty(item.key());

            if (property == nullptr)
            {
                // Add error.
                continue;
            }

            property->setValueJson(item.value());
        }

        return object;
    }
}