#ifndef GEOIQ_CONSTANTS_H
#define GEOIQ_CONSTANTS_H

namespace GeoIQ {
    // Universal Sentinels and Offsets
    constexpr double NO_DATA                     = -9999.0;
    constexpr double EARTH_RADIUS_KM             = 6371.0;
    constexpr double LOG_ZERO_OFFSET             = 0.001;
    constexpr double PI                          = 3.14159265358979323846;

    // Spatial Window Defaults (overridden by config.txt)
    constexpr double DEFAULT_LOCAL_RADIUS_KM     = 10.0;
    constexpr double DEFAULT_REGIONAL_RADIUS_KM  = 100.0;
    constexpr double DEFAULT_IDW_POWER           = 2.0;
    constexpr double DEFAULT_GRID_CELL_SIZE_KM   = 1.0;

    // Statistical Thresholds
    constexpr int    MIN_NEIGHBOURS_FOR_STATS    = 3;
    constexpr int    TARGET_NEIGHBOURS           = 8;
    constexpr double MAX_RADIUS_EXPAND_FACTOR    = 5.0;
    constexpr double ANOMALY_Z_THRESHOLD         = 2.0;
    constexpr double STRONG_ANOMALY_Z_THRESHOLD  = 3.0;

    // Singularity Analysis (Cheng 2007)
    constexpr double SINGULARITY_ENRICHED_MAX    = 1.9;
    constexpr double SINGULARITY_DEPLETED_MIN    = 2.1;
    constexpr double SINGULARITY_MIN_R_SQUARED   = 0.5;

    // Arsenic Depth Risk Zones (metres, borehole/groundwater only)
    constexpr double AS_DEPTH_VERY_SHALLOW_MAX   = 15.0;
    constexpr double AS_DEPTH_PEAK_MIN           = 15.0;
    constexpr double AS_DEPTH_PEAK_MAX           = 100.0;
    constexpr double AS_DEPTH_MEDIUM_MAX         = 150.0;
    constexpr double AS_DEPTH_SAFE_MIN           = 150.0;

    // Combined Anomaly Score Weights
    constexpr double LOCAL_Z_WEIGHT              = 0.70;
    constexpr double REGIONAL_Z_WEIGHT           = 0.30;
}

#endif // GEOIQ_CONSTANTS_H
