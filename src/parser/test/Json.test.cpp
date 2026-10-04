#include <gtest/gtest.h>
#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.parser;
import aspire.core;

namespace
{
    class JsonObject : public aspire::core::Object
    {
    public:
        JsonObject()
        {
            registerProperty("texture", texture);
            registerProperty("rect", rect);
        }

        std::filesystem::path texture;
        std::array<int, 4> rect{};
    };
}

TEST(ReadFile, basic)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<JsonObject>();

    const auto tmp = std::filesystem::temp_directory_path() / "aspire-parser-test";
    std::filesystem::create_directories(tmp);
    const auto file = tmp / "test.json";

    const auto* const json = R"({
        "name": "ground_0",
        "texture": "path/to/texture.png",
        "rect": [0, 0, 100, 100],
        "type": "JsonObject"
    })";

    std::ofstream(file) << json;

    const auto obj = aspire::parser::ReadFile(factory, file);

    // Remove the temporary input before checking the parsed result.
    EXPECT_GT(std::filesystem::remove_all(tmp), 0);

    ASSERT_NE(obj, nullptr);

    EXPECT_EQ(obj->getName(), "ground_0");
    auto* const jsonObj = dynamic_cast<JsonObject*>(obj.get());
    ASSERT_NE(jsonObj, nullptr);
    // Compare as a string: GCC 15 cannot instantiate gtest's printer for std::filesystem::path
    // when <ostream> is included alongside `import std;` (operator<< for std::quoted is not found).
    EXPECT_EQ(jsonObj->texture.generic_string(), "path/to/texture.png");
    EXPECT_EQ(jsonObj->rect, (std::array<int, 4>{0, 0, 100, 100}));
}

TEST(ReadJson, unregisteredTypeGivesNull)
{
    const aspire::core::ObjectFactory factory;
    auto json = nlohmann::json::parse(R"({ "type": "Unregistered", "name": "x" })");

    EXPECT_EQ(aspire::parser::ReadJson(factory, json), nullptr);
}

TEST(ReadFile, readOnlyPropertyIsSkipped)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<JsonObject>();

    const auto tmp = std::filesystem::temp_directory_path() / "aspire-parser-test-read-only";
    std::filesystem::create_directories(tmp);
    const auto file = tmp / "test.json";

    // State 1 is Started. Applying it would let the object skip onStartup().
    // Keys load in sorted order, so "texture" comes after "state".
    const auto* const json = R"({
        "state": 1,
        "texture": "after.png",
        "type": "JsonObject"
    })";

    std::ofstream(file) << json;

    const auto obj = aspire::parser::ReadFile(factory, file);

    EXPECT_GT(std::filesystem::remove_all(tmp), 0);

    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->getState(), aspire::core::Object::State::Created);

    // Properties after the skipped one still load.
    auto* const jsonObj = dynamic_cast<JsonObject*>(obj.get());
    ASSERT_NE(jsonObj, nullptr);
    EXPECT_EQ(jsonObj->texture.generic_string(), "after.png");
}

TEST(ReadFile, filesResolveRelativeToTheirOwnFile)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<JsonObject>();

    // root.json names sub/child.json, which names leaf.json next to itself, and leaf.json names an absolute path.
    const auto tmp = std::filesystem::temp_directory_path() / "aspire-parser-test-relative";
    std::filesystem::create_directories(tmp / "sub");
    const auto absolute = tmp / "absolute.json";

    std::ofstream(tmp / "root.json") << R"({ "type": "JsonObject", "name": "root", "files": ["sub/child.json"] })";
    std::ofstream(tmp / "sub" / "child.json") << R"({ "type": "JsonObject", "name": "child", "files": ["leaf.json"] })";
    std::ofstream(tmp / "sub" / "leaf.json") << nlohmann::json{
        {"type", "JsonObject"},
        {"name", "leaf"},
        {"files", {absolute.generic_string()}}}.dump();
    std::ofstream(absolute) << R"({ "type": "JsonObject", "name": "absolute" })";

    // Nothing depends on the working directory: each file's paths resolve against its own folder.
    const auto root = aspire::parser::ReadFile(factory, tmp / "root.json");

    EXPECT_GT(std::filesystem::remove_all(tmp), 0);

    ASSERT_NE(root, nullptr);
    const auto child = root->getChild();
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->getName(), "child");
    const auto leaf = child->getChild();
    ASSERT_NE(leaf, nullptr);
    EXPECT_EQ(leaf->getName(), "leaf");
    const auto last = leaf->getChild();
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->getName(), "absolute");
}

TEST(ReadJson, inlineObjectsResolveFilesAgainstTheBase)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<JsonObject>();

    const auto tmp = std::filesystem::temp_directory_path() / "aspire-parser-test-base";
    std::filesystem::create_directories(tmp);
    std::ofstream(tmp / "leaf.json") << R"({ "type": "JsonObject", "name": "leaf" })";

    auto json = nlohmann::json::parse(R"({ "type": "JsonObject", "objects": [ { "type": "JsonObject", "files": ["leaf.json"] } ] })");
    const auto root = aspire::parser::ReadJson(factory, json, tmp);

    EXPECT_GT(std::filesystem::remove_all(tmp), 0);

    ASSERT_NE(root, nullptr);
    const auto inlined = root->getChild();
    ASSERT_NE(inlined, nullptr);
    const auto leaf = inlined->getChild();
    ASSERT_NE(leaf, nullptr);
    EXPECT_EQ(leaf->getName(), "leaf");
}
