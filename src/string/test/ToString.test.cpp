#include <gtest/gtest.h>

import aspire.string.tostring;

TEST(ToString, ToString)
{
    EXPECT_EQ(aspire::string::ToString("hello"), "hello");
}