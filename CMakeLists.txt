cmake_minimum_required(VERSION 3.16)
project(InvestInsight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

option(INVESTINSIGHT_WIN32_SUBSYSTEM "Build Windows executable with GUI subsystem" OFF)

set(APP_ICON_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows/app-icon.rc")
set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/assets/macos/app-icon.icns")

find_package(Qt5 COMPONENTS Core Widgets Network Concurrent QUIET)
if(Qt5_FOUND)
    set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network Qt5::Concurrent)
else()
    find_package(Qt6 COMPONENTS Core Widgets Network Concurrent REQUIRED)
    set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network Qt6::Concurrent)

    # Work around macOS SDKs where AGL.framework is removed.
    foreach(_qt6_tgt Qt6::Gui Qt6::GuiPrivate Qt6::Widgets Qt6::WidgetsPrivate
                     Qt6::Core Qt6::CorePrivate Qt6::OpenGL Qt6::OpenGLPrivate
                     Qt6::QCocoaIntegrationPlugin Qt6::QCocoaWindowPlugin)
        if(TARGET ${_qt6_tgt})
            foreach(_prop INTERFACE_LINK_LIBRARIES INTERFACE_LINK_OPTIONS
                          IMPORTED_LINK_DEPENDENT_LIBRARIES IMPORTED_LINK_INTERFACE_LIBRARIES)
                get_target_property(_vals ${_qt6_tgt} ${_prop})
                if(_vals)
                    list(FILTER _vals EXCLUDE REGEX "[Aa][Gg][Ll]")
                    set_target_properties(${_qt6_tgt} PROPERTIES ${_prop} "${_vals}")
                endif()
            endforeach()
            get_target_property(_cfgs ${_qt6_tgt} IMPORTED_CONFIGURATIONS)
            if(_cfgs)
                foreach(_cfg ${_cfgs})
                    foreach(_prop IMPORTED_LINK_DEPENDENT_LIBRARIES_${_cfg}
                                  IMPORTED_LINK_INTERFACE_LIBRARIES_${_cfg})
                        get_target_property(_vals ${_qt6_tgt} ${_prop})
                        if(_vals)
                            list(FILTER _vals EXCLUDE REGEX "[Aa][Gg][Ll]")
                            set_target_properties(${_qt6_tgt} PROPERTIES ${_prop} "${_vals}")
                        endif()
                    endforeach()
                endforeach()
            endif()
        endif()
    endforeach()
endif()

set(APP_SOURCES
    src/main.cpp
    src/ui/AppTheme.cpp
    src/ui/MainWindow.cpp
    src/ui/renderers/ChartRenderer.cpp
    src/core/InsightOrchestrator.cpp
    src/core/DataCollector.cpp
    src/core/SignalExtractor.cpp
    src/core/ImpactAnalyzer.cpp
    src/core/AdviceEngine.cpp
    src/core/SectorFetcher.cpp
    src/core/ReportComposer.cpp
    src/core/HttpClient.cpp
    src/core/AIAnalyzer.cpp
    src/core/TechIndicators.cpp
    src/core/StrategyEngine.cpp
    src/core/MarketContext.cpp
    src/core/RotationDetector.cpp
    src/core/TrendStageDetector.cpp
    src/core/TrendHealthAnalyzer.cpp
    src/core/BreadthAnalyzer.cpp
    src/core/OverheatDetector.cpp
    src/core/FlowStructureAnalyzer.cpp
    src/core/MarketRegimeDetector.cpp
    src/core/ExplainabilityEngine.cpp
    src/providers/RealFinanceNewsProvider.cpp
    src/providers/RealLinkageProvider.cpp
    assets/app.qrc
)

if(WIN32)
    list(APPEND APP_SOURCES "${APP_ICON_WINDOWS}")
endif()

if(APPLE)
    list(APPEND APP_SOURCES "${APP_ICON_MACOS}")
    set_source_files_properties("${APP_ICON_MACOS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

set(APP_EXECUTABLE_OPTIONS MACOSX_BUNDLE)
if(WIN32 AND INVESTINSIGHT_WIN32_SUBSYSTEM)
    list(APPEND APP_EXECUTABLE_OPTIONS WIN32)
endif()

add_executable(InvestInsight ${APP_EXECUTABLE_OPTIONS}
    ${APP_SOURCES}
)

target_include_directories(InvestInsight
    PRIVATE
        src
)

if(MSVC)
    target_compile_options(InvestInsight PRIVATE /utf-8)
endif()
set_target_properties(InvestInsight PROPERTIES
    BUILD_RPATH "${CMAKE_SOURCE_DIR}/third_party"
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/third_party"
    BUILD_WITH_INSTALL_RPATH OFF
    MACOSX_BUNDLE_ICON_FILE "app-icon.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "InvestInsight"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.investinsight.app"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
)

if(APPLE)
    target_link_options(InvestInsight PRIVATE -F${CMAKE_SOURCE_DIR}/third_party)
endif()

target_link_libraries(InvestInsight PRIVATE ${QT_LIBS})

add_executable(InvestInsightUiSmoke
    tests/ui/AppThemeSmoke.cpp
    tests/ui/ChartRendererSmoke.cpp
    src/ui/AppTheme.cpp
    src/ui/renderers/ChartRenderer.cpp
    src/core/TechIndicators.cpp
)

target_include_directories(InvestInsightUiSmoke
    PRIVATE
        src
)

if(MSVC)
    target_compile_options(InvestInsightUiSmoke PRIVATE /utf-8)
endif()

target_link_libraries(InvestInsightUiSmoke PRIVATE ${QT_LIBS})
