# Services as direct children of Engine

## Status
- **Commit 1 (build wiring): implemented and verified, not yet committed.** The changes are the `vcpkg.json`, root `CMakeLists.txt`, `src/*/CMakeLists.txt`, `src/*/test/CMakeLists.txt` and `.github/README.md` edits described below. Verified locally with MSVC debug and release, clang-cl debug and wasm debug: 39/39 tests pass, and `clang-format-check` is clean.
- **Commit 2 (Object lifecycle): next.** Nothing implemented yet.
- **Commit 3 (services and the frame loop): not started.**
- **Not verified locally:** `src/` has never been compiled on Linux clang/libc++, GCC 15 (SteamOS) or macOS. Watch those CI jobs after pushing commit 1.
- **Windows tip:** if an old clang-cl build dir fails with a Ninja assertion (`edge && !edge->outputs_ready()`), delete that build dir and reconfigure. A fresh dir builds fine.
- **Commit style:** no AI co-author trailers on commits or PR descriptions.

## Context
Every frame, `Object` walks the whole tree for `event`, `update`, `updateFixed` and `render`. At each level it copies the child vector and calls six empty virtuals (`src/core/Object.ixx:187-282`).

Goals:
- Objects that need per-frame work opt in; everything else pays nothing.
- Leave room for a scene graph under a graphics service whose nodes queue render commands.

**Why this replaces the sigslot plan.** That plan let any Object at any depth subscribe to Engine. To rebuild what the tree walk gave for free (ordering, handled early-exit, detached objects going quiet), it needed:
- an `Engine*` back-pointer and a cross-partition `class Engine;` forward declaration
- `connections_`/`track()` on every Object
- a recursive `shutdown()` with no matching hook, so `onStartup` ran again on re-attach with no paired teardown
- a `connectEvent` re-forwarding sigslot's overload set
- global `group_id` ordering whose within-group order shuffles on disconnect (signal.hpp `clean()` swap-pops)

That is about 10 rules, all for `onUpdate`/`onUpdateFixed`, which nothing has ever overridden (`git log --all -S 'onUpdate(float'` finds only 81fa1bf). It also flattened render, which a scene graph needs nested, and kept a blocking `run()` that SDL main callbacks and wasm can't drive (evford, `app/evford/main.cpp:3`).

## Decisions
- **Opting in means deriving from `aspire::core::Service` and being a direct child of Engine.** Ownership is the registry: order is child order, and lifetime and removal follow the tree. There are no connections and no back-pointer.
- **`Object` keeps name, properties, the tree and an explicit lifecycle.** All per-frame code leaves it.
- **The lifecycle is a three-state machine on `Object` (decided; details in commit 2).**
  - `Created → Started → Shutdown`, and a `Shutdown` object can be started again.
  - `startup()` runs `onStartup()` parent-first. `shutdown()` runs `onShutdown()` in reverse tree order.
  - `ReadJson` still returns a fully formed, un-started subtree (`src/parser/Json.ixx:84-96`). It starts when it joins a Started parent, or when its root starts.
- **Deeper objects tick through the service that owns them.** For scene nodes that is the future GraphicsService (see Deferred).
- **The host drives Engine.** `iterate(std::chrono::nanoseconds)` runs one non-blocking frame. `run()` is a thin desktop loop over it. Injected time makes fixed-step tests exact.
- **Each phase works on a snapshot of the direct Service children.** A service added mid-phase joins from the next phase. A service removed or shut down mid-phase is skipped immediately: the loop only calls Started services.
- **Events go in child order until handled, and `EventWindow::Closed` quits first.** This matches today's behaviour. UI-before-world priority belongs inside GraphicsService routing.
- **A frame advances at most 50 ms.** `MaxElapsed` is its own constant (evford's `MaxFrameSeconds`), and elapsed is clamped to `[0, MaxElapsed]`. This replaces today's millisecond truncation (`Engine.ixx:32-34` drops every sub-millisecond frame, so a busy loop never reaches a fixed step) and the `FrameLimit` step cap.
- **Failures leave through a return value, not exceptions.** `quit(int exitCode)` and `exitCode()`. Emscripten disables exception catching by default (`settings.js:727`), so exceptions can't cross the SDL callbacks on the web.
- **No palsigslot in core, and no `std::move_only_function`.** Virtual hooks need neither. emsdk's libc++ lacks `move_only_function` (`version:513` has it commented out), and so do the Linux and macOS clang presets that use libc++. palsigslot is for app-level domain signals and lands with its first consumer (decided).
- **Three commits, in this order:** build wiring, then the Object lifecycle, then services and the frame loop.

How the earlier approvals map:
- **Render flattened:** `Service::onRender` only, with no pre/post hooks. Nesting lives inside the GraphicsService walk.
- **Engine skips handled events:** the `IsHandled` check in `dispatchEvents`.
- **`remove()` disconnects the subtree:** it now shuts the detached subtree down, and frame loops skip a removed service immediately.
- **Wire `src/` and deps:** kept, minus palsigslot, which waits for its first consumer (decided).
- **Approach and event order (decided):** services as direct children of Engine; events go in forward child order.

## Commit 1: build wiring (no behaviour change)
- `vcpkg.json`: add `"nameof"` and `"nlohmann-json"`.
- `CMakeLists.txt`: `add_subdirectory(src)` before `app`, and update the "legacy src/" comment.
- `src/{core,parser,string}/CMakeLists.txt`: wrap `add_subdirectory(test)` in `if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)`, as `app/evford/CMakeLists.txt` does.
- `src/*/test/CMakeLists.txt`: add `include(GoogleTest)`.
- Fix only trivial breakage in parser and string, and report anything larger. If GCC 15 (steamos) can't compile the partitions, add an option to skip `src/` on that preset rather than blocking evford.

## Commit 2: Object lifecycle (start here)
Touches `Object.ixx`, `Database.ixx`, one line in `Engine.ixx`, and `Object.test.cpp`.

### States
`Object::State`, a nested `enum class` with `std::uint8_t` underlying type, like `EventWindow::Type`. Read it with `getState()`; `isStarted()` is `state == Started`.

| State | Meaning |
|---|---|
| `Created` | Never started. |
| `Started` | Live. `addChild()` starts new children immediately. |
| `Shutdown` | Was started, then torn down. `startup()` can start it again. |

`Created → Started → Shutdown → Started → …`

### Rules
1. **`startup()` is parent-first.** If the object is already `Started` it does nothing. Otherwise it sets `Started` before the hook, as `started_` does today, runs `onStartup()`, then starts a snapshot of the children. A hook that calls `startup()` again is therefore a no-op.
2. **`shutdown()` runs in reverse tree order.** Only a `Started` object acts; any other state does nothing. It sets `Shutdown` first, shuts down a snapshot of the children last to first, then runs `onShutdown()`. Hooks therefore see `Shutdown`, a repeat call from a hook is a no-op, and `addChild()` starts nothing during teardown. For `p{a{a1}, b}` the order is start `p, a, a1, b` and shutdown `b, a1, a, p`.
3. **Hooks come in pairs.** `onShutdown()` runs once for each object that was `Started`. A never-started object runs no `onShutdown()`.
4. **`addChild()` starts the child whenever `this` is `Started`**, including while this object's own `onStartup()` is running, as today. A child added by `getOrCreateChild<T>()` inside `onStartup()` therefore starts at once, before the rest of that hook runs. While `Created` or `Shutdown`, a child waits until this object starts. That is how a formed `ReadJson` subtree starts parent-first.
5. **`remove()` shuts down the removed subtree, then detaches it.** Hooks still see their parent, because it runs while attached. Re-adding it later starts a new cycle. It runs synchronously: code after `remove()` inside the removed object's own hook sees a shut-down object.
6. **Failure policy:**
   - `onStartup()` may throw. As today, the object stays `Started` and the exception propagates. A later `shutdown()` on the root unwinds everything that started, and runs `onShutdown()` for the object whose `onStartup()` threw, so hooks must tolerate a partly initialized object.
   - `onShutdown()` is `noexcept`. Teardown can't leave an object half torn down, and a throwing override terminates.
7. **No hooks from destructors.** A virtual call in a destructor never reaches a derived class (Core Guidelines C.82), and `weak_from_this()` has already expired by then, so children couldn't reach their parent. The owner shuts the tree down while it is still alive: `Engine::run()`, `SDL_AppQuit`, or `remove()`.
8. **An object has one parent.** `addChild()` returns `false` and changes nothing if `x` is null or already has a parent, this one included. To move an object, call `remove()` first. That shuts it down under the old parent, and it starts again if the new parent is `Started`. So an attached object sits in exactly one `children_` list, and exactly one parent drives its lifecycle. Attach un-started objects: `addChild()` doesn't change the state of an object that is already started.
9. **`shutdown()` on one attached object** leaves it attached but `Shutdown`, and `startup()` brings it back. Frame loops (commit 3) skip objects that aren't `Started`.

### `src/core/Object.ixx`
```cpp
enum class State : std::uint8_t { Created, Started, Shutdown };

[[nodiscard]] auto getState() const -> State { return state_; }
[[nodiscard]] auto isStarted() const -> bool { return state_ == State::Started; }

// Returns true if added. Callers that built the child themselves may ignore the result.
auto addChild(std::shared_ptr<Object> x) -> bool
{
    if (x == nullptr || x->parent_.lock() != nullptr)
    {
        return false;   // One parent only: remove() it first to move it.
    }

    x->parent_ = weak_from_this();
    auto& child = children_.emplace_back(std::move(x));

    if (state_ == State::Started)
    {
        child->startup();   // Late start: the child's subtree is already formed.
    }

    return true;
}

// NOLINTNEXTLINE(misc-no-recursion)
auto startup() -> void
{
    if (state_ == State::Started)
    {
        return;
    }

    state_ = State::Started;   // Before the hook: a re-entrant call returns, and addChild() starts new children.
    onStartup();

    // Snapshot: hooks may add or remove children.
    const auto children = children_;

    for (const auto& child : children)
    {
        child->startup();
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
auto shutdown() -> void
{
    if (state_ != State::Started)
    {
        return;
    }

    state_ = State::Shutdown;   // Before the children: addChild() starts nothing during teardown.

    const auto children = children_;

    for (const auto& child : children | std::views::reverse)
    {
        child->shutdown();
    }

    onShutdown();
}

auto remove() -> void
{
    const auto parent = parent_.lock();

    if (parent == nullptr)
    {
        return;
    }

    const auto self = shared_from_this();   // Erasing below may drop the parent's reference to this.
    shutdown();                             // While still attached, so hooks can reach the parent.
    parent_.reset();
    std::erase(parent->children_, self);    // Last: touch no members afterwards.
}

protected:
virtual auto onStartup() -> void {}
virtual auto onShutdown() noexcept -> void {}

private:
State state_{State::Created};   // replaces bool started_
```
- The old `remove()` called `parent_.reset()` after an erase that could destroy `this`, a use-after-free when the parent held the last reference.
- `remove()` on an object with no parent does nothing, as before.
- `addChild` now returns `bool` without `[[nodiscard]]`, so existing call sites (`Json.ixx`, `getOrCreateChild`, tests) compile unchanged. A parent that was destroyed leaves an expired `parent_`, which counts as no parent.

### `src/core/Database.ixx`
- Add `auto onShutdown() noexcept -> void override { data_.clear(); }`. A removed and re-added Database then re-indexes cleanly instead of keeping stale entries.

### `src/core/Engine.ixx`
- `run()` calls `shutdown()` after the loop, before `return EXIT_SUCCESS;`. Without it, nothing pairs `onStartup` at exit. Commit 3 rewrites `run()` and keeps this call.

### Tests
A small `Recorder` test object appends `start <name>` and `stop <name>` to a shared log from its hooks.
- `Object.test.cpp`:
  - `stateStartsCreated`
  - `startupShutdownStartupAgain`: `Created → Started → Shutdown → Started`, and the log shows two start/stop cycles
  - `startupIsParentFirstShutdownIsReverse`: `p{a{a1}, b}` gives start `p, a, a1, b` and stop `b, a1, a, p`
  - `startupAndShutdownAreIdempotent`
  - `shutdownOfNeverStartedObjectRunsNoHook`: state stays `Created`
  - `hooksSeeFlippedState`: `Started` inside `onStartup`, `Shutdown` inside `onShutdown`
  - `reentrantCallsFromHooksAreIgnored`: `startup()` inside `onStartup` and `shutdown()` inside `onShutdown` each do nothing
  - `addChildRejectsNull`
  - `addChildRejectsObjectThatHasParent`: a second parent returns `false` and gets no child, even when it is `Started`. The object stays under the first parent with its state unchanged.
  - `addChildRejectsDuplicateOnSameParent`
  - `addChildAcceptsObjectAfterRemove`: moving an object between parents through `remove()`, which shuts it down and restarts it under a `Started` new parent
  - `addChildToStartedParentStartsFormedSubtree`: child and grandchild built first, as `ReadJson` does
  - `addChildToUnstartedParentWaitsForParentStartup`
  - `childAddedInOnStartupStartsImmediately`: via `getOrCreateChild`, started exactly once
  - `childAddedInOnShutdownIsNotStarted`
  - `removeShutsDownSubtreeBeforeDetaching`: the hook still sees its parent; afterwards the parent is null and the state is `Shutdown`
  - `removedSubtreeStartsAgainWhenReAdded`
  - `removeWhenParentHoldsLastReference`
- Database coverage (`Object.test.cpp` or a new `Database.test.cpp`): `shutdownClearsIndexAndRestartReindexes`.
- The old `events` test stays until commit 3, which removes the per-frame API it uses.

## Commit 3: services and the frame loop

### `src/core/Service.ixx` (new)
Imports `:event` and `:object`, and never imports `:engine`. Add it to `Module.ixx` and the core `CXX_MODULES` list.
- `class Service : public Object` uses the NVI pattern Object uses today:
  - Public `event(Event&)`, `update(float)`, `updateFixed(float)` and `render()` forward to protected virtual `onEvent`, `onUpdate`, `onUpdateFixed` and `onRender`.
  - `onRender` is non-const, because a graphics service fills its queue there.
  - Migrating a class is just a base-class change, since the hook names stay the same.
- `auto ContainsService(const Object& x) -> bool` is a free function that reports whether any descendant of `x` is a Service. Engine uses it to catch nested services, which would otherwise silently never tick.

### `src/core/Engine.ixx`
Delete the `onEvent` override and the `start_` and `accumulate_` members. Import `:service`.
```cpp
// Tree root. Drives its DIRECT Service children each frame: events, update, fixed steps, render.
// Own it with std::make_shared: children link to it through weak_from_this().
class Engine : public aspire::core::Object
{
public:
    auto enqueueEvent(aspire::core::Event x) -> void;   // main thread only

    // One non-blocking frame, for SDL_AppIterate, run() and tests. The host calls startup()
    // first and shutdown() after the last frame. Not re-entrant.
    auto iterate(std::chrono::nanoseconds elapsed) -> void
    {
        assert(isStarted());

        const auto clamped = std::clamp(elapsed, std::chrono::nanoseconds::zero(), MaxElapsed);
        const auto dt = std::chrono::duration<float>{clamped}.count();
        const auto dtFixed = std::chrono::duration<float>{IntervalFixed}.count();

        dispatchEvents();
        forEachService([dt](Service& x) { x.update(dt); });

        accumulator_ += clamped;

        while (accumulator_ >= IntervalFixed)
        {
            forEachService([dtFixed](Service& x) { x.updateFixed(dtFixed); });
            accumulator_ -= IntervalFixed;
        }

        forEachService([](Service& x) { x.render(); });
    }

    [[nodiscard]] auto run() -> int;   // startup(); steady_clock loop over iterate() while running(); shutdown(); return exitCode()
    auto quit(int exitCode = EXIT_SUCCESS) -> void;   // ends the loop after this frame; doesn't shut down mid-frame
    [[nodiscard]] auto running() const -> bool;
    [[nodiscard]] auto exitCode() const -> int;

protected:
    auto onStartup() -> void override
    {
        // The tree is formed but children haven't started yet.
        // A Service below a direct child never ticks: fail loudly in debug builds.
        // Checked at start only; a later addChild of such a subtree isn't caught.
        assert(std::ranges::none_of(getChildren(), [](const auto& x) { return ContainsService(*x); }));
    }

private:
    // Snapshot so hooks may add or remove services. Additions join from the next phase.
    // Services shut down earlier in this phase (removed or disabled by a sibling) are skipped.
    template <typename F>
    auto forEachService(F&& f) -> void
    {
        for (const auto& service : getChildren<Service>())
        {
            if (service->isStarted())
            {
                f(*service);
            }
        }
    }

    auto dispatchEvents() -> void
    {
        // Events enqueued by handlers arrive next frame. This also fixes today's
        // UB when the queue is appended to during the range-for.
        for (auto& event : std::exchange(events_, {}))
        {
            if (const auto* x = std::get_if<EventWindow>(&event); x != nullptr && x->type == EventWindow::Type::Closed)
            {
                quit();
            }

            forEachService([&event](Service& x) { if (!IsHandled(event)) { x.event(event); } });
        }
    }

    static constexpr std::chrono::nanoseconds IntervalFixed{std::chrono::milliseconds{10}};
    static constexpr std::chrono::nanoseconds MaxElapsed{std::chrono::milliseconds{50}};

    std::vector<aspire::core::Event> events_;
    std::chrono::nanoseconds accumulator_{};
    int exitCode_{EXIT_SUCCESS};
    bool running_{true}; // not reset by run(), so quit() from onStartup is honoured
};
```
- Add `#include <cassert>` to the global module fragment, next to `<cstdlib>`.
- Constants likely need CamelCase: clang-tidy probably treats `static constexpr` members as GlobalConstant, and the `.clang-tidy` `PrivateStaticMember*` keys look unrecognised. Confirm with `clang-tidy-diff` and name them to match.

### `src/core/Object.ixx`
- Delete `event`, `update`, `updateFixed`, `render`, the six per-frame `on*` virtuals, and `import :event; import :overloaded;`.

### `src/core/Event.ixx`
- Add `[[nodiscard]] auto IsHandled(const Event&) -> bool`, the visit currently inlined in `Object::event`, made null-safe for `unique_ptr<EventUser>`. Import `:overloaded`.

### Tests (Engine tests create the Engine with `std::make_shared`, call `startup()`, then drive `iterate(...)` with injected time)
- `Object.test.cpp`: delete `events` (115-136).
- `Engine.test.cpp` (replaces the stub):
  - `servicesTickInChildOrder`
  - `plainChildrenStartButDoNotTick`
  - `containsServiceFindsNestedService`
  - `runStartsThenShutsDown`: `run()` with a service that quits on its first update. The log shows startup, update, render, shutdown.
  - `shutDownServiceIsSkipped`: a service shut down by a sibling mid-phase gets no more calls
  - `fixedStepsFollowElapsed`: 25 ms gives 2 steps, then +5 ms gives 3 in total
  - `elapsedIsClamped`: 1 s gives 5 steps and dt 0.05; negative gives 0
  - `eventsStopOnceHandled`
  - `closedQuitsAndServicesSeeIt`
  - `eventEnqueuedDuringDispatchArrivesNextFrame`
  - `serviceAddedDuringUpdateJoinsNextPhase`
  - `serviceRemovedBySiblingIsSkipped`
  - `quitExitCodeIsReturnedByRun`

**Unchanged:** Data, DataService, ObjectFactory, Property, the parser, evford, and srd-lite (still dead code).

## Host contract (documented on `iterate`, used by later ports)
| SDL callback | Engine |
|---|---|
| `SDL_AppInit` | `make_shared<Engine>()`, add services and `ReadFile` trees, `startup()`, then sample the clock |
| `SDL_AppEvent` | Handle `QUIT`/`CLOSE_REQUESTED` directly: `quit()` and return `SDL_APP_SUCCESS`. Otherwise translate main-thread input and `enqueueEvent`. Lifecycle flags stay atomic, as in evford. |
| `SDL_AppIterate` | Skip while backgrounded or minimized, and pass 0 ns on the first frame after. Then `iterate(now - previous)`. While `running()` return CONTINUE; otherwise `exitCode() == 0` gives SUCCESS and anything else FAILURE. |
| `SDL_AppQuit` | `shutdown()`, which runs while the services' SDL objects are still valid, then release the Engine. |

Pause belongs to the simulation service, or to an Engine time scale later, not to the host.

## Deferred (not in this change; the srd-lite SDL3 port drives it)
- **`aspire.graphics`** (no SDL). Each frame runs collect → sort → submit.
  - **No GoF visitor on nodes.** Node types are open: any app registers them with `ObjectFactory`. A visitor would have to name types it can't see, and it would bring back the import cycle. Visitor-style dispatch is used only on the closed set of draw primitives.
  - **Ownership.** `GraphicsService : Service` owns `Node : Object` roots (JSON and the factory still work), a `Renderer`, and a `DrawList`. The `DrawList` is cleared and reused each frame, so it keeps its capacity and doesn't allocate per frame.
  - **Collect.** A free function `Collect(node, state, list)`:
    - carries the world transform and the intersected clip down the tree
    - calls `node.draw(list, state)`, a virtual on `Node` that appends zero or more items
    - recurses over the `getChildren()` span using `dynamic_cast<const Node*>`, not `getChildren<Node>()`, which allocates on every call

    ViewWorld's scissor becomes its node's clip.
  - **`DrawItem`** has:
    - `layer`: the group (world, UI, debug)
    - `order`: y for a y-sorted world layer, 0 where tree order should decide
    - `sequence`: the traversal index
    - the baked transform and a clip index
    - `primitive`: a `std::variant<Sprite, Rect, Text, ...>`
  - **Sort.** `std::ranges::sort` with the projection `std::tuple{layer, order, sequence}`. `sequence` keeps the order deterministic without `stable_sort`. Baking the transform and clip into each item makes sorting safe, because there are no push/pop pairs for it to split.
  - **Submit.** The backend calls `std::visit` with `Overloaded` on each primitive, the same idiom `Event` uses. It changes clip and texture state only when they differ from the previous item. A second backend such as SDL_GPU is just another visitor, with no change to nodes or primitives.
  - **Update.** Nodes opt in with a `process_` flag (JSON `"process": true`). `GraphicsService::onUpdate` checks the flag during its own walk over a snapshot, so there's no registry and Node never names GraphicsService.
  - **Input.** Pointer buttons and scroll are hit-tested front to back until handled. Pointer moves go to every node, or as enter/leave. Keyboard goes to the focused node first.
- **`aspire.sdl`.** SDL_Renderer backend and `ToEvent(const SDL_Event&)`.
- **Optional follow-up.** Port evford onto Engine as one service. It needs `setIntervalFixed(1/120 s)`.
- **Rename `DataService`** (`src/core/DataService.ixx`). It's a plain Object, and the name will mislead once `Service` exists.
- **palsigslot, when it lands.**
  - Use `sigslot::signal`, never `signal_st`.
  - Track with `weak_from_this()` and capture `this`, never an owning `shared_ptr`.
  - Don't rely on order within a group.
  - Link `Pal::Sigslot` only to the target that declares the signal. It brings `Threads::Threads`, so check the wasm link.

## Verification
1. `cmake --preset x64-windows-msvc-release`, then `cmake --build --preset x64-windows-msvc-release`, then `ctest --preset x64-windows-msvc-release`. The core, parser, string and evford tests pass.
2. Run the same three steps with `x64-windows-clang-debug` (clang-cl).
3. `cmake --preset wasm32-emscripten-emcc-debug` and build. The libraries compile; tests are off on that preset.
4. In the devcontainer, as `lint.yml` does: `cmake --preset x64-linux-clang-debug`, then build the `clang-format-check` and `clang-tidy-diff` targets.
5. After commit 1, watch the steamos (GCC 15) and macOS CI runs, since they're the first builds of the partitioned `src/` there.
6. After commit 2, the lifecycle tests pass on MSVC and clang-cl, and `clang-tidy-diff` is clean on the new enum and the `noexcept` hook.
