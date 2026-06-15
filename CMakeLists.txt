cmake_minimum_required(VERSION 3.16)
project(InvestInsight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

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

add_executable(InvestInsight
    src/main.cpp
    src/ui/MainWindow.cpp
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
)

target_include_directories(InvestInsight
    PRIVATE
        src
)

target_link_options(InvestInsight PRIVATE -F${CMAKE_SOURCE_DIR}/third_party)
set_target_properties(InvestInsight PROPERTIES
    BUILD_RPATH "${CMAKE_SOURCE_DIR}/third_party"
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/third_party"
    BUILD_WITH_INSTALL_RPATH OFF
)

target_link_libraries(InvestInsight PRIVATE ${QT_LIBS})
