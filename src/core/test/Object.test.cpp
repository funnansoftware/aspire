#include <gtest/gtest.h>

import std;
import aspire.core.object;

namespace
{
    constexpr int InitialPropertyValue = 10;

    class TestObject : public aspire::core::Object
    {
    };

    class TestObject2 : public aspire::core::Object
    {
    };

    class TestObjectWithProperty : public aspire::core::Object
    {
    public:
        TestObjectWithProperty()
        {
            registerProperty("value", value_);
        }

    private:
        int value_{InitialPropertyValue};
    };
}

TEST(Object, setName)
{
    auto obj = std::make_shared<aspire::core::Object>();
    obj->setName("test");
    EXPECT_EQ(obj->getName(), "test");
}

TEST(Object, addChild)
{
    auto parent = std::make_shared<aspire::core::Object>();
    auto child = std::make_shared<aspire::core::Object>();

    parent->addChild(child);

    auto children = parent->getChildren();
    ASSERT_EQ(children.size(), 1);
    EXPECT_EQ(children.front(), child);
}

TEST(Object, getChildren)
{
    auto parent = std::make_shared<aspire::core::Object>();
    auto child1 = std::make_shared<aspire::core::Object>();
    auto child2 = std::make_shared<aspire::core::Object>();

    parent->addChild(child1);
    parent->addChild(child2);

    auto children = parent->getChildren();
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(children.front(), child1);
    EXPECT_EQ(children.back(), child2);
}

TEST(Object, remove)
{
    auto parent = std::make_shared<aspire::core::Object>();
    auto child = std::make_shared<aspire::core::Object>();

    parent->addChild(child);

    EXPECT_EQ(child->getParent(), parent);

    child->remove();

    auto children = parent->getChildren();
    ASSERT_EQ(children.size(), 0);
    EXPECT_EQ(child->getParent(), nullptr);
}

TEST(Object, getChildrenOfType)
{
    auto parent = std::make_shared<aspire::core::Object>();
    auto child1 = std::make_shared<TestObject>();
    auto child2 = std::make_shared<TestObject2>();

    parent->addChild(child1);
    parent->addChild(child2);

    auto testObjectChildren = parent->getChildren<TestObject>();
    ASSERT_EQ(testObjectChildren.size(), 1);
    EXPECT_EQ(testObjectChildren.front(), child1);

    auto testObject2Children = parent->getChildren<TestObject2>();
    ASSERT_EQ(testObject2Children.size(), 1);
    EXPECT_EQ(testObject2Children.front(), child2);
}

TEST(Object, getProperties)
{
    auto obj = std::make_shared<TestObjectWithProperty>();
    auto properties = obj->getProperties();
    ASSERT_EQ(properties.size(), 1);
}

TEST(Object, getPropertyNameAndValue)
{
    auto obj = std::make_shared<TestObjectWithProperty>();
    auto properties = obj->getProperties();
    ASSERT_EQ(properties.size(), 1);
    EXPECT_EQ(properties.front()->name(), "value");
    EXPECT_EQ(properties.front()->getValueAs<int>(), InitialPropertyValue);
}
