#pragma once
// =============================================================================
// DataTypes.h  —  GeoIQ HTML Frontend Generator
// Defines all in-memory data structures read from GeoIQ outputs.
// GeoIQ.exe is NOT modified. This file is unique to the frontend generator.
// =============================================================================
#include <string>
#include <vector>
#include <cmath>

// ─── Constants ────────────────────────────────────────────────────────────────
static constexpr double NO_DATA_VAL   = -9999.0;
static constexpr double PAIR_RADIUS_KM = 5.0;   // Max distance for GeoIQ↔GeoHealth pairing

// CONTRACT-01: "SAFE" must NEVER appear in output.
// Use this string for background-level results.
static const std::string LOW_RISK_LABEL =
    "LOW RISK \xe2\x80\x94 Background level. Continue routine monitoring.";

// ─── WHO / Regulatory Thresholds (mg/L for water, ppm=mg/kg for soil) ────────
// These are the same values embedded in toxicology_database.json.
// Duplicated here for fast C++ comparisons inside ToxComparer.
struct ToxThreshold {
    std::string element;
    double who_guideline_water;   // mg/L
    double high_soil_threshold;   // ppm (mg/kg), NO_DATA_VAL if not applicable
    double critical_soil_threshold; // ppm, NO_DATA_VAL if not applicable
};

static const ToxThreshold TOX_THRESHOLDS[] = {
    // element,  WHO water (mg/L),  soil HIGH (ppm),  soil CRITICAL (ppm)
    { "As",  0.01,   10.0,    100.0  },  // WHO 2022 / EPA RSL
    { "Pb",  0.01,  400.0,   1200.0  },  // WHO 2017 / EPA Residential
    { "Hg",  0.006,   1.0,     10.0  },  // WHO 2017 / EPA RSL
    { "Cd",  0.003,  70.0,    200.0  },  // WHO 2022 / EPA RSL
    { "Cr",  0.05,  100.0,    500.0  },  // WHO 2022
    { "F",   1.5,   NO_DATA_VAL, NO_DATA_VAL }, // water only (ppm ≡ mg/L)
};
static constexpr int TOX_COUNT = 6;

// ─── Evidence Item (one element's stats for a prediction) ────────────────────
struct EvidenceItem {
    std::string element;
    std::string unit;       // "ppm", "ppb", "%", "pH", "mV"
    double      raw_value  = NO_DATA_VAL;
    double      local_z    = NO_DATA_VAL;
    double      regional_z = NO_DATA_VAL;
    double      alpha      = NO_DATA_VAL; // Singularity index (Cheng 2007)
    std::string singularity_state;        // "ENRICHED", "BACKGROUND", "DEPLETED", "INVALID"
    std::string formula;                  // e.g. "Z=(log10(7.9+0.001)-0.941)/0.105=-0.409"
};

// ─── Geology prediction for ONE target at ONE sample location ─────────────────
struct GeoTargetResult {
    std::string target;          // "GOLD", "COPPER_PORPHYRY", "ARSENIC_HAZARD", etc.
    std::string risk_level;      // "LOW", "MEDIUM", "MEDIUM_HIGH", "HIGH", "CRITICAL"
    double      score      = 0.0;
    double      confidence = 0.0;
    std::string annotation;
    std::vector<std::string> triggered_rules;
    bool        dispersion_detected  = false;
    double      source_bearing_deg   = NO_DATA_VAL;
    double      source_distance_km   = NO_DATA_VAL;
    double      spatial_consistency  = 0.0;
    std::string data_quality_note;
    std::vector<EvidenceItem> evidence;
};

// ─── One sample location's full geology prediction (all targets) ───────────────
struct GeoFeature {
    std::string sample_id;
    double lat = 0.0;
    double lon = 0.0;
    double depth_m    = NO_DATA_VAL;
    double elevation_m= NO_DATA_VAL;
    std::vector<GeoTargetResult> targets; // all targets scored for this sample

    // Raw element values (populated from predictions.geojson evidence arrays)
    // Keyed by element symbol → raw_value
    std::vector<std::pair<std::string, double>> raw_elements;
};

// ─── Health prediction for ONE target at ONE sample location ──────────────────
struct HealthTargetResult {
    std::string target;          // "ARSENIC_HAZARD", etc.
    std::string risk_level;
    double      score      = 0.0;
    std::string annotation;
    std::vector<std::string> triggered_rules;
    std::string data_quality_note;
    std::vector<EvidenceItem> evidence;
};

// ─── One health sample point (from audit_health.db) ──────────────────────────
struct HealthRecord {
    std::string sample_id;
    double lat = 0.0;
    double lon = 0.0;
    std::vector<HealthTargetResult> targets;

    // Worst (highest) risk level across all targets at this location
    std::string worst_risk_level = "LOW";
};

// ─── Paired GeoIQ + GeoHealth record (output of GeoHealthGrouper) ─────────────
struct GroupedRecord {
    std::string group_id;           // "GRP_0001"

    // Geology side (always present)
    std::string geo_sample_id;
    double geo_lat = 0.0;
    double geo_lon = 0.0;

    // Health side (optional — may have no pair within PAIR_RADIUS_KM)
    bool   has_health_pair    = false;
    std::string health_sample_id;
    double health_lat         = 0.0;
    double health_lon         = 0.0;
    double pair_distance_km   = NO_DATA_VAL;

    // All predictions
    std::vector<GeoTargetResult>    geology_targets;
    std::vector<HealthTargetResult> health_targets;

    // Raw element snapshot (union of geology + health evidence)
    std::vector<std::pair<std::string, double>> raw_elements; // {symbol, raw_value}

    // Derived severity for map display
    std::string worst_geology_risk = "LOW";
    std::string worst_health_risk  = "LOW";

    // Cross-reference mine data (populated from cross_referenced_anomalies.json)
    std::string nearest_mine_name;
    std::string nearest_mine_target;
    double      nearest_mine_dist_km = NO_DATA_VAL;
};

// ─── Mine cross-reference entry (from cross_referenced_anomalies.json) ────────
struct MineRef {
    std::string sample_id;
    double lat = 0.0;
    double lon = 0.0;
    std::string target;
    std::string risk_level;
    double score = 0.0;
    std::string mine_name;
    std::string mine_target;
    double distance_km = 0.0;
};

// ─── Risk level ordering helper (for "worst risk" comparisons) ───────────────
inline int riskOrder(const std::string& r) {
    if (r == "CRITICAL")    return 5;
    if (r == "HIGH")        return 4;
    if (r == "MEDIUM_HIGH") return 3;
    if (r == "MEDIUM")      return 2;
    if (r == "LOW")         return 1;
    return 0;
}

inline const std::string& worstRisk(const std::string& a, const std::string& b) {
    return (riskOrder(a) >= riskOrder(b)) ? a : b;
}

// ─── Colour mapping for map markers ──────────────────────────────────────────
inline std::string riskToColour(const std::string& risk) {
    if (risk == "CRITICAL")    return "#ff3333";
    if (risk == "HIGH")        return "#ff8800";
    if (risk == "MEDIUM_HIGH") return "#ffcc00";
    if (risk == "MEDIUM")      return "#ffe066";
    return "#22c55e"; // LOW — green
}

// Haversine distance in km
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    static constexpr double R = 6371.0;
    static constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;
    double dlat = (lat2 - lat1) * DEG2RAD;
    double dlon = (lon2 - lon1) * DEG2RAD;
    double a = std::sin(dlat/2)*std::sin(dlat/2)
             + std::cos(lat1*DEG2RAD)*std::cos(lat2*DEG2RAD)
             * std::sin(dlon/2)*std::sin(dlon/2);
    return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0-a));
}
