project(srd-lite)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)

find_package(raylib CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)