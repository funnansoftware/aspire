#include <gtest/gtest.h>
#include <limits>

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
    constexpr auto result = stream::string::FromString<std::int8_t>("42");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), static_cast<std::int8_t>(42));
}

TEST(FromString, float)
{
    {
        const auto result = stream::string::FromString<float>("3.14");
        EXPECT_EQ(result.value(), 3.14F);
    }

    {
        const auto result = stream::string::FromString<float>(std::to_string(std::numeric_limits<float>::max()));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), std::numeric_limits<float>::max());
    }

    {
        const auto result = stream::string::FromString<float>("1e39");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::OutOfRange);
    }

    {
        const auto result = stream::string::FromString<float>("abc");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, double)
{
    {
        const auto result = stream::string::FromString<double>("3.14");
        EXPECT_EQ(result.value(), 3.14);
    }

    {
        const auto result = stream::string::FromString<double>(std::to_string(std::numeric_limits<double>::max()));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), std::numeric_limits<double>::max());
    }

    {
        const auto result = stream::string::FromString<double>("1e309");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::OutOfRange);
    }

    {
        const auto result = stream::string::FromString<double>("abc");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, bool)
{
    {
        constexpr auto result = stream::string::FromString<bool>("true");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), true);
    }

    {
        constexpr auto result = stream::string::FromString<bool>("false");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), false);
    }

    {
        constexpr auto result = stream::string::FromString<bool>("TruE");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), true);
    }

    {
        constexpr auto result = stream::string::FromString<bool>("fAlSe");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), false);
    }

    {
        constexpr auto result = stream::string::FromString<bool>("1");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }

    {
        constexpr auto result = stream::string::FromString<bool>("0");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }

    {
        constexpr auto result = stream::string::FromString<bool>("batman");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }

    {
        constexpr auto result = stream::string::FromString<bool>(" true");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

TEST(FromString, char)
{
    {
        constexpr auto result = stream::string::FromString<char>("A");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 'A');
    }

    {
        constexpr auto result = stream::string::FromString<char>("9");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), '9');
    }

    {
        constexpr auto result = stream::string::FromString<char>("AB");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }

    {
        constexpr auto result = stream::string::FromString<char>("");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), stream::string::ErrorCode::InvalidArgument);
    }
}

// TEST(FromString, nullptr)
// {
//     EXPECT_EQ(stream::string::FromString<std::nullptr_t>("nullptr"), nullptr);
// }
