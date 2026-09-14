#include <gtest/gtest.h>

import aspire.core.object;

namespace
{
    class TestObject : public aspire::core::Object
    {
    public:
        TestObject() = default;
        ~TestObject() override = default;
    };

    class TestObject2 : public aspire::core::Object
    {
    public:
        TestObject2() = default;
        ~TestObject2() override = default;
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
    EXPECT_EQ(children[0], child);
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
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
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
    EXPECT_EQ(testObjectChildren[0], child1);

    auto testObject2Children = parent->getChildren<TestObject2>();
    ASSERT_EQ(testObject2Children.size(), 1);
    EXPECT_EQ(testObject2Children[0], child2);
}
