module;

#include <cassert>
#include <cstdlib>

export module aspire.core:engine;

import std;
import :event;
import :object;
import :service;

export namespace aspire::core
{
    /// @brief Root of the object tree, which drives its direct Service children each frame.
    ///
    /// A frame runs four phases over the started services, in child order: events, update, fixed steps, then render.
    /// Each phase works on a snapshot of the children, so a service added during a phase joins from the next phase,
    /// and a service removed or shut down during a phase gets no further calls.
    ///
    /// @warning Own an Engine with `std::make_shared`. Children link to it through `weak_from_this()`.
    class Engine : public Object
    {
    public:
        /// @brief Queues an event for the next frame's event phase.
        ///
        /// An event queued while events are being dispatched arrives in the following frame.
        ///
        /// @param x The event to queue.
        /// @pre Called on the main thread.
        auto enqueueEvent(Event x) -> void
        {
            events_.emplace_back(std::move(x));
        }

        /// @brief Runs one frame without blocking.
        ///
        /// For hosts that own the loop, such as SDL's main callbacks and tests. The host calls `startup()` before the
        /// first frame, calls `iterate()` while `running()`, and calls `shutdown()` after the last frame.
        ///
        /// Every queued event goes to every started service. An `EventWindow::Type::Closed` event also ends the loop,
        /// keeping the current exit code, before the services receive it.
        ///
        /// @param x Time since the previous frame. Clamped to the range zero to 50 ms. Pass zero for the first frame
        /// after the app resumes from the background.
        /// @pre `isStarted()`.
        /// @warning Not re-entrant: don't call it from a service.
        auto iterate(std::chrono::nanoseconds x) -> void
        {
            assert(isStarted());

            const auto elapsed = std::clamp(x, std::chrono::nanoseconds::zero(), MaxElapsed);
            const auto dt = std::chrono::duration<float>{elapsed}.count();
            const auto dtFixed = std::chrono::duration<float>{IntervalFixed}.count();

            dispatchEvents();
            forEachService([dt](Service& service) { service.update(dt); });

            accumulator_ += elapsed;

            while (accumulator_ >= IntervalFixed)
            {
                forEachService([dtFixed](Service& service) { service.updateFixed(dtFixed); });
                accumulator_ -= IntervalFixed;
            }

            forEachService([](Service& service) { service.render(); });
        }

        /// @brief Runs frames until `quit()` is called, for desktop hosts that let Engine own the loop.
        ///
        /// Starts the tree, calls `iterate()` with the measured frame time while `running()`, then shuts the tree down.
        ///
        /// @note `running()` isn't reset here, so a `quit()` from a startup hook ends the loop before the first frame.
        /// @return The exit code passed to `quit()`, or `EXIT_SUCCESS`.
        [[nodiscard]] auto run() -> int
        {
            startup();

            auto previous = std::chrono::steady_clock::now();

            while (running_)
            {
                const auto now = std::chrono::steady_clock::now();
                iterate(std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous));
                previous = now;
            }

            shutdown();
            return exitCode_;
        }

        /// @brief Ends the loop after the current frame.
        ///
        /// The rest of the frame still runs, and nothing shuts down here: `run()` or the host calls `shutdown()` once
        /// the loop ends.
        ///
        /// @param x The exit code that `exitCode()` and `run()` report.
        auto quit(int x = EXIT_SUCCESS) -> void
        {
            running_ = false;
            exitCode_ = x;
        }

        /// @brief Reports whether the loop should keep running.
        /// @return `false` once `quit()` has been called.
        [[nodiscard]] auto running() const -> bool
        {
            return running_;
        }

        /// @brief Reports the exit code for the host to return.
        /// @return The exit code passed to the last `quit()`, or `EXIT_SUCCESS`.
        [[nodiscard]] auto exitCode() const -> int
        {
            return exitCode_;
        }

    private:
        // Snapshot, so a service may add or remove services. Services shut down earlier in the phase are skipped.
        template <std::invocable<Service&> F>
        auto forEachService(const F& f) const -> void
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
            // Swap the queue out first: events enqueued by handlers arrive next frame.
            for (auto& event : std::exchange(events_, {}))
            {
                const auto* window = std::get_if<EventWindow>(&event);

                if (window != nullptr && window->type == EventWindow::Type::Closed)
                {
                    // Keep the current code: a service may have quit with a failure earlier in this frame.
                    quit(exitCode_);
                }

                forEachService([&event](Service& service) { service.event(event); });
            }
        }

        // Static constexpr members fall under clang-tidy's GlobalConstant naming: CamelCase, no suffix.
        static constexpr std::chrono::nanoseconds IntervalFixed{std::chrono::milliseconds{10}};
        static constexpr std::chrono::nanoseconds MaxElapsed{std::chrono::milliseconds{50}};

        std::vector<Event> events_;
        std::chrono::nanoseconds accumulator_{};
        int exitCode_{EXIT_SUCCESS};

        // Not reset by run(), so a quit() from a startup hook is honoured.
        bool running_{true};
    };
}
