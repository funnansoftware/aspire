#include <gtest/gtest.h>

import stream.string.errorcode;
import stream.string.fromstring;

TEST(FromString, string)
{
    const auto result = stream::string::FromString<std::string>("hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "hello");
}

TEST(FromString, string_view)
{
    constexpr auto result = stream::string::FromString<std::string_view>("hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "hello");
}

TEST(FromString, char_star)
{
    constexpr auto result = stream::string::FromString<const char*>("hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), std::string_view{"hello"});
}

TEST(FromString, int)
{
    constexpr auto result = stream::string::FromString<int>("42");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(FromString, unsigned_int)
{
    {
        constexpr auto result = stream::string::FromString<unsigned int>("42");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42U);
    }

    {
        constexpr auto result = stream::string::FromString<unsigned int>("-1");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, long)
{
    {
        constexpr auto result = stream::string::FromString<long>("42");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42L);
    }

    {
        constexpr auto result = stream::string::FromString<long>("abc");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, unsigned_long)
{
    {
        constexpr auto result = stream::string::FromString<unsigned long>("42");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42UL);
    }

    {
        constexpr auto result = stream::string::FromString<unsigned long>("-1");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, long_long)
{
    {
        constexpr auto result = stream::string::FromString<long long>("42");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42LL);
    }

    {
        constexpr auto result = stream::string::FromString<long long>("abc");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, int8_t)
{
    EXPECT_EQ(stream::string::FromString<std::int8_t>("42"), static_cast<std::int8_t>(42));
}

// TEST(FromString, float)
// {
//     EXPECT_EQ(stream::string::FromString<float>("3.14"), 3.14F);
// }

// TEST(FromString, double)
// {
//     EXPECT_EQ(stream::string::FromString<double>("3.14"), 3.14);
// }

// TEST(FromString, bool)
// {
//     EXPECT_EQ(stream::string::FromString<bool>("true"), true);
//     EXPECT_EQ(stream::string::FromString<bool>("false"), false);
// }

// TEST(FromString, char)
// {
//     EXPECT_EQ(stream::string::FromString<char>("a"), 'a');
// }

// TEST(FromString, nullptr)
// {
//     EXPECT_EQ(stream::string::FromString<std::nullptr_t>("nullptr"), nullptr);
// }
