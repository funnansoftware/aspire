#include <raylib.h>
#include <memory>
#include <nameof.hpp>

import std;
import aspire;

// NOLINTBEGIN

auto main() -> int
try
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::raylib::Texture>();
    factory.registerObject<aspire::raylib::TileMap>();

    auto engine = std::make_shared<aspire::core::Engine>();

    constexpr auto windowWidth = 1280;
    constexpr auto windowHeight = 720;

    auto window = std::make_shared<aspire::raylib::Window>(windowWidth, windowHeight, "Not D&D Like");
    window->setScale(Vector2{.x = 4.0F, .y = 4.0F});
    engine->addChild(window);

    auto object = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/levels/level_1.json");
    auto* map = dynamic_cast<aspire::raylib::TileMap*>(object.get());
    map->setPosition(Vector2{.x = (window->width() / 4.0F) * 0.5F, .y = 0});
    window->addChild(object);

    auto character = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/characters/hero.json");
    auto* texture = dynamic_cast<aspire::raylib::Texture*>(character.get());
    texture->setPosition(Vector2{.x = (window->width() / 4.0F) * 0.55F, .y = 50});
    window->addChild(character);

    return engine->run();
}
catch (...)
{
    return EXIT_FAILURE;
}

// NOLINTEND
