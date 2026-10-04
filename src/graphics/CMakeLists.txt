project(aspire-graphics)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Color.ixx
        DrawItem.ixx
        DrawList.ixx
        DrawRect.ixx
        DrawSprite.ixx
        DrawState.ixx
        DrawText.ixx
        Label.ixx
        Module.ixx
        Node.ixx
        Rect.ixx
        RenderBackend.ixx
        Renderer.ixx
        RenderService.ixx
        Sprite.ixx
        TileMap.ixx
        Transform.ixx
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    aspire-core
)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(test)
endif()
