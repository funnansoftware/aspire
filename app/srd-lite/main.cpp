#include <raylib.h>
#include <memory>
#include <nameof.hpp>

import std;
import aspire;
import sl.character;
import sl.viewworld;

// NOLINTBEGIN

auto main() -> int
try
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::core::DataService>();
    factory.registerObject<sl::Character>();
    factory.registerObject<aspire::raylib::Texture>();
    factory.registerObject<aspire::raylib::TileMap>();
    factory.registerObject<sl::ViewWorld>();
    factory.registerObject<aspire::core::Database>();
    factory.registerObject<aspire::core::Data>();

    auto engine = std::make_shared<aspire::core::Engine>();

    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/database.json"));
    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/config/Game.json"));

    constexpr auto windowWidth = 1280;
    constexpr auto windowHeight = 720;

    auto window = std::make_shared<aspire::raylib::Window>(windowWidth, windowHeight, "Not D&D Like");
    window->setScale(Vector2{.x = 4.0F, .y = 4.0F});
    engine->addChild(window);

    auto world = std::make_shared<sl::ViewWorld>();
    world->setPosition(Vector2{.x = (window->width() / 4.0F) * 0.5F, .y = 0});
    window->addChild(world);

    auto level = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/levels/level_1.json");
    world->addChild(level);

    auto character = std::dynamic_pointer_cast<aspire::raylib::Node>(
        aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/characters/goblin.json"));
    character->setPosition(Vector2{.x = 50, .y = 50});
    world->addChild(character);
    window->addChild(std::make_shared<aspire::raylib::Text>());

    return engine->run();
}
catch (...)
{
    return EXIT_FAILURE;
}

// NOLINTEND
