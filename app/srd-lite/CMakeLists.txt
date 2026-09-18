project(srd-lite)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)

target_sources(${PROJECT_NAME} PRIVATE
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        Character.ixx
        ViewWorld.ixx
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    aspire
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)