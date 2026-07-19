qt_add_executable(TropoLink WIN32
    main.cpp
)

# tropolink_uiplugin is the auto-generated backing plugin of the static QML module;
# linking it registers the TropoLink QML module with the engine.
target_link_libraries(TropoLink PRIVATE tropolink_ui tropolink_uiplugin tropolink_core
    Qt6::Quick Qt6::QuickControls2)
tropolink_set_warnings(TropoLink)

# Deploy runtime data (ITU maps, libraries, bundled reference terrain) next to the
# executable.
add_custom_command(TARGET TropoLink POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/data $<TARGET_FILE_DIR:TropoLink>/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/terrain $<TARGET_FILE_DIR:TropoLink>/data/terrain
)
