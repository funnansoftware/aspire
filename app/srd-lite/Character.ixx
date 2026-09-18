

export module sl.character;

import std;
import aspire.core.object;

export namespace sl
{
    class Character : public aspire::core::Object
    {
    public:
        Character()
        {
            registerProperty("position", position_);
            registerProperty("str", str_);
            registerProperty("dex", dex_);
            registerProperty("con", con_);
            registerProperty("int", int_);
            registerProperty("wis", wis_);
            registerProperty("cha", cha_);
            registerProperty("hp", hp_);
        }

        auto setPosition(std::array<int, 2> x) noexcept -> void
        {
            position_ = x;
        }

        [[nodiscard]] auto getPosition() const noexcept
        {
            return position_;
        }

        auto setStr(int x) noexcept -> void
        {
            str_ = x;
        }

        [[nodiscard]] auto getStr() const noexcept -> int
        {
            return str_;
        }

        auto setDex(int x) noexcept -> void
        {
            dex_ = x;
        }

        [[nodiscard]] auto getDex() const noexcept -> int
        {
            return dex_;
        }

        auto setCon(int x) noexcept -> void
        {
            con_ = x;
        }

        [[nodiscard]] auto getCon() const noexcept -> int
        {
            return con_;
        }

        auto setInt(int x) noexcept -> void
        {
            int_ = x;
        }

        [[nodiscard]] auto getInt() const noexcept -> int
        {
            return int_;
        }

        auto setWis(int x) noexcept -> void
        {
            wis_ = x;
        }

        [[nodiscard]] auto getWis() const noexcept -> int
        {
            return wis_;
        }

        auto setCha(int x) noexcept -> void
        {
            cha_ = x;
        }

        [[nodiscard]] auto getCha() const noexcept -> int
        {
            return cha_;
        }

        auto setHp(int x) noexcept -> void
        {
            hp_ = x;
        }

        [[nodiscard]] auto getHp() const noexcept -> int
        {
            return hp_;
        }

    private:
        std::array<int, 2> position_{};
        int str_{};
        int dex_{};
        int con_{};
        int int_{};
        int wis_{};
        int cha_{};
        int hp_{};
    };
}