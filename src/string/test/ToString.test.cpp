#include <gtest/gtest.h>

import stream.string.tostring;

TEST(ToString, ToString)
{
    EXPECT_EQ(stream::string::ToString("hello"), "hello");
}