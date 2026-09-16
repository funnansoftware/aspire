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
        }

        [[nodiscard]] auto getPosition() const noexcept -> Vector2
        {
            return position_;
        }

        auto setRotation(Vector2 rotation) noexcept -> void
        {
            rotation_ = rotation;
        }

        [[nodiscard]] auto getRotation() const noexcept -> Vector2
        {
            return rotation_;
        }

        auto setScale(Vector2 scale) noexcept -> void
        {
            scale_ = scale;
        }

        [[nodiscard]] auto getScale() const noexcept -> Vector2
        {
            return scale_;
        }

        [[nodiscard]] auto getTransform() const noexcept -> Matrix
        {
            Matrix transform = MatrixIdentity();
            transform = MatrixMultiply(transform, MatrixTranslate(position_.x, position_.y, 0.0F));
            transform = MatrixMultiply(transform, MatrixRotateXYZ({.x = rotation_.x, .y = rotation_.y, .z = 0.0F}));
            transform = MatrixMultiply(transform, MatrixScale(scale_.x, scale_.y, 1.0F));
            return transform;
        }

    protected:
        auto onRenderPre() const -> void override
        {
            rlPushMatrix();
            rlMultMatrixf(MatrixToFloat(getTransform()));
        }

        auto onRenderPost() const -> void override
        {
            rlPopMatrix();
        }

    private:
        Vector2 position_{.x = 0.0F, .y = 0.0F};
        Vector2 rotation_{.x = 0.0F, .y = 0.0F};
        Vector2 scale_{.x = 1.0F, .y = 1.0F};
    };
}