#include <gtest/gtest.h>

import std;
import aspire.string;

TEST(ToString, ToString)
{
    EXPECT_EQ(aspire::string::ToString("hello"), "hello");
}
