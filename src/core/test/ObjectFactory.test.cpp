#include <gtest/gtest.h>
#include <nameof.hpp>

import aspire.core.object;
import aspire.core.objectfactory;

namespace
{
    class MyObject : public aspire::core::Object
    {
    };
}

TEST(ObjectFactory, RegisterAndCreateObject)
{
    aspire::core::ObjectFactory factory;

    // Assuming you have a class MyObject that satisfies the ObjectType concept
    factory.registerObject<MyObject>();
    auto obj = factory.create("MyObject");

    EXPECT_NE(obj, nullptr);
}

TEST(ObjectFactory, RegisterWithCustomName)
{
    aspire::core::ObjectFactory factory;

    factory.registerObject<MyObject>("CustomObjectName");
    auto obj = factory.create("CustomObjectName");

    EXPECT_NE(obj, nullptr);
}
