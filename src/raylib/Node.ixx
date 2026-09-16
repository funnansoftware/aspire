module;

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

export module aspire.raylib.node;

import std;
import aspire.core.object;

export namespace aspire::raylib
{
    class Node : public aspire::core::Object
    {
    public:
        auto setPosition(Vector2 position) noexcept -> void
        {
            position_ = position;
            transformDirtyLocal_ = true;
            transformDirtyGlobal_ = true;
        }

        [[nodiscard]] auto getPosition() const noexcept -> Vector2
        {
            return position_;
        }

        auto setRotation(Vector2 rotation) noexcept -> void
        {
            rotation_ = rotation;
            transformDirtyLocal_ = true;
            transformDirtyGlobal_ = true;
        }

        [[nodiscard]] auto getRotation() const noexcept -> Vector2
        {
            return rotation_;
        }

        auto setScale(Vector2 scale) noexcept -> void
        {
            scale_ = scale;
            transformDirtyLocal_ = true;
            transformDirtyGlobal_ = true;
        }

        [[nodiscard]] auto getScale() const noexcept -> Vector2
        {
            return scale_;
        }

        [[nodiscard]] auto getTransformLocal() const noexcept -> Matrix
        {
            if (transformDirtyLocal_)
            {
                transformLocal_ = MatrixIdentity();
                transformLocal_ = MatrixMultiply(transformLocal_, MatrixTranslate(position_.x, position_.y, 0.0F));
                transformLocal_ = MatrixMultiply(transformLocal_, MatrixRotateXYZ({.x = rotation_.x, .y = rotation_.y, .z = 0.0F}));
                transformLocal_ = MatrixMultiply(transformLocal_, MatrixScale(scale_.x, scale_.y, 1.0F));
                transformDirtyLocal_ = false;
            }

            return transformLocal_;
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        [[nodiscard]] auto getTransformGlobal() const noexcept -> Matrix
        {
            if (transformDirtyGlobal_)
            {
                auto parent = getParent<aspire::raylib::Node>();

                if (parent)
                {
                    transformGlobal_ = MatrixMultiply(parent->getTransformGlobal(), getTransformLocal());
                }
                else
                {
                    transformGlobal_ = getTransformLocal();
                }

                transformDirtyGlobal_ = false;
            }

            return transformGlobal_;
        }

        [[nodiscard]] auto mapFromGlobal(Vector2 x) const noexcept -> Vector2
        {
            const auto invTransform = MatrixInvert(getTransformGlobal());
            return Vector2Transform(x, invTransform);
        }

        [[nodiscard]] auto mapFromLocal(Vector2 x) const noexcept -> Vector2
        {
            return Vector2Transform(x, getTransformLocal());
        }

    protected:
        auto onRenderPre() const -> void override
        {
            rlPushMatrix();
            rlMultMatrixf(MatrixToFloat(getTransformLocal()));
        }

        auto onRenderPost() const -> void override
        {
            rlPopMatrix();
        }

    private:
        Vector2 position_{.x = 0.0F, .y = 0.0F};
        Vector2 rotation_{.x = 0.0F, .y = 0.0F};
        Vector2 scale_{.x = 1.0F, .y = 1.0F};
        mutable Matrix transformLocal_{};
        mutable Matrix transformGlobal_{};
        mutable bool transformDirtyLocal_{true};
        mutable bool transformDirtyGlobal_{true};
    };
}