cmake_minimum_required(VERSION 3.14)
project(GeoIQ VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files grouped logically
set(SOURCES
    src/main.cpp
    
    # Utilities (Tier 0)
    src/utils/MathUtils.cpp
    src/utils/Statistics.cpp
    src/utils/Logger.cpp
    src/utils/Config.cpp
    
    # Data layer (Tier 1)
    src/data/CSVLoader.cpp
    src/data/Dataset.cpp
    
    # Spatial layer (Tier 2)
    src/spatial/KDTree.cpp
    src/spatial/IDW.cpp
    src/spatial/MovingWindow.cpp
    src/spatial/SpatialAnalysis.cpp
    
    # Engine (Tier 3)
    src/engine/RuleEngine.cpp
    src/engine/ScoreEngine.cpp
    src/engine/GridGenerator.cpp
    
    # Output layer (Tier 4)
    src/output/GeoJSONWriter.cpp
    src/output/HTMLWriter.cpp
    src/output/ReportWriter.cpp
    src/output/SQLiteWriter.cpp
    src/output/PMTilesWriter.cpp

    # SQLite static amalgamation
    src/utils/sqlite3.c
)

add_executable(GeoIQ ${SOURCES})

# Target private folder includes for clean #include directories
target_include_directories(GeoIQ PRIVATE src/)

# Link ZLIB for PMTiles tile compression
find_package(ZLIB REQUIRED)
target_link_libraries(GeoIQ PRIVATE ZLIB::ZLIB)

# Compiler flags for maximum optimization and warnings
if(MSVC)
    target_compile_options(GeoIQ PRIVATE /W4 /O2 /EHsc)
else()
    target_compile_options(GeoIQ PRIVATE -Wall -Wextra -O3)
endif()
