#include <gtest/gtest.h>
#include <nameof.hpp>

import std;
import aspire.parser.json;
import aspire.core.object;
import aspire.core.objectfactory;

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
        std::array<int, 4> rect;
    };
}

TEST(ReadFile, basic)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<JsonObject>();

    const auto tmp = std::filesystem::temp_directory_path() / "aspire-parser-test";
    std::filesystem::create_directories(tmp);
    const auto file = tmp / "test.json";

    const auto json = R"({
        "name": "ground_0",
        "texture": "path/to/texture.png",
        "rect": [0, 0, 100, 100],
        "type": "JsonObject"
    })";

    std::ofstream(file) << json;

    const auto obj = aspire::parser::json::ReadFile(factory, file);

    // Remove file prior to performing any assert checks. We don't need this anymore.
    std::filesystem::remove_all(tmp);

    ASSERT_NE(obj, nullptr);

    EXPECT_EQ(obj->getName(), "ground_0");
    const auto jsonObj = dynamic_cast<JsonObject*>(obj.get());
    ASSERT_NE(jsonObj, nullptr);
    EXPECT_EQ(jsonObj->texture, "path/to/texture.png");
    EXPECT_EQ(jsonObj->rect, (std::array<int, 4>{0, 0, 100, 100}));
}
