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
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui
    src/ui/DriverCardWidget.h
    src/ui/DriverCardWidget.cpp
    src/ui/CategorySectionWidget.h
    src/ui/CategorySectionWidget.cpp
    src/scanner/DriverScanner.cpp
    src/scanner/DriverScanner.h
    src/models/DriverInfo.h
    src/models/DeviceCategory.h
    src/formatter/DriverDateFormatter.cpp
    src/formatter/DriverDateFormatter.h
    src/formatter/DriverStatusFormatter.cpp
    src/formatter/DriverStatusFormatter.h
    src/formatter/DriverVersionFormatter.cpp
    src/formatter/DriverVersionFormatter.h
    src/formatter/DriverImportanceFormatter.cpp
    src/formatter/DriverImportanceFormatter.h
    src/scoring/DriverImportanceEvaluator.cpp
    src/scoring/DriverImportanceEvaluator.h
    src/grouper/CategoryGrouper.cpp
    src/grouper/CategoryGrouper.h
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
    src/formatter
    src/grouper
    src/models
    src/scanner
    src/scoring
    src/ui
)

target_link_libraries(DriverVisualiser
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    setupapi
    cfgmgr32
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
