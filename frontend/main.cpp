// =============================================================================
// main.cpp  —  GeoIQ HTML Frontend Generator
// Standalone program. GeoIQ.exe is NOT modified.
//
// Usage:
//   HTMLWriter.exe [--geo-path <path>] [--health-db <path>]
//                  [--cross-ref <path>] [--tox-db <path>] [--out-dir <path>]
//
// Defaults (relative to exe location, assumed to be run from GeoIQ/ root):
//   --geo-path   output/predictions.geojson
//   --health-db  output/audit_health.db
//   --cross-ref  output/cross_referenced_anomalies.json
//   --tox-db     data/toxicology_database.json
//   --out-dir    frontend/output/
// =============================================================================

#include "src/DataTypes.h"
#include "src/JsonParser.h"
#include "src/GeoReader.h"
#include "src/HealthReader.h"
#include "src/GeoHealthGrouper.h"
#include "src/HtmlCommon.h"
#include "src/HtmlTier1.h"
#include "src/HtmlTier2.h"
#include "src/HtmlTier3.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>

// ─── Parse CLI arguments ──────────────────────────────────────────────────────
struct Config {
    std::string geoPath    = "output/predictions.geojson";
    std::string healthDb   = "output/audit_health.db";
    std::string crossRef   = "output/cross_referenced_anomalies.json";
    std::string toxDb      = "data/toxicology_database.json";
    std::string outDir     = "frontend/output";
};

Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc - 1; ++i) {
        std::string arg = argv[i];
        if (arg == "--geo-path"  && i+1 < argc) { cfg.geoPath  = argv[++i]; }
        if (arg == "--health-db" && i+1 < argc) { cfg.healthDb = argv[++i]; }
        if (arg == "--cross-ref" && i+1 < argc) { cfg.crossRef = argv[++i]; }
        if (arg == "--tox-db"    && i+1 < argc) { cfg.toxDb    = argv[++i]; }
        if (arg == "--out-dir"   && i+1 < argc) { cfg.outDir   = argv[++i]; }
    }
    return cfg;
}

// ─── Save intermediate geo_health_groups.json ─────────────────────────────────
void saveGroupsJson(const std::vector<GroupedRecord>& groups, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "[main] Cannot write: " << path << "\n"; return; }
    f << "[\n";
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i) f << ",\n";
        f << serializeGroupToJS(groups[i], 3);
    }
    f << "\n]\n";
    std::cout << "[main] Saved groups: " << path << " (" << groups.size() << " records)\n";
}

// ─── Entry point ─────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║     GeoIQ HTML Frontend Generator v1.0           ║\n";
    std::cout << "║     Standalone — GeoIQ.exe is NOT modified       ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    Config cfg = parseArgs(argc, argv);

    // ── 1. Create output directory ────────────────────────────────────────────
    try {
        std::filesystem::create_directories(cfg.outDir);
        std::cout << "[main] Output directory: " << cfg.outDir << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[main] Warning — could not create output dir: " << e.what() << "\n";
    }

    // ── 2. Load geology features from predictions.geojson ─────────────────────
    std::cout << "\n[Step 1/5] Loading geology predictions...\n";
    std::vector<GeoFeature> geoFeatures = loadGeoFeatures(cfg.geoPath);
    if (geoFeatures.empty()) {
        std::cerr << "[main] ERROR: No geology features loaded. Check path: " << cfg.geoPath << "\n";
        return 1;
    }

    // ── 3. Load health records from audit_health.db ────────────────────────────
    std::cout << "\n[Step 2/5] Loading health predictions from SQLite...\n";
    std::vector<HealthRecord> healthRecords = loadHealthRecords(cfg.healthDb);
    // Note: empty health records is OK — geology-only mode still works

    // ── 4. Load mine cross-references ─────────────────────────────────────────
    std::cout << "\n[Step 3/5] Loading mine cross-references...\n";
    std::vector<MineRef> mineRefs = loadMineRefs(cfg.crossRef);

    // ── 5. Load toxicology database as raw JSON string ────────────────────────
    std::cout << "\n[Step 4/5] Loading toxicology database...\n";
    std::string toxJsonRaw = readFileToString(cfg.toxDb);
    if (toxJsonRaw.empty()) {
        std::cerr << "[main] Warning: Toxicology DB not found at " << cfg.toxDb
                  << " — Tier 1 illness detail will be unavailable.\n";
        toxJsonRaw = "{\"disclaimer\":\"\",\"toxicants\":[]}";
    } else {
        std::cout << "[main] Toxicology DB loaded (" << toxJsonRaw.size() << " bytes)\n";
    }

    // ── 6. Pair GeoIQ + GeoHealth by Haversine ────────────────────────────────
    std::cout << "\n[Step 5/5] Pairing GeoIQ + GeoHealth locations (≤"
              << PAIR_RADIUS_KM << " km)...\n";
    std::vector<GroupedRecord> groups = buildGroupedRecords(geoFeatures, healthRecords, mineRefs);

    // Save intermediate file
    saveGroupsJson(groups, cfg.outDir + "/geo_health_groups.json");

    // ── 7. Write 3 HTML files ─────────────────────────────────────────────────
    std::cout << "\n── Writing HTML files ──────────────────────────────\n";

    writeTier1(cfg.outDir + "/tier1_public.html",   groups, toxJsonRaw);
    writeTier2(cfg.outDir + "/tier2_advanced.html",  groups);
    writeTier3(cfg.outDir + "/tier3_auditor.html",   groups, mineRefs);

    std::cout << "\n══════════════════════════════════════════════════\n";
    std::cout << "  Done! Open these files in your browser:\n";
    std::cout << "  → " << cfg.outDir << "/tier1_public.html\n";
    std::cout << "  → " << cfg.outDir << "/tier2_advanced.html\n";
    std::cout << "  → " << cfg.outDir << "/tier3_auditor.html\n";
    std::cout << "══════════════════════════════════════════════════\n";
    return 0;
}
