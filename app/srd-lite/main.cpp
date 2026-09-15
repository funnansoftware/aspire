#include <raylib.h>
#include <nameof.hpp>

import std;
import aspire;

auto main() -> int
try
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::raylib::Texture>();
    factory.registerObject<aspire::raylib::TileMap>();

    auto engine = std::make_shared<aspire::core::Engine>();

    constexpr auto windowWidth = 1280;
    constexpr auto windowHeight = 720;

    aspire::raylib::Window window{windowWidth, windowHeight, "Not D&D Like"};
    window.setScale(Vector2{.x = 4.0F, .y = 4.0F});

    auto object = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/levels/level_1.json");
    auto* map = dynamic_cast<aspire::raylib::TileMap*>(object.get());
    map->setPosition(Vector2{.x = (window.width() / 4.0F) * 0.5F, .y = 0});
    engine->addChild(object);

    auto character = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/characters/hero.json");
    auto* texture = dynamic_cast<aspire::raylib::Texture*>(character.get());
    texture->setPosition(Vector2{.x = (window.width() / 4.0F) * 0.55F, .y = 50});
    engine->addChild(character);

    while (!aspire::raylib::Window::shouldClose())
    {
        // Render Game View.
        const auto bg = Color{.r = 71, .g = 45, .b = 60, .a = 255};
        window.beginDraw(bg);

        for (const auto& [index, drawable] : std::views::enumerate(engine->getChildren<aspire::raylib::Drawable>()))
        {
            drawable->draw();
        }

        window.endDraw();
    }

    return EXIT_SUCCESS;
}
catch (...)
{
    return EXIT_FAILURE;
}
