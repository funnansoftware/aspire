#include <gtest/gtest.h>
#include <nameof.hpp>

import aspire.core;

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

TEST(ObjectFactory, RegisterModuleTypeByDefaultName)
{
    aspire::core::ObjectFactory factory;

    // Object belongs to the aspire.core module. GCC adds that to the type's name, which the factory must strip.
    factory.registerObject<aspire::core::Object>();

    EXPECT_NE(factory.create("Object"), nullptr);
}

TEST(ObjectFactory, RegisterWithCustomName)
{
    aspire::core::ObjectFactory factory;

    factory.registerObject<MyObject>("CustomObjectName");
    auto obj = factory.create("CustomObjectName");

    EXPECT_NE(obj, nullptr);
}
