module;

#include <nlohmann/json.hpp>

export module aspire.parser:json;

import std;
import aspire.core;

export namespace aspire::parser
{
    auto ReadFile(const aspire::core::ObjectFactory& factory, const std::filesystem::path& x) -> std::shared_ptr<aspire::core::Object>;

    /// @brief Builds an object tree from JSON.
    ///
    /// `type` picks the class from `factory`, `name` sets the name, `objects` lists child objects inline, and
    /// `files` lists JSON files to read as children. Every other key sets the property of that name.
    ///
    /// @param factory Creates objects by type name.
    /// @param json The JSON. Keys it uses are removed from it.
    /// @param base The folder relative `files` resolve against. Empty means the working directory.
    /// @return The object, or null if `type` is missing or not registered.
    // ReadJson and ReadFile call each other to follow `files`; the depth is the nesting of the data's files.
    // NOLINTNEXTLINE(misc-no-recursion)
    auto ReadJson(const aspire::core::ObjectFactory& factory, nlohmann::json& json, const std::filesystem::path& base = {})
        -> std::shared_ptr<aspire::core::Object>
    {
        auto typeIt = json.find("type");

        if (typeIt == std::end(json))
        {
            return nullptr;
        }

        auto object = factory.create(typeIt->get<std::string>());

        if (object == nullptr)
        {
            std::println("Type not registered: {}", typeIt->get<std::string>());
            return nullptr;
        }

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
                    // Joining an absolute path replaces the base, so absolute paths are used as they are.
                    files.emplace_back(base / file.get<std::filesystem::path>());
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

            if (property->isReadOnly())
            {
                std::println("Property is read-only: {}", item.key());
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
            // Inline objects come from the same file, so their files resolve against the same folder.
            auto child = ReadJson(factory, o, base);
            object->addChild(child);
        }

        return object;
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    auto ReadFile(const aspire::core::ObjectFactory& factory, const std::filesystem::path& x) -> std::shared_ptr<aspire::core::Object>
    {
        auto json = nlohmann::json::parse(std::ifstream{x}, nullptr, true, true);

        // A file's own files are relative to its folder.
        return ReadJson(factory, json, x.parent_path());
    }
}
