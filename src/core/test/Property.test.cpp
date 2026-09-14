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
    aspire::core::TemplateProperty<int> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(42);
    EXPECT_EQ(std::any_cast<int>(prop.getValueAny()), 42);

    prop.setValueString("42");
    EXPECT_EQ(std::any_cast<int>(prop.getValueAny()), 42);
}

TEST(TemplateProperty, string)
{
    aspire::core::TemplateProperty<std::string> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::string("hello"));
    EXPECT_EQ(std::any_cast<std::string>(prop.getValueAny()), "hello");

    prop.setValueString("\"world\"");
    EXPECT_EQ(std::any_cast<std::string>(prop.getValueAny()), "world");
}

TEST(TemplateProperty, double)
{
    aspire::core::TemplateProperty<double> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(3.14);
    EXPECT_EQ(std::any_cast<double>(prop.getValueAny()), 3.14);

    prop.setValueString("3.14");
    EXPECT_EQ(std::any_cast<double>(prop.getValueAny()), 3.14);
}

TEST(TemplateProperty, bool)
{
    aspire::core::TemplateProperty<bool> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(true);
    EXPECT_EQ(std::any_cast<bool>(prop.getValueAny()), true);

    prop.setValueString("true");
    EXPECT_EQ(std::any_cast<bool>(prop.getValueAny()), true);
}

TEST(TemplateProperty, array)
{
    aspire::core::TemplateProperty<std::array<int, 5>> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::array<int, 5>{1, 2, 3, 0, 0});

    const auto val1 = prop.getValueAs<std::array<int, 5>>();
    const auto val2 = std::array<int, 5>{1, 2, 3, 0, 0};
    EXPECT_EQ(val1, val2);

    prop.setValueString("[1, 2, 3, 0, 0]");
    const auto val3 = prop.getValueAs<std::array<int, 5>>();
    const auto val4 = std::array<int, 5>{1, 2, 3, 0, 0};
    EXPECT_EQ(val3, val4);
}

TEST(TemplateProperty, nested_array)
{
    aspire::core::TemplateProperty<std::array<std::array<int, 3>, 2>> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});

    const auto val1 = prop.getValueAs<std::array<std::array<int, 3>, 2>>();
    const auto val2 = (std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
    EXPECT_EQ(val1, val2);

    prop.setValueString("[[1, 2, 3], [4, 5, 6]]");
    const auto val3 = prop.getValueAs<std::array<std::array<int, 3>, 2>>();
    const auto val4 = (std::array<std::array<int, 3>, 2>{{{1, 2, 3}, {4, 5, 6}}});
    EXPECT_EQ(val3, val4);
}

TEST(TemplateProperty, vector)
{
    aspire::core::TemplateProperty<std::vector<int>> prop("test");
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
    aspire::core::TemplateProperty<std::map<std::string, int>> prop("test");
    EXPECT_EQ(prop.name(), "test");

    prop.setValueAny(std::map<std::string, int>{{"one", 1}, {"two", 2}});
    const auto val1 = prop.getValueAs<std::map<std::string, int>>();
    const auto val2 = (std::map<std::string, int>{{"one", 1}, {"two", 2}});
    EXPECT_EQ(val1, val2);

    prop.setValueString("{\"one\": 1, \"two\": 2}");
    const auto val3 = prop.getValueAs<std::map<std::string, int>>();
    const auto val4 = (std::map<std::string, int>{{"one", 1}, {"two", 2}});
    EXPECT_EQ(val3, val4);
}

TEST(TemplateProperty, set)
{
    aspire::core::TemplateProperty<std::set<int>> prop("test");
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