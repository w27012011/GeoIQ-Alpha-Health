#pragma once
// =============================================================================
// GeoHealthGrouper.h  —  GeoIQ HTML Frontend Generator
// Pairs each GeoIQ geology sample with the nearest GeoHealth health sample
// within PAIR_RADIUS_KM using the Haversine formula.
// Builds GroupedRecord objects for all 3 tiers.
// =============================================================================
#include "DataTypes.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <limits>

// ─── Build worst risk summary across all targets for a feature ────────────────
inline std::string computeWorstRisk(const std::vector<GeoTargetResult>& targets) {
    std::string worst = "LOW";
    for (const auto& t : targets) {
        if (riskOrder(t.risk_level) > riskOrder(worst)) worst = t.risk_level;
    }
    return worst;
}
inline std::string computeWorstHealthRisk(const std::vector<HealthTargetResult>& targets) {
    std::string worst = "LOW";
    for (const auto& t : targets) {
        if (riskOrder(t.risk_level) > riskOrder(worst)) worst = t.risk_level;
    }
    return worst;
}

// ─── Attach mine cross-reference data to GroupedRecords ──────────────────────
inline void attachMineRefs(
    std::vector<GroupedRecord>& groups,
    const std::vector<MineRef>& mineRefs)
{
    // For each group, find best (closest) mine reference matching its geo_sample_id
    for (auto& grp : groups) {
        double bestDist = std::numeric_limits<double>::max();
        for (const auto& mr : mineRefs) {
            if (mr.sample_id == grp.geo_sample_id && mr.distance_km < bestDist) {
                bestDist = mr.distance_km;
                grp.nearest_mine_name   = mr.mine_name;
                grp.nearest_mine_target = mr.mine_target;
                grp.nearest_mine_dist_km= mr.distance_km;
            }
        }
    }
}

// ─── Main grouping function ───────────────────────────────────────────────────
inline std::vector<GroupedRecord> buildGroupedRecords(
    const std::vector<GeoFeature>&   geoFeatures,
    const std::vector<HealthRecord>& healthRecords,
    const std::vector<MineRef>&      mineRefs)
{
    std::vector<GroupedRecord> groups;
    groups.reserve(geoFeatures.size());

    int groupCounter = 1;

    for (const GeoFeature& gf : geoFeatures) {
        GroupedRecord grp;

        // Format group ID
        std::ostringstream idss;
        idss << "GRP_" << std::setfill('0') << std::setw(4) << groupCounter++;
        grp.group_id      = idss.str();
        grp.geo_sample_id = gf.sample_id;
        grp.geo_lat       = gf.lat;
        grp.geo_lon       = gf.lon;
        grp.geology_targets = gf.targets;
        grp.raw_elements    = gf.raw_elements;
        grp.worst_geology_risk = computeWorstRisk(gf.targets);

        // Find nearest health record within PAIR_RADIUS_KM
        double bestDist = std::numeric_limits<double>::max();
        const HealthRecord* bestHR = nullptr;
        for (const HealthRecord& hr : healthRecords) {
            double d = haversineKm(gf.lat, gf.lon, hr.lat, hr.lon);
            if (d < PAIR_RADIUS_KM && d < bestDist) {
                bestDist = d;
                bestHR   = &hr;
            }
        }

        if (bestHR) {
            grp.has_health_pair    = true;
            grp.health_sample_id   = bestHR->sample_id;
            grp.health_lat         = bestHR->lat;
            grp.health_lon         = bestHR->lon;
            grp.pair_distance_km   = bestDist;
            grp.health_targets     = bestHR->targets;
            grp.worst_health_risk  = computeWorstHealthRisk(bestHR->targets);

            // Merge health evidence elements into raw_elements if not already present
            for (const auto& tr : bestHR->targets) {
                for (const auto& ev : tr.evidence) {
                    if (!ev.element.empty() && ev.raw_value != NO_DATA_VAL) {
                        bool found = false;
                        for (auto& re : grp.raw_elements) {
                            if (re.first == ev.element) { found = true; break; }
                        }
                        if (!found) grp.raw_elements.push_back({ev.element, ev.raw_value});
                    }
                }
            }
        } else {
            grp.worst_health_risk = "LOW";
        }

        groups.push_back(std::move(grp));
    }

    // Attach mine references
    attachMineRefs(groups, mineRefs);

    // Also create standalone health-only groups for health records
    // that had NO geology pair (so they still appear on health map)
    std::unordered_map<std::string, bool> pairedHealth;
    for (const auto& grp : groups) {
        if (grp.has_health_pair) pairedHealth[grp.health_sample_id] = true;
    }

    int hOnlyCounter = (int)groups.size() + 1;
    for (const HealthRecord& hr : healthRecords) {
        if (pairedHealth.count(hr.sample_id)) continue; // already paired
        GroupedRecord grp;
        std::ostringstream idss;
        idss << "GRP_H_" << std::setfill('0') << std::setw(4) << hOnlyCounter++;
        grp.group_id           = idss.str();
        grp.geo_sample_id      = ""; // no geology pair
        grp.geo_lat            = hr.lat;
        grp.geo_lon            = hr.lon;
        grp.has_health_pair    = true;
        grp.health_sample_id   = hr.sample_id;
        grp.health_lat         = hr.lat;
        grp.health_lon         = hr.lon;
        grp.pair_distance_km   = 0.0;
        grp.health_targets     = hr.targets;
        grp.worst_geology_risk = "LOW";
        grp.worst_health_risk  = hr.worst_risk_level;
        groups.push_back(std::move(grp));
    }

    std::cout << "[Grouper] Built " << groups.size() << " grouped records ("
              << geoFeatures.size() << " geology + "
              << healthRecords.size() << " health locations)\n";
    return groups;
}
