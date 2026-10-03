#include <gtest/gtest.h>

import std;
import aspire.core;

namespace
{
    constexpr int InitialPropertyValue = 10;

    class TestObject : public aspire::core::Object
    {
    };

    class TestObject2 : public aspire::core::Object
    {
    };

    using Log = std::vector<std::string>;

    class Recorder : public aspire::core::Object
    {
    public:
        explicit Recorder(Log& log, std::string_view name = {}) : log_{log}
        {
            setName(name);
        }

        auto whenStarted(std::function<void()> x) -> void
        {
            onStart_ = std::move(x);
        }

        auto whenStopped(std::function<void()> x) -> void
        {
            onStop_ = std::move(x);
        }

    protected:
        auto onStartup() -> void override
        {
            log_.get().push_back("start " + std::string{getName()});

            if (onStart_)
            {
                onStart_();
            }
        }

        // NOLINTNEXTLINE(bugprone-exception-escape): a throwing test hook should terminate.
        auto onShutdown() noexcept -> void override
        {
            log_.get().push_back("stop " + std::string{getName()});

            if (onStop_)
            {
                onStop_();
            }
        }

    private:
        std::reference_wrapper<Log> log_;
        std::function<void()> onStart_;
        std::function<void()> onStop_;
    };

    // Default-constructible, for getOrCreateChild<T>(); logs to a per-test static.
    struct LateChild : public aspire::core::Object
    {
        static inline int Starts{0};
        static inline int Stops{0};

    protected:
        auto onStartup() -> void override
        {
            ++Starts;
        }

        auto onShutdown() noexcept -> void override
        {
            ++Stops;
        }
    };

    auto Make(Log& log, std::string_view name) -> std::shared_ptr<Recorder>
    {
        return std::make_shared<Recorder>(log, name);
    }

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
    ASSERT_EQ(std::size(children), 1);
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
    ASSERT_EQ(std::size(children), 2);
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
    ASSERT_EQ(std::size(children), 0);
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
    ASSERT_EQ(std::size(testObjectChildren), 1);
    EXPECT_EQ(testObjectChildren.front(), child1);

    auto testObject2Children = parent->getChildren<TestObject2>();
    ASSERT_EQ(std::size(testObject2Children), 1);
    EXPECT_EQ(testObject2Children.front(), child2);
}

TEST(Object, getProperties)
{
    auto obj = std::make_shared<TestObjectWithProperty>();
    std::vector<std::string_view> names;

    for (const auto& property : obj->getProperties())
    {
        names.emplace_back(property->name());
    }

    // Object's own properties come first, since its constructor runs first.
    EXPECT_EQ(names, (std::vector<std::string_view>{"name", "state", "value"}));
}

TEST(Object, getPropertyNameAndValue)
{
    auto obj = std::make_shared<TestObjectWithProperty>();
    const auto* property = obj->getProperty("value");
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property->name(), "value");
    EXPECT_EQ(property->getValueAs<int>(), InitialPropertyValue);
}

TEST(Object, namePropertyIsWritable)
{
    auto obj = std::make_shared<aspire::core::Object>();
    auto* property = obj->getProperty("name");
    ASSERT_NE(property, nullptr);
    EXPECT_FALSE(property->isReadOnly());

    property->setValueString(R"("renamed")");
    EXPECT_EQ(obj->getName(), "renamed");
}

TEST(Object, statePropertyIsReadOnly)
{
    using State = aspire::core::Object::State;

    auto obj = std::make_shared<aspire::core::Object>();
    auto* property = obj->getProperty("state");
    ASSERT_NE(property, nullptr);
    EXPECT_TRUE(property->isReadOnly());

    // Setting it does nothing: only startup() and shutdown() change the state.
    property->setValueAny(State::Started);
    property->setValueString("1");
    EXPECT_EQ(obj->getState(), State::Created);

    // Reading it follows the live state.
    obj->startup();
    EXPECT_EQ(property->getValueAs<State>(), State::Started);
}

using State = aspire::core::Object::State;

TEST(Object, stateStartsCreated)
{
    const auto obj = std::make_shared<aspire::core::Object>();
    EXPECT_EQ(obj->getState(), State::Created);
    EXPECT_FALSE(obj->isStarted());
}

TEST(Object, startupShutdownStartupAgain)
{
    Log log;
    const auto obj = Make(log, "a");

    obj->startup();
    EXPECT_EQ(obj->getState(), State::Started);
    obj->shutdown();
    EXPECT_EQ(obj->getState(), State::Shutdown);
    obj->startup();
    EXPECT_EQ(obj->getState(), State::Started);
    obj->shutdown();

    EXPECT_EQ(log, (Log{"start a", "stop a", "start a", "stop a"}));
}

TEST(Object, startupIsParentFirstShutdownIsReverse)
{
    Log log;
    const auto p = Make(log, "p");
    const auto a = Make(log, "a");
    const auto a1 = Make(log, "a1");
    const auto b = Make(log, "b");

    p->addChild(a);
    a->addChild(a1);
    p->addChild(b);

    p->startup();
    p->shutdown();

    EXPECT_EQ(log, (Log{"start p", "start a", "start a1", "start b", "stop b", "stop a1", "stop a", "stop p"}));
}

TEST(Object, startupAndShutdownAreIdempotent)
{
    Log log;
    const auto obj = Make(log, "a");

    obj->startup();
    obj->startup();
    obj->shutdown();
    obj->shutdown();

    EXPECT_EQ(log, (Log{"start a", "stop a"}));
}

TEST(Object, shutdownOfNeverStartedObjectRunsNoHook)
{
    Log log;
    const auto obj = Make(log, "a");

    obj->shutdown();

    EXPECT_TRUE(std::empty(log));
    EXPECT_EQ(obj->getState(), State::Created);
}

TEST(Object, hooksSeeFlippedState)
{
    Log log;
    const auto obj = Make(log, "a");
    auto* raw = obj.get();
    State inStart{};
    State inStop{};
    obj->whenStarted([&] { inStart = raw->getState(); });
    obj->whenStopped([&] { inStop = raw->getState(); });

    obj->startup();
    obj->shutdown();

    EXPECT_EQ(inStart, State::Started);
    EXPECT_EQ(inStop, State::Shutdown);
}

TEST(Object, reentrantCallsFromHooksAreIgnored)
{
    Log log;
    const auto obj = Make(log, "a");
    auto* raw = obj.get();
    obj->whenStarted([&] { raw->startup(); });
    obj->whenStopped([&] { raw->shutdown(); });

    obj->startup();
    obj->shutdown();

    EXPECT_EQ(log, (Log{"start a", "stop a"}));
}

TEST(Object, addChildRejectsNull)
{
    const auto parent = std::make_shared<aspire::core::Object>();

    EXPECT_FALSE(parent->addChild(nullptr));
    EXPECT_TRUE(std::empty(parent->getChildren()));
}

TEST(Object, addChildRejectsObjectThatHasParent)
{
    Log log;
    const auto first = std::make_shared<aspire::core::Object>();
    const auto second = std::make_shared<aspire::core::Object>();
    const auto child = Make(log, "c");

    first->addChild(child);
    first->startup();
    second->startup();

    EXPECT_FALSE(second->addChild(child));
    EXPECT_TRUE(std::empty(second->getChildren()));
    EXPECT_EQ(child->getParent(), first);
    EXPECT_EQ(child->getState(), State::Started);
    EXPECT_EQ(log, (Log{"start c"}));
}

TEST(Object, addChildRejectsDuplicateOnSameParent)
{
    const auto parent = std::make_shared<aspire::core::Object>();
    const auto child = std::make_shared<aspire::core::Object>();

    EXPECT_TRUE(parent->addChild(child));
    EXPECT_FALSE(parent->addChild(child));
    EXPECT_EQ(std::size(parent->getChildren()), 1);
}

TEST(Object, addChildRejectsSelf)
{
    Log log;
    const auto obj = Make(log, "a");
    obj->startup();

    EXPECT_FALSE(obj->addChild(obj));
    EXPECT_TRUE(std::empty(obj->getChildren()));
    EXPECT_EQ(obj->getParent(), nullptr);
    EXPECT_EQ(log, (Log{"start a"}));
}

TEST(Object, addChildRejectsAncestor)
{
    Log log;
    const auto root = Make(log, "root");
    const auto a = Make(log, "a");
    const auto a1 = Make(log, "a1");
    root->addChild(a);
    a->addChild(a1);
    root->startup();

    // The root has no parent, so only the cycle check stops this.
    EXPECT_FALSE(a1->addChild(root));
    EXPECT_TRUE(std::empty(a1->getChildren()));
    EXPECT_EQ(root->getParent(), nullptr);

    // A non-root ancestor is already rejected for having a parent.
    EXPECT_FALSE(a1->addChild(a));
    EXPECT_TRUE(std::empty(a1->getChildren()));

    EXPECT_EQ(log, (Log{"start root", "start a", "start a1"}));
}

TEST(Object, addChildAcceptsObjectAfterRemove)
{
    Log log;
    const auto first = std::make_shared<aspire::core::Object>();
    const auto second = std::make_shared<aspire::core::Object>();
    const auto child = Make(log, "c");

    first->addChild(child);
    first->startup();
    second->startup();

    child->remove();
    EXPECT_TRUE(second->addChild(child));

    EXPECT_EQ(child->getParent(), second);
    EXPECT_EQ(child->getState(), State::Started);
    EXPECT_EQ(log, (Log{"start c", "stop c", "start c"}));
}

TEST(Object, addChildToStartedParentStartsFormedSubtree)
{
    Log log;
    const auto parent = std::make_shared<aspire::core::Object>();
    const auto child = Make(log, "c");
    const auto grandchild = Make(log, "g");

    child->addChild(grandchild);
    parent->startup();
    parent->addChild(child);

    EXPECT_EQ(log, (Log{"start c", "start g"}));
}

TEST(Object, addChildToUnstartedParentWaitsForParentStartup)
{
    Log log;
    const auto parent = std::make_shared<aspire::core::Object>();
    const auto child = Make(log, "c");

    parent->addChild(child);
    EXPECT_TRUE(std::empty(log));
    EXPECT_EQ(child->getState(), State::Created);

    parent->startup();
    EXPECT_EQ(log, (Log{"start c"}));
}

TEST(Object, childAddedInOnStartupStartsImmediately)
{
    LateChild::Starts = 0;
    Log log;
    const auto parent = Make(log, "p");
    auto* raw = parent.get();
    State seen{};
    int startsInHook{-1};
    parent->whenStarted(
        [&]
        {
            seen = raw->getOrCreateChild<LateChild>()->getState();
            startsInHook = LateChild::Starts;
        });

    parent->startup();

    EXPECT_EQ(seen, State::Started);
    EXPECT_EQ(startsInHook, 1);
    EXPECT_EQ(LateChild::Starts, 1);
}

TEST(Object, childAddedInOnShutdownIsNotStarted)
{
    Log log;
    const auto parent = Make(log, "p");
    auto* raw = parent.get();
    std::shared_ptr<LateChild> child;
    parent->whenStopped([&] { child = raw->getOrCreateChild<LateChild>(); });

    parent->startup();
    parent->shutdown();

    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->getState(), State::Created);
}

TEST(Object, removeShutsDownSubtreeBeforeDetaching)
{
    Log log;
    const auto parent = std::make_shared<aspire::core::Object>();
    const auto child = Make(log, "c");
    auto* raw = child.get();
    std::shared_ptr<aspire::core::Object> parentSeen;
    child->whenStopped([&] { parentSeen = raw->getParent(); });

    parent->addChild(child);
    parent->startup();
    child->remove();

    EXPECT_EQ(parentSeen, parent);
    EXPECT_EQ(child->getParent(), nullptr);
    EXPECT_EQ(child->getState(), State::Shutdown);
    EXPECT_TRUE(std::empty(parent->getChildren()));
}

TEST(Object, removedSubtreeStartsAgainWhenReAdded)
{
    Log log;
    const auto parent = std::make_shared<aspire::core::Object>();
    const auto child = Make(log, "c");
    const auto grandchild = Make(log, "g");

    child->addChild(grandchild);
    parent->addChild(child);
    parent->startup();
    child->remove();
    parent->addChild(child);

    EXPECT_EQ(log, (Log{"start c", "start g", "stop g", "stop c", "start c", "start g"}));
}

TEST(Object, removeWhenParentHoldsLastReference)
{
    Log log;
    const auto parent = std::make_shared<aspire::core::Object>();
    auto owned = Make(log, "c");
    auto* child = owned.get();
    parent->addChild(owned);
    // The parent now holds the last reference.
    owned.reset();
    parent->startup();

    // Must not touch members after the erase.
    child->remove();

    EXPECT_TRUE(std::empty(parent->getChildren()));
    EXPECT_EQ(log, (Log{"start c", "stop c"}));
}

TEST(Database, shutdownClearsIndexAndRestartReindexes)
{
    const auto database = std::make_shared<aspire::core::Database>();
    const auto data = std::make_shared<aspire::core::Data>();
    data->setName("d");
    database->addChild(data);

    database->startup();
    EXPECT_EQ(database->query("d"), data.get());

    database->shutdown();
    EXPECT_EQ(database->query("d"), nullptr);

    database->startup();
    EXPECT_EQ(database->query("d"), data.get());
}
