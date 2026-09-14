#include <gtest/gtest.h>
#include <nameof.hpp>

import aspire.core.object;
import aspire.core.objectfactory;

namespace
{
    class MyObject : public aspire::core::Object
    {
    public:
        MyObject() = default;
        ~MyObject() override = default;
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