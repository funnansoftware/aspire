#include <gtest/gtest.h>

import std;
import aspire.core.property;

namespace
{
    struct UnsupportedType
    {
    };

    static_assert(!aspire::core::JsonSerializable<UnsupportedType>);
}

TEST(TemplateProperty, int)
{
    constexpr int expectedValue = 42;
    int value{};
    aspire::core::TemplateProperty<int> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(expectedValue);
    EXPECT_EQ(std::any_cast<int>(prop.getValueAny()), expectedValue);

    prop.setValueString("42");
    EXPECT_EQ(std::any_cast<int>(prop.getValueAny()), expectedValue);
}

TEST(TemplateProperty, string)
{
    std::string value{};
    aspire::core::TemplateProperty<std::string> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::string("hello"));
    EXPECT_EQ(std::any_cast<std::string>(prop.getValueAny()), "hello");

    prop.setValueString("\"world\"");
    EXPECT_EQ(std::any_cast<std::string>(prop.getValueAny()), "world");
}

TEST(TemplateProperty, double)
{
    constexpr double expectedValue = 3.14;
    double value{};
    aspire::core::TemplateProperty<double> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(expectedValue);
    EXPECT_EQ(std::any_cast<double>(prop.getValueAny()), expectedValue);

    prop.setValueString("3.14");
    EXPECT_EQ(std::any_cast<double>(prop.getValueAny()), expectedValue);
}

TEST(TemplateProperty, bool)
{
    bool value{};
    aspire::core::TemplateProperty<bool> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(true);
    EXPECT_EQ(std::any_cast<bool>(prop.getValueAny()), true);

    prop.setValueString("true");
    EXPECT_EQ(std::any_cast<bool>(prop.getValueAny()), true);
}

TEST(TemplateProperty, array)
{
    constexpr std::size_t elementCount = 5;
    std::array<int, elementCount> value{};
    aspire::core::TemplateProperty<std::array<int, elementCount>> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::array<int, elementCount>{1, 2, 3, 0, 0});

    const auto val1 = prop.getValueAs<std::array<int, elementCount>>();
    const auto val2 = std::array<int, elementCount>{1, 2, 3, 0, 0};
    EXPECT_EQ(val1, val2);

    prop.setValueString("[1, 2, 3, 0, 0]");
    const auto val3 = prop.getValueAs<std::array<int, elementCount>>();
    const auto val4 = std::array<int, elementCount>{1, 2, 3, 0, 0};
    EXPECT_EQ(val3, val4);
}

TEST(TemplateProperty, nested_array)
{
    constexpr auto expectedValue = std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}};
    std::array<std::array<int, 3>, 2> value{};
    aspire::core::TemplateProperty<std::array<std::array<int, 3>, 2>> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(expectedValue);

    const auto val1 = prop.getValueAs<std::array<std::array<int, 3>, 2>>();
    EXPECT_EQ(val1, expectedValue);

    prop.setValueString("[[1, 2, 3], [4, 5, 6]]");
    const auto val3 = prop.getValueAs<std::array<std::array<int, 3>, 2>>();
    EXPECT_EQ(val3, expectedValue);
}

TEST(TemplateProperty, vector)
{
    std::vector<int> value{};
    aspire::core::TemplateProperty<std::vector<int>> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::vector<int>{1, 2, 3});
    const auto val1 = prop.getValueAs<std::vector<int>>();
    const auto val2 = (std::vector<int>{1, 2, 3});
    EXPECT_EQ(val1, val2);

    prop.setValueString("[1, 2, 3]");
    const auto val3 = prop.getValueAs<std::vector<int>>();
    const auto val4 = (std::vector<int>{1, 2, 3});
    EXPECT_EQ(val3, val4);
}

TEST(TemplateProperty, map)
{
    std::map<std::string, int> value{};
    aspire::core::TemplateProperty<std::map<std::string, int>> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::map<std::string, int>{{"one", 1}, {"two", 2}});
    const auto val1 = prop.getValueAs<std::map<std::string, int>>();
    const auto val2 = (std::map<std::string, int>{{"one", 1}, {"two", 2}});
    EXPECT_EQ(val1, val2);

    prop.setValueString(R"({"one": 1, "two": 2})");
    const auto val3 = prop.getValueAs<std::map<std::string, int>>();
    const auto val4 = (std::map<std::string, int>{{"one", 1}, {"two", 2}});
    EXPECT_EQ(val3, val4);
}

TEST(TemplateProperty, set)
{
    std::set<int> value{};
    aspire::core::TemplateProperty<std::set<int>> prop("test", value);
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::set<int>{1, 2, 3});
    const auto val1 = prop.getValueAs<std::set<int>>();
    const auto val2 = (std::set<int>{1, 2, 3});
    EXPECT_EQ(val1, val2);

    prop.setValueString("[1, 2, 3]");
    const auto val3 = prop.getValueAs<std::set<int>>();
    const auto val4 = (std::set<int>{1, 2, 3});
    EXPECT_EQ(val3, val4);
}
