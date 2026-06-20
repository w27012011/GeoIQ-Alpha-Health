#ifndef GEOIQ_SAMPLEPOINT_H
#define GEOIQ_SAMPLEPOINT_H

#include <string>
#include <cmath>
#include "utils/Constants.h"
#include "utils/Enums.h"

namespace GeoIQ {

    struct SamplePoint {
        // Location coordinates (defaults: NO_DATA)
        double lat          = NO_DATA; // decimal degrees (-90 to +90)
        double lon          = NO_DATA; // decimal degrees (-180 to +180)
        double elevation_m  = NO_DATA; // height above sea level
        double depth_m      = NO_DATA; // depth below surface (borehole/groundwater)

        // Metadata string fields
        std::string sample_id = "";
        std::string source    = "";
        std::string lithology = "";

        // Metadata enums
        SampleType  sample_type = SampleType::UNKNOWN;
        HorizonType horizon     = HorizonType::UNKNOWN;

        // Trace elements concentrations (ppm, default: NO_DATA)
        double As = NO_DATA;
        double Pb = NO_DATA;
        double Cu = NO_DATA;
        double Zn = NO_DATA;
        double Ni = NO_DATA;
        double Cr = NO_DATA;
        double Co = NO_DATA;
        double Mo = NO_DATA;
        double Sn = NO_DATA;
        double W  = NO_DATA;
        double V  = NO_DATA;
        double Ti = NO_DATA;
        double Ba = NO_DATA;
        double Sr = NO_DATA;
        double Zr = NO_DATA;
        double Rb = NO_DATA;
        double Cs = NO_DATA;
        double Li = NO_DATA;
        double La = NO_DATA;
        double Ce = NO_DATA;
        double Th = NO_DATA;
        double Y  = NO_DATA;
        double U  = NO_DATA;
        double Nb = NO_DATA;
        double Ta = NO_DATA;
        double Mn = NO_DATA;
        double Sb = NO_DATA;
        double Bi = NO_DATA;
        double Tl = NO_DATA;
        double Te = NO_DATA;
        double Se = NO_DATA;
        double Cd = NO_DATA;
        double Hg = NO_DATA;
        double F  = NO_DATA;
        double Ag = NO_DATA;

        // Precious metal (ppb, default: NO_DATA)
        double Au = NO_DATA;

        // Major elements concentrations (%, default: NO_DATA)
        double Fe   = NO_DATA;
        double Al   = NO_DATA;
        double SiO2 = NO_DATA;
        double MgO  = NO_DATA;
        double K2O  = NO_DATA;
        double Na2O = NO_DATA;
        double CaO  = NO_DATA;
        double P2O5 = NO_DATA;
        double S    = NO_DATA;
        double LOI  = NO_DATA;

        // Environmental parameters (default: NO_DATA)
        double pH = NO_DATA;
        double Eh = NO_DATA;

        // Computed geochemical ratios (populated by Dataset static applicator)
        double K_Rb_ratio  = NO_DATA;
        double Nb_Ta_ratio = NO_DATA;
        double Rb_Sr_ratio = NO_DATA;
        double Cr_Ni_ratio = NO_DATA;
        double Zn_Pb_ratio = NO_DATA;
        double Cu_Mo_ratio = NO_DATA;
        double Mg_Li_ratio = NO_DATA;
    };

    // Pointer-to-member array mapping ElementField enum indices to SamplePoint double fields (TRAP-01 Fix)
    inline double SamplePoint::* const ELEMENT_MEMBERS[] = {
        // Trace elements (0-34)
        &SamplePoint::As, &SamplePoint::Pb, &SamplePoint::Cu, &SamplePoint::Zn, &SamplePoint::Ni,
        &SamplePoint::Cr, &SamplePoint::Co, &SamplePoint::Mo, &SamplePoint::Sn, &SamplePoint::W,
        &SamplePoint::V,  &SamplePoint::Ti, &SamplePoint::Ba, &SamplePoint::Sr, &SamplePoint::Zr,
        &SamplePoint::Rb, &SamplePoint::Cs, &SamplePoint::Li, &SamplePoint::La, &SamplePoint::Ce,
        &SamplePoint::Th, &SamplePoint::Y,  &SamplePoint::U,  &SamplePoint::Nb, &SamplePoint::Ta,
        &SamplePoint::Mn, &SamplePoint::Sb, &SamplePoint::Bi, &SamplePoint::Tl, &SamplePoint::Te,
        &SamplePoint::Se, &SamplePoint::Cd, &SamplePoint::Hg, &SamplePoint::F,  &SamplePoint::Ag,

        // Precious metal (35)
        &SamplePoint::Au,

        // Major elements (36-45)
        &SamplePoint::Fe, &SamplePoint::Al, &SamplePoint::SiO2, &SamplePoint::MgO, &SamplePoint::K2O,
        &SamplePoint::Na2O, &SamplePoint::CaO, &SamplePoint::P2O5, &SamplePoint::S,  &SamplePoint::LOI,

        // Environmental (46-47)
        &SamplePoint::pH, &SamplePoint::Eh,

        // Location parameter (48)
        &SamplePoint::elevation_m
    };

    // Defensive epsilon helper checks for local data checks
    inline bool hasData(double v) {
        return std::abs(v - NO_DATA) > 1e-3;
    }

    // O(1) field lookup in innermost loops avoiding string comparisons
    inline double getFieldValue(const SamplePoint& point, ElementField field) {
        int idx = static_cast<int>(field);
        if (idx < 0 || idx >= static_cast<int>(ElementField::COUNT)) {
            return NO_DATA;
        }
        return point.*ELEMENT_MEMBERS[idx];
    }

    // O(1) field modification helper
    inline void setFieldValue(SamplePoint& point, ElementField field, double value) {
        int idx = static_cast<int>(field);
        if (idx >= 0 && idx < static_cast<int>(ElementField::COUNT)) {
            point.*ELEMENT_MEMBERS[idx] = value;
        }
    }
}

#endif // GEOIQ_SAMPLEPOINT_H
