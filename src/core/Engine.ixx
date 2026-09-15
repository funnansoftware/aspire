module;

#include <cstdlib>

export module aspire.core.engine;

import std;
import aspire.core.object;
import aspire.core.overloaded;
import aspire.core.event;

export namespace aspire::core
{
    class Engine : public aspire::core::Object
    {
    public:
        auto enqueueEvent(aspire::core::Event e) -> void
        {
            events_.emplace_back(std::move(e));
        }

        [[nodiscard]] auto run() -> int
        {
            aspire::core::Event e = aspire::core::EventStartup{};
            event(e);

            start_ = std::chrono::steady_clock::now();
            running_ = true;

            while (running_)
            {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
                accumulate_ += elapsed;
                start_ = now;

                for (auto& evt : events_)
                {
                    event(evt);
                }

                events_.clear();

                // Updates.
                e = aspire::core::EventUpdate{.elapsed = elapsed};
                event(e);

                // Physics.
                auto count = 0;
                while (accumulate_ >= IntervalFixed && count < FrameLimit)
                {
                    e = aspire::core::EventUpdateFixed{.elapsed = IntervalFixed};
                    event(e);
                    accumulate_ -= IntervalFixed;
                    ++count;
                }

                // Rendering.
                e = aspire::core::EventRender{.elapsed = elapsed};
                event(e);
            }

            return EXIT_SUCCESS;
        }

        auto quit() -> void
        {
            running_ = false;
        }

    protected:
        auto onEvent(aspire::core::Event& e) -> void override
        {
            std::visit(
                aspire::core::Overloaded{
                    [this](aspire::core::EventWindow& e)
                    {
                        switch (e.type)
                        {
                            case aspire::core::EventWindow::Type::Closed:
                                quit();
                                break;
                            default:
                                break;
                        }
                    },
                    [](auto&&) {},
                },
                e);
        }

    private:
        static constexpr int FrameLimit{5};
        static constexpr std::chrono::steady_clock::duration IntervalFixed{std::chrono::milliseconds(10)};

        std::vector<aspire::core::Event> events_;

        std::chrono::steady_clock::time_point start_;
        std::chrono::steady_clock::duration accumulate_;
        bool running_{false};
    };
}
