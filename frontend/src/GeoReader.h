#pragma once
// =============================================================================
// GeoReader.h  —  GeoIQ HTML Frontend Generator
// Reads predictions.geojson and cross_referenced_anomalies.json,
// populates std::vector<GeoFeature> and std::vector<MineRef>.
// GeoIQ.exe is NOT touched — this file only reads its outputs.
// =============================================================================
#include "DataTypes.h"
#include "JsonParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// Read entire file into string
inline std::string readFileToString(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[GeoReader] Cannot open: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ─── Parse EvidenceItem from JSON object ─────────────────────────────────────
inline EvidenceItem parseEvidenceItem(const JValue& jv) {
    EvidenceItem ev;
    ev.element           = jv.get("element").asString();
    ev.unit              = jv.get("unit").asString("ppm");
    ev.raw_value         = jv.get("raw_value").asDouble(NO_DATA_VAL);
    ev.local_z           = jv.get("local_z").asDouble(NO_DATA_VAL);
    ev.regional_z        = jv.get("regional_z").asDouble(NO_DATA_VAL);
    ev.alpha             = jv.get("alpha").isNull() ? NO_DATA_VAL : jv.get("alpha").asDouble(NO_DATA_VAL);
    ev.singularity_state = jv.get("singularity_state").asString("INVALID");
    ev.formula           = jv.get("formula").asString();
    return ev;
}

// ─── Parse one GeoJSON feature → GeoTargetResult + updates GeoFeature ────────
// predictions.geojson has ONE feature per (sample_id × target).
// We group them by sample_id into GeoFeature.
inline void mergeFeatureIntoMap(
    const JValue& feature,
    std::unordered_map<std::string, GeoFeature>& featureMap)
{
    const JValue& geom  = feature.get("geometry");
    const JValue& props = feature.get("properties");

    if (geom.isNull() || props.isNull()) return;
    const JArray& coords = geom.get("coordinates").asArray();
    if (coords.size() < 2) return;

    double lon = coords[0].asDouble();
    double lat = coords[1].asDouble();
    std::string sid = props.get("sample_id").asString();
    if (sid.empty()) return;

    // Get or create GeoFeature for this sample_id
    GeoFeature& gf = featureMap[sid];
    if (gf.sample_id.empty()) {
        gf.sample_id   = sid;
        gf.lat         = lat;
        gf.lon         = lon;
        gf.depth_m     = props.get("depth_m").isNull()      ? NO_DATA_VAL : props.get("depth_m").asDouble(NO_DATA_VAL);
        gf.elevation_m = props.get("elevation_m").isNull()  ? NO_DATA_VAL : props.get("elevation_m").asDouble(NO_DATA_VAL);
    }

    // Build GeoTargetResult
    GeoTargetResult tr;
    tr.target      = props.get("target").asString();
    tr.risk_level  = props.get("risk_level").asString("LOW");
    tr.score       = props.get("score").asDouble(0.0);
    tr.confidence  = props.get("confidence").asDouble(0.0);
    tr.annotation  = props.get("annotation").asString();
    tr.data_quality_note = props.get("data_quality_note").asString();
    tr.spatial_consistency = props.get("spatial_consistency").asDouble(0.0);
    tr.dispersion_detected = props.get("dispersion_detected").asBool(false);

    // Triggered rules
    for (const JValue& r : props.get("triggered_rules").asArray()) {
        tr.triggered_rules.push_back(r.asString());
    }

    // Source bearing/distance
    const JValue& bearing = props.get("source_bearing_deg");
    const JValue& dist    = props.get("source_distance_km");
    tr.source_bearing_deg = bearing.isNull() ? NO_DATA_VAL : bearing.asDouble(NO_DATA_VAL);
    tr.source_distance_km = dist.isNull()    ? NO_DATA_VAL : dist.asDouble(NO_DATA_VAL);

    // Evidence array
    for (const JValue& ev : props.get("evidence").asArray()) {
        EvidenceItem item = parseEvidenceItem(ev);
        tr.evidence.push_back(item);

        // Also populate raw_elements on the parent GeoFeature (first time seen)
        if (!item.element.empty() && item.raw_value != NO_DATA_VAL) {
            bool found = false;
            for (auto& pair : gf.raw_elements) {
                if (pair.first == item.element) { found = true; break; }
            }
            if (!found) gf.raw_elements.push_back({item.element, item.raw_value});
        }
    }

    gf.targets.push_back(std::move(tr));
}

// ─── Load predictions.geojson → vector of GeoFeature ─────────────────────────
inline std::vector<GeoFeature> loadGeoFeatures(const std::string& geojsonPath) {
    std::string raw = readFileToString(geojsonPath);
    if (raw.empty()) return {};

    JValue root = parseJson(raw);
    if (!root.isObject()) {
        std::cerr << "[GeoReader] Invalid GeoJSON root\n";
        return {};
    }
    const JArray& features = root.get("features").asArray();

    std::unordered_map<std::string, GeoFeature> featureMap;
    featureMap.reserve(features.size());

    for (const JValue& f : features) {
        mergeFeatureIntoMap(f, featureMap);
    }

    // Flatten map → vector
    std::vector<GeoFeature> result;
    result.reserve(featureMap.size());
    for (auto& kv : featureMap) {
        result.push_back(std::move(kv.second));
    }

    // Compute worst_geology_risk per GeoFeature
    // (used later by grouper for map colouring)
    // Not stored in struct — computed by GroupedRecord builder

    std::cout << "[GeoReader] Loaded " << result.size()
              << " geology sample locations from " << geojsonPath << "\n";
    return result;
}

// ─── Load cross_referenced_anomalies.json → vector of MineRef ────────────────
inline std::vector<MineRef> loadMineRefs(const std::string& crossRefPath) {
    std::string raw = readFileToString(crossRefPath);
    if (raw.empty()) return {};

    JValue root = parseJson(raw);
    if (!root.isArray()) {
        std::cerr << "[GeoReader] cross_referenced_anomalies.json is not an array\n";
        return {};
    }

    std::vector<MineRef> result;
    result.reserve(root.asArray().size());

    for (const JValue& item : root.asArray()) {
        MineRef mr;
        mr.sample_id    = item.get("sample_id").asString();
        mr.lat          = item.get("lat").asDouble();
        mr.lon          = item.get("lon").asDouble();
        mr.target       = item.get("target").asString();
        mr.risk_level   = item.get("risk_level").asString();
        mr.score        = item.get("score").asDouble();
        mr.mine_name    = item.get("mine_name").asString();
        mr.mine_target  = item.get("mine_target").asString();
        mr.distance_km  = item.get("distance_km").asDouble();
        result.push_back(std::move(mr));
    }

    std::cout << "[GeoReader] Loaded " << result.size()
              << " mine cross-references from " << crossRefPath << "\n";
    return result;
}
