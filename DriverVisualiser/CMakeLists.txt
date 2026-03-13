cmake_minimum_required(VERSION 3.16)

project(DriverVisualiser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_library(SETUP_LIB setupapi)

set(PROJECT_SOURCES
    # Main
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui

    # Core - Scanner
    src/core/scanner/DriverScanner.cpp
    src/core/scanner/DriverScanner.h
    src/core/scanner/ErrorLogReader.cpp
    src/core/scanner/ErrorLogReader.h
    src/core/scanner/SystemInfoCollector.cpp
    src/core/scanner/SystemInfoCollector.h

    # Core - Models
    src/core/models/DeviceCategory.h
    src/core/models/DriverInfo.h
    src/core/models/ErrorLogEntry.h
    src/core/models/HealthFlag.h
    src/core/models/SystemInfo.h

    # Core - Scoring
    src/core/scoring/DriverImportanceEvaluator.cpp
    src/core/scoring/DriverImportanceEvaluator.h
    src/core/scoring/HealthScoreEvaluator.cpp
    src/core/scoring/HealthScoreEvaluator.h
    src/core/scoring/SystemHealthSummary.cpp
    src/core/scoring/SystemHealthSummary.h

    # Core - Reporting
    src/core/reporting/ReportGenerator.cpp
    src/core/reporting/ReportGenerator.h
    src/core/reporting/formatters/IReportFormatter.h
    src/core/reporting/formatters/TextReportFormatter.cpp
    src/core/reporting/formatters/TextReportFormatter.h
    src/core/reporting/formatters/HtmlReportFormatter.cpp
    src/core/reporting/formatters/HtmlReportFormatter.h

    # Utils - Formatters
    src/utils/formatters/DriverDateFormatter.cpp
    src/utils/formatters/DriverDateFormatter.h
    src/utils/formatters/DriverImportanceFormatter.cpp
    src/utils/formatters/DriverImportanceFormatter.h
    src/utils/formatters/DriverStatusFormatter.cpp
    src/utils/formatters/DriverStatusFormatter.h
    src/utils/formatters/DriverVersionFormatter.cpp
    src/utils/formatters/DriverVersionFormatter.h

    # Utils - Grouper
    src/utils/grouper/CategoryGrouper.cpp
    src/utils/grouper/CategoryGrouper.h
    src/utils/grouper/CategoryProcessor.cpp
    src/utils/grouper/CategoryProcessor.h

    # Utils - Mappers
    src/utils/mappers/ClassNameMapper.cpp
    src/utils/mappers/ClassNameMapper.h

    # Utils - Theme
    src/utils/AppTheme.cpp
    src/utils/AppTheme.h

    # UI - Widgets
    src/ui/widgets/CategorySectionWidget.cpp
    src/ui/widgets/CategorySectionWidget.h
    src/ui/widgets/DriverCardWidget.cpp
    src/ui/widgets/DriverCardWidget.h
    src/ui/widgets/ErrorLogIndicatorWidget.cpp
    src/ui/widgets/ErrorLogIndicatorWidget.h
    src/ui/widgets/FlagIndicatorWidget.cpp
    src/ui/widgets/FlagIndicatorWidget.h
    src/ui/widgets/IssueListWidget.cpp
    src/ui/widgets/IssueListWidget.h
    src/ui/widgets/ScoreRingWidget.cpp
    src/ui/widgets/ScoreRingWidget.h
    src/ui/widgets/SearchFilterBar.cpp
    src/ui/widgets/SearchFilterBar.h
    src/ui/widgets/ReportNotification.cpp
    src/ui/widgets/ReportNotification.h

    # UI - Popups
    src/ui/popups/DriverInfoPopup.cpp
    src/ui/popups/DriverInfoPopup.h
    src/ui/popups/ErrorLogPopupWidget.cpp
    src/ui/popups/ErrorLogPopupWidget.h
    src/ui/popups/FilterPopupWidget.cpp
    src/ui/popups/FilterPopupWidget.h
    src/ui/popups/FlagPopupWidget.cpp
    src/ui/popups/FlagPopupWidget.h

    # UI - Dashboard
    src/ui/dashboard/DashboardWidget.cpp
    src/ui/dashboard/DashboardWidget.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DriverVisualiser
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(DriverVisualiser SHARED ${PROJECT_SOURCES})
    else()
        add_executable(DriverVisualiser ${PROJECT_SOURCES})
    endif()
endif()

target_include_directories(DriverVisualiser PRIVATE
    src
    src/core/models
    src/core/scanner
    src/core/scoring
    src/core/reporting
    src/core/reporting/formatters
    src/utils
    src/utils/formatters
    src/utils/grouper
    src/utils/mappers
    src/ui
    src/ui/widgets
    src/ui/popups
    src/ui/dashboard
)

target_link_libraries(DriverVisualiser
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    setupapi
    cfgmgr32
    wevtapi
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.DriverVisualiser)
endif()
set_target_properties(DriverVisualiser PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)

include(GNUInstallDirs)
install(TARGETS DriverVisualiser
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DriverVisualiser)
endif()

add_custom_command(TARGET DriverVisualiser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resources/device_class_mappings.json
        $<TARGET_FILE_DIR:DriverVisualiser>/device_class_mappings.json
    COMMENT "Copying device_class_mappings.json to output directory"
)