set(TROPOLINK_UI_SOURCES
    models/AppController.cpp
    models/AppController.h
    models/HelpContent.cpp
    models/HelpContent.h
    map/MapSources.cpp
    map/MapSources.h
    map/MapDownloader.cpp
    map/MapDownloader.h
    map/TileMapItem.cpp
    map/TileMapItem.h
    map/MapOverlayItem.cpp
    map/MapOverlayItem.h
    profile/ProfileItem.cpp
    profile/ProfileItem.h
    profile/CurveItem.cpp
    profile/CurveItem.h
    report/PdfReport.cpp
    report/PdfReport.h
)

qt_add_library(tropolink_ui STATIC ${TROPOLINK_UI_SOURCES})

qt_policy(SET QTP0004 NEW)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(tropolink_ui
    URI TropoLink
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/MapView.qml
        qml/ProfilePanel.qml
        qml/SitesPanel.qml
        qml/ResultsPanel.qml
        qml/SolverPanel.qml
        qml/WaterfallChart.qml
        qml/PanelSection.qml
        qml/NumberField.qml
        qml/StatusBar.qml
        qml/OnboardingTour.qml
        qml/HelpButton.qml
        qml/HelpDialog.qml
        qml/MapDownloadDialog.qml
)

target_include_directories(tropolink_ui PUBLIC ${CMAKE_SOURCE_DIR})
# qmltyperegistrar includes declared headers by basename.
target_include_directories(tropolink_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/map
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/profile
    ${CMAKE_CURRENT_SOURCE_DIR}/report
)
target_link_libraries(tropolink_ui
    PUBLIC tropolink_core Qt6::Quick Qt6::QuickControls2 Qt6::Svg
    PRIVATE unofficial::sqlite3::sqlite3 GDAL::GDAL GeographicLib::GeographicLib
)
target_compile_definitions(tropolink_ui PUBLIC
    TROPOLINK_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/resources/data"
)

if(NOT TROPOLINK_AIRGAP)
    target_link_libraries(tropolink_ui PRIVATE Qt6::Network ZLIB::ZLIB)
endif()

# i18n: Polish-first operator audience; English via source strings.
qt_add_translations(tropolink_ui
    TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/i18n/tropolink_pl.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

tropolink_set_warnings(tropolink_ui)
