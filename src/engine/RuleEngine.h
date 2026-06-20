#ifndef GEOIQ_RULEENGINE_H
#define GEOIQ_RULEENGINE_H

#include <vector>
#include <array>
#include <string>
#include "data/SamplePoint.h"
#include "spatial/MovingWindow.h"
#include "spatial/SpatialAnalysis.h"
#include "utils/Structs.h"
#include "utils/Enums.h"

namespace GeoIQ {

    struct PointContext {
        const SamplePoint*                                                           point;
        std::array<MovingWindow::Result, static_cast<size_t>(ElementField::COUNT)>   mw;   // O(1) direct array access
        SpatialAnalysis::ConsistencyResult                                           consistency; // multi-element (specifically for As)
        double                                                                       consistency_radius_km;
    };

    class RuleEngine {
    public:
        // Configuration flags
        bool enable_arsenic     = true;
        bool enable_gold        = true;
        bool enable_diamond     = true;
        bool enable_copper      = true;
        bool enable_lithium     = true;
        bool enable_ree         = true;
        bool enable_zinc_lead   = true;
        bool enable_gossan      = true;

        // Main entry point. Returns one RuleResult per enabled target.
        std::vector<RuleResult> evaluateAll(const PointContext& ctx) const;

    private:
        // Private evaluators per target/deposit type
        RuleResult evalArsenic (const PointContext& ctx) const;
        RuleResult evalGold    (const PointContext& ctx) const;
        RuleResult evalDiamond (const PointContext& ctx) const;
        RuleResult evalCopperPorphyry(const PointContext& ctx) const;
        RuleResult evalCopperVMS(const PointContext& ctx) const;
        RuleResult evalLithiumPegmatite(const PointContext& ctx) const;
        RuleResult evalLithiumBrine(const PointContext& ctx) const;
        RuleResult evalREE     (const PointContext& ctx) const;
        RuleResult evalZincLead(const PointContext& ctx) const;
        RuleResult evalGossan  (const PointContext& ctx) const;

        // Helper: get stats/values for a field from PointContext.
        double z(const PointContext& ctx, ElementField field) const;
        double alpha(const PointContext& ctx, ElementField field) const;
        double raw(const PointContext& ctx, ElementField field) const;
        bool   hasZ(const PointContext& ctx, ElementField field) const;

        // Helper: build a RuleCondition struct
        RuleCondition makeCondition(
            const std::string& id, const std::string& desc,
            const std::string& lhs_name, double lhs_value,
            const std::string& op, double threshold_low,
            double threshold_high = NO_DATA) const;

        // Helper: evaluate a condition and record it
        bool evalCond(RuleCondition& cond) const;

        // Helper: aggregate rule results into final score and risk level
        void finalise(RuleResult& result, Target target,
                      const std::vector<RuleTrace>& traces,
                      const PointContext& ctx,
                      const std::vector<ElementField>& elements_used) const;

        // Depth modifier for arsenic
        DepthModifier computeDepthModifier(const SamplePoint& p) const;

        // Annotation builder for each target
        std::string buildAnnotation(Target target, RiskLevel rl,
                                    const std::vector<RuleTrace>& traces,
                                    const PointContext& ctx) const;

        // Rule IDs string builder
        static std::string buildRuleIdsString(const std::vector<RuleTrace>& traces);
    };
}

#endif // GEOIQ_RULEENGINE_H
