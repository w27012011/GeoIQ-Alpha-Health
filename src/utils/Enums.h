#ifndef GEOIQ_ENUMS_H
#define GEOIQ_ENUMS_H

#include <string>

namespace GeoIQ {

    // Risk levels used in predictions and reports
    enum class RiskLevel {
        INSUFFICIENT_DATA,
        LOW,
        MEDIUM,
        MEDIUM_HIGH,
        HIGH,
        CRITICAL
    };

    // Geochemical exploration and hazard targets
    enum class Target {
        ARSENIC_HAZARD,
        FLUORIDE_HAZARD,      // Placeholder for future v2 extension
        GOLD,
        GOSSAN,
        DIAMOND_KIMBERLITE,
        COPPER_PORPHYRY,
        COPPER_VMS,
        LITHIUM_PEGMATITE,
        LITHIUM_BRINE,
        REE_CARBONATITE,
        NICKEL_PGE,           // Placeholder for future v2 extension
        ZINC_LEAD
    };

    // Geochemical sample mediums
    enum class SampleType {
        STREAM_SEDIMENT,
        SOIL,
        BOREHOLE,
        GROUNDWATER,
        ROCK,
        VIRTUAL_GRID,         // IDW-interpolated cell node representing virtual surface samples
        UNKNOWN
    };

    inline std::string sampleTypeToString(SampleType type) {
        switch (type) {
            case SampleType::STREAM_SEDIMENT: return "STREAM_SEDIMENT";
            case SampleType::SOIL:            return "SOIL";
            case SampleType::BOREHOLE:        return "BOREHOLE";
            case SampleType::GROUNDWATER:     return "GROUNDWATER";
            case SampleType::ROCK:            return "ROCK";
            case SampleType::VIRTUAL_GRID:    return "VIRTUAL_GRID";
            default:                          return "UNKNOWN";
        }
    }

    // Soil horizons (primarily for soil classification)
    enum class HorizonType {
        SURFACE,
        A_HORIZON,
        B_HORIZON,
        C_HORIZON,
        BOREHOLE_SAMPLE,
        UNKNOWN
    };

    // Cheng 2007 singularity power-law states
    enum class SingularityState {
        ENRICHED,
        BACKGROUND,
        DEPLETED,
        INVALID
    };

    // O(1) contiguous element index structure for avoiding string loops
    enum class ElementField : int {
        // Trace elements (0 - 34)
        As = 0, Pb, Cu, Zn, Ni, Cr, Co, Mo, Sn, W, V, Ti,
        Ba, Sr, Zr, Rb, Cs, Li, La, Ce, Th, Y, U, Nb, Ta,
        Mn, Sb, Bi, Tl, Te, Se, Cd, Hg, F, Ag,
 
        // Precious metal (35)
        Au,

        // Major elements (36 - 45)
        Fe, Al, SiO2, MgO, K2O, Na2O, CaO, P2O5, S, LOI,

        // Environmental parameters (46 - 47)
        pH, Eh,

        // Location parameter mapped to static array indices (48)
        elevation_m,

        // Count sentinel (49)
        COUNT
    };

    // Helper to resolve ElementField enum to symbol string (O(1))
    inline std::string elementFieldToString(ElementField field) {
        static const char* names[] = {
            "As", "Pb", "Cu", "Zn", "Ni", "Cr", "Co", "Mo", "Sn", "W", "V", "Ti",
            "Ba", "Sr", "Zr", "Rb", "Cs", "Li", "La", "Ce", "Th", "Y", "U", "Nb", "Ta",
            "Mn", "Sb", "Bi", "Tl", "Te", "Se", "Cd", "Hg", "F", "Ag", "Au",
            "Fe", "Al", "SiO2", "MgO", "K2O", "Na2O", "CaO", "P2O5", "S", "LOI",
            "pH", "Eh", "elevation_m"
        };
        int idx = static_cast<int>(field);
        if (idx < 0 || idx >= static_cast<int>(ElementField::COUNT)) return "UNKNOWN";
        return names[idx];
    }

    // Helper to resolve symbol string back to ElementField enum (Linear search over 49 items)
    inline ElementField stringToElementField(const std::string& name) {
        for (int i = 0; i < static_cast<int>(ElementField::COUNT); ++i) {
            if (elementFieldToString(static_cast<ElementField>(i)) == name) {
                return static_cast<ElementField>(i);
            }
        }
        return ElementField::COUNT;
    }
}

#endif // GEOIQ_ENUMS_H
