module;

#include <nlohmann/json.hpp>

export module aspire.parser.json;

import std;
import aspire.core.object;
import aspire.core.objectfactory;

export namespace aspire::parser::json
{
    auto ReadFile(const aspire::core::ObjectFactory& factory, const std::filesystem::path& x) -> std::shared_ptr<aspire::core::Object>;

    auto ReadJson(const aspire::core::ObjectFactory& factory, nlohmann::json& json) -> std::shared_ptr<aspire::core::Object>
    {
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

        std::vector<nlohmann::json> objects;

        auto objectIt = json.find("objects");

        if (objectIt != std::end(json))
        {
            if (objectIt->is_array())
            {
                for (auto&& obj : *objectIt)
                {
                    objects.emplace_back(std::move(obj));
                }
            }

            json.erase(objectIt);
        }

        std::vector<std::filesystem::path> files;

        auto fileIt = json.find("files");

        if (fileIt != std::end(json))
        {
            if (fileIt->is_array())
            {
                for (const auto& file : *fileIt)
                {
                    files.emplace_back(file.get<std::filesystem::path>());
                }
            }

            json.erase(fileIt);
        }

        for (const auto& item : json.items())
        {
            auto* property = object->getProperty(item.key());
            std::println("Processing property: {}", item.key());

            if (property == nullptr)
            {
                std::println("Property not found: {}", item.key());
                // Add error.
                continue;
            }

            property->setValueJson(item.value());
        }

        for (const auto& f : files)
        {
            auto child = ReadFile(factory, f);
            object->addChild(child);
        }

        for (auto& o : objects)
        {
            auto child = ReadJson(factory, o);
            object->addChild(child);
        }

        return object;
    }

    auto ReadFile(const aspire::core::ObjectFactory& factory, const std::filesystem::path& x) -> std::shared_ptr<aspire::core::Object>
    {
        auto json = nlohmann::json::parse(std::ifstream{x}, nullptr, true, true);
        return ReadJson(factory, json);
    }
}
