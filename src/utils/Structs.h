#ifndef GEOIQ_STRUCTS_H
#define GEOIQ_STRUCTS_H

#include <string>
#include <vector>
#include "utils/Constants.h"
#include "utils/Enums.h"

namespace GeoIQ {

    // Statistics computed over local/regional moving windows
    struct WindowStats {
        double median     = NO_DATA;  // Median in log-space
        double mean       = NO_DATA;  // Mean in log-space
        double stddev     = NO_DATA;  // Std deviation in log-space
        double p95        = NO_DATA;  // 95th percentile in log-space
        double p05        = NO_DATA;  // 5th percentile in log-space
        int    n          = 0;        // Count of valid values used
        bool   valid      = false;    // true if n >= MIN_NEIGHBOURS_FOR_STATS
    };

    // Full spatial context for a single element at a sample point
    struct SpatialContext {
        WindowStats local;                    // Stats from local window
        WindowStats regional;                 // Stats from regional window
        double      local_z              = NO_DATA;  // Z-score vs local median
        double      regional_z           = NO_DATA;  // Z-score vs regional median
        double      anomaly_score        = NO_DATA;  // Combined score (70% local, 30% regional)
        double      confidence           = 0.0;      // Confidence multiplier (0.0 - 1.0)
        double      local_radius_used    = 0.0;      // Actual radius in km
        double      regional_radius_used = 0.0;      // Actual radius in km
    };

    // Singularity power-law regression results (Cheng 2007)
    struct SingularityResult {
        double           alpha      = NO_DATA;                  // Computed index
        double           r_squared  = 0.0;                      // OLS fit quality
        SingularityState state      = SingularityState::INVALID;// Anomaly classification
        bool             valid      = false;                    // true if r_squared >= limit
    };

    // Depth correction values (arsenic groundwater risk only)
    struct DepthModifier {
        double      multiplier    = 1.0;   // Score multiplier (0.5 - 1.4)
        std::string zone_name     = "";    // Name of depth interval
        std::string annotation    = "";    // Physical geological explanation
        bool        applicable    = false; // true only for BOREHOLE/GROUNDWATER mediums
    };

    // JIT formatting geochemical evidence block (O(1) layout)
    struct ElementEvidence {
        ElementField     element               = ElementField::As;
        double           raw_value             = NO_DATA;
        double           log_value             = NO_DATA;
        double           local_median_log      = NO_DATA;
        double           local_stddev_log      = NO_DATA;
        int              n_local               = 0;
        double           local_radius_km       = 0.0;
        double           local_z               = NO_DATA;
        double           regional_median_log   = NO_DATA;
        double           regional_stddev_log   = NO_DATA;
        int              n_regional            = 0;
        double           regional_z            = NO_DATA;
        double           anomaly_score         = NO_DATA;
        double           alpha                 = NO_DATA;
        SingularityState singularity_state     = SingularityState::INVALID;
        bool             is_anomalous          = false;
        bool             is_strongly_anomalous = false;
        std::string      data_quality_note     = ""; // Tag for virtual cell Z-score warnings (FIX-B)
    };

    // Single logical condition inside a rule
    struct RuleCondition {
        std::string condition_id    = "";    // e.g., "AS-1-C1"
        std::string description     = "";    // e.g., "As local_z >= 2.5"
        std::string lhs_name        = "";    // Variable description
        double      lhs_value       = NO_DATA;
        std::string operator_str    = "";    // Operator symbol (">=", "==", etc.)
        double      threshold_low   = NO_DATA;
        double      threshold_high  = NO_DATA; // Used in range bounds
        bool        passed          = false;
    };

    // Complete tracking trace for an individual rule execution
    struct RuleTrace {
        std::string                  rule_id            = "";                  // e.g., "AS-1"
        std::string                  rule_name          = "";                  // e.g., "Alluvial Arsenic"
        std::string                  confidence_level   = "";                  // "HIGH", "MEDIUM" etc.
        bool                         fired              = false;               // true if passed
        std::vector<RuleCondition>   conditions         = {};                  // Conditions checks list
        double                       score_contribution = 0.0;                 // Base rule weight
        std::string                  scientific_basis   = "";                  // Literature references
    };

    // Compiled rules output for a single target potential
    struct RuleResult {
        Target                       target                = Target::ARSENIC_HAZARD;
        RiskLevel                    risk_level            = RiskLevel::INSUFFICIENT_DATA;
        double                       score                 = 0.0;
        double                       confidence            = 0.0;
        bool                         sufficient_data       = false;
        std::string                  annotation            = "";                  // Summary explanation
        std::vector<std::string>     triggered_rules       = {};                  // Fired rule IDs
        double                       spatial_consistency   = 0.0;                 // Vote fraction (0.0 - 1.0)
        DepthModifier                depth_info;                                  // Arsenic modifier
        std::vector<ElementEvidence> evidence              = {};                  // Geochemical evidence trails
        std::vector<RuleTrace>       rule_traces           = {};                  // All traces evaluated
        std::string                  final_score_formula   = "";                  // Score computation equation
        std::string                  risk_level_derivation = "";                  // Score conversion detail
        std::string                  data_quality_note     = "";                  // Warnings summary string
    };

    // Consolidated prediction record output per point
    struct PredictionRecord {
        double      lat                  = NO_DATA;
        double      lon                  = NO_DATA;
        double      elevation_m          = NO_DATA;
        double      depth_m              = NO_DATA;
        std::string sample_id            = "";
        SampleType  sample_type          = SampleType::UNKNOWN;

        std::vector<RuleResult> results; // Results for all target models evaluated

        bool        dispersion_detected  = false;
        double      source_bearing_deg   = NO_DATA;
        double      source_distance_km   = NO_DATA;
        std::string dispersion_annotation = "";
    };
}

#endif // GEOIQ_STRUCTS_H
