#include "utils/Config.h"
#include "utils/Logger.h"
#include "utils/Enums.h"
#include "data/CSVLoader.h"
#include "data/Dataset.h"
#include "spatial/MovingWindow.h"
#include "spatial/SpatialAnalysis.h"
#include "engine/RuleEngine.h"
#include "engine/ScoreEngine.h"
#include "engine/GridGenerator.h"
#include "output/GeoJSONWriter.h"
#include "output/HTMLWriter.h"
#include "output/ReportWriter.h"
#include "output/SQLiteWriter.h"
#include "output/PMTilesWriter.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <exception>

namespace {
    void printHelp() {
        std::cout << "GeoIQ Geological Intelligence Engine - v1.0\n"
                  << "Usage: GeoIQ.exe [options]\n"
                  << "Options:\n"
                  << "  --help, -h               Show this help message\n"
                  << "  --config <filepath>      Load custom configuration file (default: config.txt)\n"
                  << "  --input <filepath>       Override the input CSV file path\n";
    }

    GeoIQ::Logger::LogLevel parseLogLevel(const std::string& level) {
        if (level == "DEBUG") return GeoIQ::Logger::LogLevel::DEBUG_LVL;
        if (level == "INFO") return GeoIQ::Logger::LogLevel::INFO_LVL;
        if (level == "WARNING" || level == "WARN") return GeoIQ::Logger::LogLevel::WARN_LVL;
        if (level == "ERROR") return GeoIQ::Logger::LogLevel::ERROR_LVL;
        return GeoIQ::Logger::LogLevel::INFO_LVL;
    }
}

int main(int argc, char* argv[]) {
    // 1. CLI Parsing
    std::string config_path = "config.txt";
    std::string input_override = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                std::cerr << "Error: --config requires a filepath argument.\n";
                return 1;
            }
        } else if (arg == "--input") {
            if (i + 1 < argc) {
                input_override = argv[++i];
            } else {
                std::cerr << "Error: --input requires a filepath argument.\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            printHelp();
            return 1;
        }
    }

    // 2. Config Loading
    GeoIQ::Config config;
    if (std::filesystem::exists(config_path)) {
        if (!config.load(config_path)) {
            std::cerr << "Warning: Failed to load config file from '" << config_path << "'. Using defaults.\n";
        }
    } else {
        if (config_path != "config.txt") {
            std::cerr << "Error: Configuration file '" << config_path << "' does not exist.\n";
            return 1;
        }
    }

    if (!input_override.empty()) {
        config.input_file = input_override;
    }

    // 3. Initialize Logger & Output directory
    try {
        std::filesystem::create_directories(config.output_dir);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: Failed to create output directory " << config.output_dir << ": " << e.what() << "\n";
        return 1;
    }

    std::string log_filepath = config.output_dir + "/geoiq.log";
    GeoIQ::Logger::getInstance().setLevel(parseLogLevel(config.log_level));
    GeoIQ::Logger::getInstance().setLogFile(log_filepath);

    GeoIQ::Logger::getInstance().info("Main", "GeoIQ Pipeline initialized.");

    // 4. Load Data
    GeoIQ::Logger::getInstance().info("Main", "Loading CSV file: " + config.input_file);
    GeoIQ::CSVLoader loader;
    std::vector<GeoIQ::SamplePoint> raw_points;
    try {
        raw_points = loader.load(config.input_file);
    } catch (const std::exception& e) {
        GeoIQ::Logger::getInstance().error("Main", "Failed to load CSV: " + std::string(e.what()));
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // 5. Build Spatial Index & Dataset
    GeoIQ::Logger::getInstance().info("Main", "Assembling spatial index and calculating geochemical ratios...");
    GeoIQ::Dataset dataset(std::move(raw_points));

    // 6. Background Statistics & Singularity (Moving Window)
    GeoIQ::MovingWindow mw;
    mw.local_radius_km = config.local_radius_km;
    mw.regional_radius_km = config.regional_radius_km;

    const auto& points = dataset.getPoints();
    std::vector<std::array<GeoIQ::MovingWindow::Result, static_cast<size_t>(GeoIQ::ElementField::COUNT)>> real_mw_results;
    real_mw_results.resize(points.size());

    GeoIQ::Logger::getInstance().info("Main", "Computing moving window statistics for raw points...");
    for (size_t i = 0; i < points.size(); ++i) {
        real_mw_results[i] = mw.computeAll(points[i], dataset);
    }

    // 7. Spatial Analysis (Consistency & Dispersion)
    GeoIQ::SpatialAnalysis sa;

    // 8. Grid Generation & Virtual Point Evaluation (If enabled)
    std::vector<GeoIQ::SamplePoint> grid_points;
    std::vector<std::array<GeoIQ::MovingWindow::Result, static_cast<size_t>(GeoIQ::ElementField::COUNT)>> grid_mw_results;

    if (config.enable_grid) {
        GeoIQ::Logger::getInstance().info("Main", "Generating computational grid...");
        GeoIQ::GridGenerator generator;
        generator.cell_size_km = config.grid_cell_size_km;
        generator.boundary_padding_km = config.grid_boundary_padding_km;
        generator.idw_power = config.idw_power;
        generator.idw_search_radius_km = config.regional_radius_km;

        grid_points = generator.generate(dataset);
        grid_mw_results.resize(grid_points.size());

        GeoIQ::Logger::getInstance().info("Main", "Computing moving window statistics for grid cells...");
        for (size_t i = 0; i < grid_points.size(); ++i) {
            grid_mw_results[i] = mw.computeAll(grid_points[i], dataset);
        }
    }

    // 9. Rule Evaluation (Rule Engine)
    GeoIQ::RuleEngine engine;
    engine.enable_arsenic   = config.enable_arsenic;
    engine.enable_gold      = config.enable_gold;
    engine.enable_diamond   = config.enable_diamond;
    engine.enable_copper    = config.enable_copper;
    engine.enable_lithium   = config.enable_lithium;
    engine.enable_ree       = config.enable_ree;
    engine.enable_zinc_lead = config.enable_zinc_lead;
    engine.enable_gossan    = config.enable_gossan;

    GeoIQ::Logger::getInstance().info("Main", "Evaluating geochemical rules for raw points...");
    std::vector<std::vector<GeoIQ::RuleResult>> real_rule_results(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        GeoIQ::PointContext ctx;
        ctx.point = &points[i];
        ctx.mw = real_mw_results[i];

        // Compute consistency on primary pathfinder (As for Arsenic)
        std::vector<const GeoIQ::SamplePoint*> nbrs = dataset.queryRadius(points[i].lat, points[i].lon, config.local_radius_km);
        ctx.consistency = sa.spatialConsistency(points[i], GeoIQ::ElementField::As, nbrs);
        ctx.consistency_radius_km = config.local_radius_km;

        real_rule_results[i] = engine.evaluateAll(ctx);
    }

    std::vector<std::vector<GeoIQ::RuleResult>> grid_rule_results(grid_points.size());
    if (config.enable_grid) {
        GeoIQ::Logger::getInstance().info("Main", "Evaluating geochemical rules for grid cells...");
        for (size_t i = 0; i < grid_points.size(); ++i) {
            GeoIQ::PointContext ctx;
            ctx.point = &grid_points[i];
            ctx.mw = grid_mw_results[i];

            std::vector<const GeoIQ::SamplePoint*> nbrs = dataset.queryRadius(grid_points[i].lat, grid_points[i].lon, config.local_radius_km);
            ctx.consistency = sa.spatialConsistency(grid_points[i], GeoIQ::ElementField::As, nbrs);
            ctx.consistency_radius_km = config.local_radius_km;

            grid_rule_results[i] = engine.evaluateAll(ctx);
        }
    }

    // 10. Aggregation & Compilation (Score Engine & Dispersion)
    GeoIQ::ScoreEngine scorer;
    std::vector<GeoIQ::PredictionRecord> compiled_records;
    compiled_records.reserve(points.size() + grid_points.size());

    GeoIQ::Logger::getInstance().info("Main", "Compiling prediction records...");
    for (size_t i = 0; i < points.size(); ++i) {
        GeoIQ::SpatialAnalysis::DispersionResult disp;
        if (points[i].sample_type == GeoIQ::SampleType::STREAM_SEDIMENT || points[i].sample_type == GeoIQ::SampleType::SOIL) {
            disp = sa.detectDispersionTrain(points[i], GeoIQ::ElementField::As, dataset);
        }
        GeoIQ::PredictionRecord rec = scorer.compile(points[i], real_rule_results[i], disp);
        compiled_records.push_back(std::move(rec));
    }

    for (size_t i = 0; i < grid_points.size(); ++i) {
        GeoIQ::SpatialAnalysis::DispersionResult disp;
        GeoIQ::PredictionRecord rec = scorer.compile(grid_points[i], grid_rule_results[i], disp);
        compiled_records.push_back(std::move(rec));
    }

    // 11. Serialization (Writers)
    GeoIQ::Logger::getInstance().info("Main", "Serializing results...");

    std::string db_filename = config.is_geology ? "/audit.db" : "/audit_health.db";
    std::string pmtiles_filename = config.is_geology ? "/predictions.pmtiles" : "/predictions_health.pmtiles";
    std::string report_filename = config.is_geology ? "/report.txt" : "/report_health.txt";

    std::string db_path = config.output_dir + db_filename;
    std::string pmtiles_path = config.output_dir + pmtiles_filename;
    std::string report_path = config.output_dir + report_filename;

    // Write SQLite database
    GeoIQ::SQLiteWriter sqlite_writer;
    if (!sqlite_writer.write(db_path, compiled_records, config.is_geology)) {
        GeoIQ::Logger::getInstance().error("Main", "Failed to write SQLite database to " + db_path);
    }

    // Write PMTiles archive
    GeoIQ::PMTilesWriter pmtiles_writer;
    if (!pmtiles_writer.write(pmtiles_path, compiled_records, config.is_geology)) {
        GeoIQ::Logger::getInstance().error("Main", "Failed to write PMTiles vector archive to " + pmtiles_path);
    }

    GeoIQ::GeoJSONWriter geojson_writer;
    std::string geojson_path = config.output_dir + "/predictions.geojson";
    if (!geojson_writer.write(geojson_path, compiled_records)) {
        GeoIQ::Logger::getInstance().error("Main", "Failed to write GeoJSON to " + geojson_path);
    }

    GeoIQ::HTMLWriter html_writer;
    std::string html_path = config.output_dir + "/map.html";
    if (!html_writer.write(html_path, compiled_records)) {
        GeoIQ::Logger::getInstance().error("Main", "Failed to write HTML Map to " + html_path);
    } else {
        // Copy the logo image to the output directory so the HTML map can display it
        std::string logo_source = "geoiq_logo.png";
        std::string logo_dest = config.output_dir + "/geoiq_logo.png";
        if (std::filesystem::exists(logo_source)) {
            std::error_code ec;
            std::filesystem::copy_file(logo_source, logo_dest, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                GeoIQ::Logger::getInstance().warn("Main", "Failed to copy geoiq_logo.png to output directory: " + ec.message());
            } else {
                GeoIQ::Logger::getInstance().info("Main", "Copied geoiq_logo.png to output directory.");
            }
        } else {
            GeoIQ::Logger::getInstance().warn("Main", "geoiq_logo.png not found at project root. Skipping copy.");
        }
    }

    GeoIQ::ReportWriter report_writer;
    report_writer.input_filename = config.input_file;
    if (!report_writer.write(
        report_path,
        compiled_records,
        loader.getMissingColumns(),
        dataset.getExpandedRadiusCount(),
        loader.getSkippedCount())) {
        GeoIQ::Logger::getInstance().error("Main", "Failed to write text report to " + report_path);
    }

    // 12. Final Console Output
    std::cout << "\n==================================================\n"
              << "          GeoIQ Pipeline execution complete       \n"
              << "==================================================\n"
              << "  Raw samples loaded:       " << points.size() << "\n"
              << "  Grid cells generated:     " << grid_points.size() << "\n"
              << "  Skipped raw samples:      " << loader.getSkippedCount() << "\n"
              << "  Radius expansions:        " << dataset.getExpandedRadiusCount() << "\n"
              << "  GeoJSON output saved to:  " << geojson_path << "\n"
              << "  Interactive map saved to: " << html_path << "\n"
              << "  Report saved to:          " << report_path << "\n"
              << "==================================================\n\n";

    GeoIQ::Logger::getInstance().info("Main", "GeoIQ Pipeline execution complete.");
    return 0;
}
