#include "engine/RuleEngine.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include "utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace GeoIQ {

    std::vector<RuleResult> RuleEngine::evaluateAll(const PointContext& ctx) const {
        std::vector<RuleResult> results;
        results.reserve(10);

        if (enable_arsenic) {
            results.push_back(evalArsenic(ctx));
        }
        if (enable_gold) {
            results.push_back(evalGold(ctx));
        }
        if (enable_diamond) {
            results.push_back(evalDiamond(ctx));
        }
        if (enable_copper) {
            results.push_back(evalCopperPorphyry(ctx));
            results.push_back(evalCopperVMS(ctx));
        }
        if (enable_lithium) {
            results.push_back(evalLithiumPegmatite(ctx));
            results.push_back(evalLithiumBrine(ctx));
        }
        if (enable_ree) {
            results.push_back(evalREE(ctx));
        }
        if (enable_zinc_lead) {
            results.push_back(evalZincLead(ctx));
        }
        if (enable_gossan) {
            results.push_back(evalGossan(ctx));
        }

        // Return only sufficient data results
        std::vector<RuleResult> filtered;
        filtered.reserve(results.size());
        for (auto& r : results) {
            if (r.sufficient_data && r.risk_level != RiskLevel::INSUFFICIENT_DATA) {
                filtered.push_back(std::move(r));
            }
        }
        return filtered;
    }

    double RuleEngine::z(const PointContext& ctx, ElementField field) const {
        int idx = static_cast<int>(field);
        if (idx < 0 || idx >= static_cast<int>(ElementField::COUNT)) return NO_DATA;
        return ctx.mw[idx].context.local_z;
    }

    double RuleEngine::alpha(const PointContext& ctx, ElementField field) const {
        int idx = static_cast<int>(field);
        if (idx < 0 || idx >= static_cast<int>(ElementField::COUNT)) return NO_DATA;
        return ctx.mw[idx].singularity.alpha;
    }

    double RuleEngine::raw(const PointContext& ctx, ElementField field) const {
        return getFieldValue(*ctx.point, field);
    }

    bool RuleEngine::hasZ(const PointContext& ctx, ElementField field) const {
        int idx = static_cast<int>(field);
        if (idx < 0 || idx >= static_cast<int>(ElementField::COUNT)) return false;
        return hasData(ctx.mw[idx].context.local_z);
    }

    RuleCondition RuleEngine::makeCondition(
        const std::string& id, const std::string& desc,
        const std::string& lhs_name, double lhs_value,
        const std::string& op, double threshold_low,
        double threshold_high) const {
        
        RuleCondition cond;
        cond.condition_id = id;
        cond.description = desc;
        cond.lhs_name = lhs_name;
        cond.lhs_value = lhs_value;
        cond.operator_str = op;
        cond.threshold_low = threshold_low;
        cond.threshold_high = threshold_high;
        cond.passed = evalCond(cond);
        return cond;
    }

    bool RuleEngine::evalCond(RuleCondition& cond) const {
        double v = cond.lhs_value;
        // Benefit of the doubt for missing raw or Z-score parameter inputs
        if (Math::isNoData(v)) {
            return true;
        }

        if (cond.operator_str == ">=") {
            return v >= cond.threshold_low;
        } else if (cond.operator_str == "<") {
            return v < cond.threshold_low;
        } else if (cond.operator_str == "range") {
            return v >= cond.threshold_low && v <= cond.threshold_high;
        } else if (cond.operator_str == "==") {
            return std::abs(v - cond.threshold_low) < 1e-3;
        }
        return false;
    }

    void RuleEngine::finalise(RuleResult& result, Target target,
                              const std::vector<RuleTrace>& traces,
                              const PointContext& ctx,
                              const std::vector<ElementField>& elements_used) const {
        result.target = target;
        result.rule_traces = traces;

        // Fired rules list
        std::vector<RuleTrace> fired_traces;
        for (const auto& t : traces) {
            if (t.fired) {
                fired_traces.push_back(t);
                result.triggered_rules.push_back(t.rule_id);
            }
        }

        // Score computation
        if (fired_traces.empty()) {
            result.score = 0.0;
            result.risk_level = RiskLevel::LOW;
        } else {
            double sum_scores = 0.0;
            for (const auto& t : fired_traces) {
                sum_scores += t.score_contribution;
            }
            result.score = sum_scores / fired_traces.size();
        }

        if (target == Target::ARSENIC_HAZARD) {
            DepthModifier dm = computeDepthModifier(*ctx.point);
            result.depth_info = dm;
            result.score = std::clamp(result.score * dm.multiplier, 0.0, 1.0);
        }

        // Confidence logic
        double min_conf = 1.0;
        if (!elements_used.empty()) {
            for (auto el : elements_used) {
                double conf = ctx.mw[static_cast<size_t>(el)].context.confidence;
                if (conf < min_conf) {
                    min_conf = conf;
                }
            }
        }
        result.confidence = min_conf;

        if (result.confidence <= 0.0) {
            result.risk_level = RiskLevel::INSUFFICIENT_DATA;
            result.sufficient_data = false;
        } else {
            result.risk_level = Stats::riskLevelFromScore(result.score);
            result.sufficient_data = true;
        }

        // Confidence cap: with fewer than 4 local neighbours (confidence < 0.5),
        // the local std-dev estimate has only 2 degrees of freedom and is unreliable.
        // A near-zero std-dev inflates Z-scores by 3-10x, causing false CRITICAL flags
        // (confirmed empirically: Cu=7.6 ppm, nationally normal, fires CRITICAL at n=3).
        // Cap to MEDIUM_HIGH so the site is still flagged and visible but not over-stated.
        // The rule thresholds themselves (z>=2.5 etc.) are scientifically correct for n>=8;
        // this cap only activates when the window is too sparse to trust those scores.
        if (result.sufficient_data && result.confidence < 0.5) {
            if (result.risk_level == RiskLevel::CRITICAL) {
                result.risk_level = RiskLevel::MEDIUM_HIGH;
            } else if (result.risk_level == RiskLevel::HIGH) {
                result.risk_level = RiskLevel::MEDIUM;
            }
        }

        // Spatial consistency
        result.spatial_consistency = ctx.consistency.consistency;

        // Load lazy ElementEvidence blocks
        result.evidence.reserve(elements_used.size());
        for (auto el : elements_used) {
            result.evidence.push_back(ctx.mw[static_cast<size_t>(el)].evidence);
        }

        // Final score formula string
        std::stringstream fs;
        if (fired_traces.empty()) {
            fs << "score=0.0";
        } else {
            fs << "score=mean([";
            for (size_t i = 0; i < fired_traces.size(); ++i) {
                fs << std::fixed << std::setprecision(2) << fired_traces[i].score_contribution;
                if (i + 1 < fired_traces.size()) fs << ",";
            }
            fs << "])";
            if (target == Target::ARSENIC_HAZARD) {
                fs << "×depth_mod(" << std::fixed << std::setprecision(1) << result.depth_info.multiplier << ")";
            }
            fs << "×confidence(" << std::fixed << std::setprecision(2) << result.confidence << ")";
        }
        result.final_score_formula = fs.str();

        // Final risk derivation string
        std::stringstream rd;
        rd << "score=" << std::fixed << std::setprecision(3) << result.score << " → "
           << Stats::riskLevelToString(result.risk_level);
        result.risk_level_derivation = rd.str();

        // Build data quality note
        std::stringstream dq;
        bool low_density = false;
        bool radius_expanded = false;
        for (const auto& ev : result.evidence) {
            if (ev.n_local < TARGET_NEIGHBOURS) {
                low_density = true;
            }
            if (ev.local_radius_km > DEFAULT_LOCAL_RADIUS_KM) {
                radius_expanded = true;
            }
        }
        if (radius_expanded) {
            dq << "Local search radius expanded beyond default " << DEFAULT_LOCAL_RADIUS_KM << " km due to sparse data. ";
        }
        if (low_density) {
            dq << "Low sample density: some elements have fewer than " << TARGET_NEIGHBOURS << " neighbours. ";
        }
        if (ctx.point->sample_type == SampleType::VIRTUAL_GRID) {
            dq << "VIRTUAL_GRID: Z-scores calculated on smoothed IDW surface. ";
        }
        result.data_quality_note = dq.str();

        // Smelter Gate: If surface soil/stream Lead (Pb) or Zinc (Zn) is over 1000 ppm,
        // downgrade target anomalies (exploration metals only) to MEDIUM and flag as anthropogenic.
        if (target != Target::ARSENIC_HAZARD && target != Target::FLUORIDE_HAZARD) {
            double pb_raw = raw(ctx, ElementField::Pb);
            double zn_raw = raw(ctx, ElementField::Zn);
            bool is_surface = (ctx.point->horizon == HorizonType::A_HORIZON || 
                               ctx.point->horizon == HorizonType::SURFACE ||
                               ctx.point->depth_m <= 0.15 ||
                               ctx.point->sample_type == SampleType::SOIL ||
                               ctx.point->sample_type == SampleType::STREAM_SEDIMENT);
            if (is_surface && ((hasData(pb_raw) && pb_raw > 1000.0) || (hasData(zn_raw) && zn_raw > 1000.0))) {
                if (result.risk_level == RiskLevel::CRITICAL || result.risk_level == RiskLevel::HIGH || result.risk_level == RiskLevel::MEDIUM_HIGH) {
                    result.risk_level = RiskLevel::MEDIUM;
                    std::stringstream rds;
                    rds << "score=" << std::fixed << std::setprecision(3) << result.score << " → MEDIUM (Smelter Gate Downgrade)";
                    result.risk_level_derivation = rds.str();
                }
                result.data_quality_note += "⚠️ SUSPECTED ANTHROPOGENIC CONTAMINATION / HISTORIC SMELTER SLAG. ";
            }
        }

        // Annotation
        result.annotation = buildAnnotation(target, result.risk_level, traces, ctx);
    }

    DepthModifier RuleEngine::computeDepthModifier(const SamplePoint& p) const {
        if (p.sample_type != SampleType::BOREHOLE && p.sample_type != SampleType::GROUNDWATER) {
            return DepthModifier{1.0, "N/A", "Depth modifier not applicable (not a borehole/groundwater sample)", false};
        }
        if (!hasData(p.depth_m)) {
            return DepthModifier{1.0, "UNKNOWN_DEPTH", "Depth not recorded — no modifier applied", true};
        }

        if (p.depth_m < AS_DEPTH_VERY_SHALLOW_MAX) {
            std::stringstream ss;
            ss << "Depth " << std::fixed << std::setprecision(1) << p.depth_m << "m < 15m: oxidising zone, As locked onto Fe-oxides. Lower risk.";
            return DepthModifier{0.6, "VERY_SHALLOW_LOW_RISK", ss.str(), true};
        }
        if (p.depth_m <= AS_DEPTH_PEAK_MAX) {
            std::stringstream ss;
            ss << "Depth " << std::fixed << std::setprecision(1) << p.depth_m << "m in 15-100m PEAK ZONE: reducing conditions, Fe-oxyhydroxide dissolution. Highest risk. [Smedley & Kinniburgh 2002]";
            return DepthModifier{1.4, "PEAK_RISK_ZONE", ss.str(), true};
        }
        if (p.depth_m <= AS_DEPTH_MEDIUM_MAX) {
            std::stringstream ss;
            ss << "Depth " << std::fixed << std::setprecision(1) << p.depth_m << "m in 100-150m transitional zone: variable redox conditions.";
            return DepthModifier{1.0, "TRANSITIONAL_ZONE", ss.str(), true};
        }
        std::stringstream ss;
        ss << "Depth " << std::fixed << std::setprecision(1) << p.depth_m << "m > 150m: typically oxic groundwater, As re-adsorbed. Lower risk. [NIH 2023]";
        return DepthModifier{0.5, "DEEP_SAFE_ZONE", ss.str(), true};
    }

    std::string RuleEngine::buildRuleIdsString(const std::vector<RuleTrace>& traces) {
        std::string rule_ids = "";
        int fired_count = 0;
        for (const auto& t : traces) {
            if (t.fired) {
                if (fired_count > 0) rule_ids += ", ";
                rule_ids += t.rule_id;
                fired_count++;
            }
        }
        if (rule_ids.empty()) rule_ids = "None";
        return rule_ids;
    }

    std::string RuleEngine::buildAnnotation(Target target, RiskLevel rl,
                                            const std::vector<RuleTrace>& traces,
                                            const PointContext& ctx) const {
        (void)ctx;
        std::string rule_ids = buildRuleIdsString(traces);
        std::stringstream ss;
        if (target != Target::ARSENIC_HAZARD && target != Target::FLUORIDE_HAZARD) {
            ss << "Potentially ";
        }
        ss << Stats::targetToString(target) << " Risk is " << Stats::riskLevelToString(rl);
        if (rule_ids == "None") {
            ss << " (Background values, no rules triggered)";
        } else {
            ss << " due to rules: [" << rule_ids << "]";
        }
        return ss.str();
    }

    RuleResult RuleEngine::evalArsenic(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::As)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::ARSENIC_HAZARD;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(4);

        // RULE AS-1
        {
            RuleTrace t;
            t.rule_id = "AS-1";
            t.rule_name = "Reducing alluvial arsenic hazard";
            t.scientific_basis = "Smedley & Kinniburgh 2002 (Applied Geochemistry 17:517-568, BGS); reductive dissolution of Fe-oxyhydroxides releases adsorbed As. Peak mobilisation in Holocene alluvial/deltaic reducing aquifers.";
            t.score_contribution = 0.85;

            bool is_low_alluvial = (ctx.point->sample_type == SampleType::STREAM_SEDIMENT ||
                                    ctx.point->sample_type == SampleType::SOIL ||
                                    (hasData(ctx.point->elevation_m) && ctx.point->elevation_m < 100.0));

            t.conditions.push_back(makeCondition("AS-1-C1", "As locally anomalous (z >= 2.5σ above local median)", "local_z[As]", z(ctx, ElementField::As), ">=", 2.5));
            t.conditions.push_back(makeCondition("AS-1-C2", "Fe elevated (Fe-oxyhydroxide dissolution indicator)", "local_z[Fe]", z(ctx, ElementField::Fe), ">=", 1.5));
            t.conditions.push_back(makeCondition("AS-1-C3", "Low-lying or alluvial setting", "is_low_alluvial", is_low_alluvial ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("AS-1-C4", "Spatial coherence: >=50% of neighbours also anomalous", "consistency", ctx.consistency.consistency, ">=", 0.50));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE AS-2
        {
            RuleTrace t;
            t.rule_id = "AS-2";
            t.rule_name = "Mn-oxide arsenic co-mobilisation";
            t.scientific_basis = "Smedley & Kinniburgh 2002; Mn-oxides release As under reducing/acid conditions. pH < 6.5 accelerates As desorption from Fe/Mn-oxide surfaces.";
            t.score_contribution = 0.55;

            double p_val = raw(ctx, ElementField::pH);
            double z_mn = z(ctx, ElementField::Mn);
            bool pass_c3 = (hasData(p_val) && p_val < 6.5) || (hasData(z_mn) && z_mn >= 3.0);

            t.conditions.push_back(makeCondition("AS-2-C1", "As locally anomalous (z >= 2.0)", "local_z[As]", z(ctx, ElementField::As), ">=", 2.0));
            t.conditions.push_back(makeCondition("AS-2-C2", "Mn elevated — Mn-oxide dissolution", "local_z[Mn]", z_mn, ">=", 2.0));
            t.conditions.push_back(makeCondition("AS-2-C3", "Acid conditions OR strongly anomalous Mn", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE AS-3
        {
            RuleTrace t;
            t.rule_id = "AS-3";
            t.rule_name = "Regional low-elevation arsenic zone";
            t.scientific_basis = "Smedley & Kinniburgh 2002; low-lying floodplains accumulate reducing organic-rich sediment. WHO guideline 10 µg/L; BGS G-BASE arsenic mapping.";
            t.score_contribution = 0.30;

            int idx = static_cast<int>(ElementField::As);
            double reg_z = ctx.mw[idx].context.regional_z;

            bool is_low = (ctx.point->elevation_m == NO_DATA || ctx.point->elevation_m < 50.0);

            t.conditions.push_back(makeCondition("AS-3-C1", "As regionally elevated (entire province)", "regional_z[As]", reg_z, ">=", 1.5));
            t.conditions.push_back(makeCondition("AS-3-C2", "Low-lying area (< 50m ASL, floodplain risk)", "is_low", is_low ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("AS-3-C3", "At least weakly anomalous locally", "local_z[As]", z(ctx, ElementField::As), ">=", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE AS-DEPTH
        {
            RuleTrace t;
            t.rule_id = "AS-DEPTH";
            t.rule_name = "Rising arsenic with depth — reducing aquifer";
            t.scientific_basis = "Smedley & Kinniburgh 2002; NIH 2023 Bangladesh studies; Peak As in 15-100m zone (reducing conditions). Deep >150m = oxic = safer.";
            t.score_contribution = 0.70;

            bool is_borehole = (ctx.point->sample_type == SampleType::BOREHOLE || ctx.point->sample_type == SampleType::GROUNDWATER);
            bool is_peak_depth = (hasData(ctx.point->depth_m) && ctx.point->depth_m >= AS_DEPTH_PEAK_MIN && ctx.point->depth_m <= AS_DEPTH_PEAK_MAX);

            t.conditions.push_back(makeCondition("AS-D-C1", "Borehole or groundwater sample", "is_borehole", is_borehole ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("AS-D-C2", "Sample in peak arsenic risk zone (15-100m depth)", "is_peak_depth", is_peak_depth ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("AS-D-C3", "As anomalous at this depth", "local_z[As]", z(ctx, ElementField::As), ">=", 1.5));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::As, ElementField::Fe, ElementField::Mn};
        finalise(result, Target::ARSENIC_HAZARD, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalGold(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Au) && !hasZ(ctx, ElementField::As) && !hasZ(ctx, ElementField::Sb)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::GOLD;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(4);

        // RULE AU-1
        {
            RuleTrace t;
            t.rule_id = "AU-1";
            t.rule_name = "As-Sb pathfinder suite with Fe-oxide (orogenic/Carlin gold)";
            t.scientific_basis = "Economic Geology (Society of Economic Geologists); As (arsenopyrite) + Sb (stibnite) = classic orogenic gold pathfinder pair. Fe-oxide anomaly = gossan/oxidised sulfide. Cheng 2007 singularity α < 1.9. Both As AND Sb required — neither alone is sufficient.";
            t.score_contribution = 0.85;

            double a_as = alpha(ctx, ElementField::As);
            double a_sb = alpha(ctx, ElementField::Sb);
            bool pass_c4 = false;
            if (Math::isNoData(a_as) && Math::isNoData(a_sb)) {
                pass_c4 = true;
            } else {
                if (hasData(a_as) && a_as < 1.9) pass_c4 = true;
                if (hasData(a_sb) && a_sb < 1.9) pass_c4 = true;
            }

            bool pass_c5 = (ctx.point->horizon == HorizonType::C_HORIZON ||
                            ctx.point->horizon == HorizonType::UNKNOWN ||
                            ctx.point->sample_type != SampleType::SOIL);

            t.conditions.push_back(makeCondition("AU-1-C1", "As elevated (z >= 2.0)", "local_z[As]", z(ctx, ElementField::As), ">=", 2.0));
            t.conditions.push_back(makeCondition("AU-1-C2", "Sb elevated — As+Sb pair required for gold", "local_z[Sb]", z(ctx, ElementField::Sb), ">=", 1.8));
            t.conditions.push_back(makeCondition("AU-1-C3", "Fe-oxide anomaly (gossan/oxidised sulfide)", "local_z[Fe]", z(ctx, ElementField::Fe), ">=", 1.5));
            t.conditions.push_back(makeCondition("AU-1-C4", "Singularity enrichment (α<1.9) — sharp local spike", "pass_c4", pass_c4 ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("AU-1-C5", "Not restricted to surface A-horizon", "pass_c5", pass_c5 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE AU-2
        {
            RuleTrace t;
            t.rule_id = "AU-2";
            t.rule_name = "Hg+Tl volatile pathfinder suite (epithermal gold)";
            t.scientific_basis = "Economic Geology; Lyell Collection (Geological Society London). Hg = most volatile gold pathfinder, forms vapour halos above epithermal systems. Tl = highly diagnostic of epithermal systems, often overlooked.";
            t.score_contribution = 0.85;

            t.conditions.push_back(makeCondition("AU-2-C1", "Hg strongly anomalous — volatile epithermal indicator", "local_z[Hg]", z(ctx, ElementField::Hg), ">=", 2.5));
            t.conditions.push_back(makeCondition("AU-2-C2", "Tl elevated — highly diagnostic of epithermal systems", "local_z[Tl]", z(ctx, ElementField::Tl), ">=", 2.0));
            t.conditions.push_back(makeCondition("AU-2-C3", "As elevated in epithermal zone", "local_z[As]", z(ctx, ElementField::As), ">=", 1.5));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE AU-3
        {
            RuleTrace t;
            t.rule_id = "AU-3";
            t.rule_name = "Polymetallic sulfide system (potential gold-bearing)";
            t.scientific_basis = "SEG Special Publications; porphyry/epithermal systems often carry Au. Cu+Pb+Zn polymetallic suite in hydrothermal system.";
            t.score_contribution = 0.55;

            t.conditions.push_back(makeCondition("AU-3-C1", "Cu elevated in polymetallic halo", "local_z[Cu]", z(ctx, ElementField::Cu), ">=", 2.0));
            t.conditions.push_back(makeCondition("AU-3-C2", "Pb elevated in polymetallic halo", "local_z[Pb]", z(ctx, ElementField::Pb), ">=", 1.5));
            t.conditions.push_back(makeCondition("AU-3-C3", "Zn elevated in polymetallic halo", "local_z[Zn]", z(ctx, ElementField::Zn), ">=", 1.5));
            t.conditions.push_back(makeCondition("AU-3-C4", "As pathfinder weakly anomalous", "local_z[As]", z(ctx, ElementField::As), ">=", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE AU-4
        {
            RuleTrace t;
            t.rule_id = "AU-4";
            t.rule_name = "Direct Au anomaly (ppb)";
            t.scientific_basis = "BGS G-BASE; USGS NGDB. Au background < 5 ppb stream sediment. > 10 ppb = anomalous. Note: Au in ppb, stored as ppb in dataset.";
            t.score_contribution = 0.85;

            bool has_au_data = hasData(raw(ctx, ElementField::Au));

            t.conditions.push_back(makeCondition("AU-4-C1", "Au directly anomalous vs local background", "local_z[Au]", z(ctx, ElementField::Au), ">=", 2.0));
            t.conditions.push_back(makeCondition("AU-4-C2", "Au data present in dataset", "has_au_data", has_au_data ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Au, ElementField::As, ElementField::Sb, ElementField::Fe, ElementField::Hg, ElementField::Tl, ElementField::Cu, ElementField::Pb, ElementField::Zn};
        finalise(result, Target::GOLD, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalDiamond(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Cr) && !hasZ(ctx, ElementField::Ni)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::DIAMOND_KIMBERLITE;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(3);

        // RULE DIA-1
        {
            RuleTrace t;
            t.rule_id = "DIA-1";
            t.rule_name = "Ultramafic Cr-Ni-Mg suite with low SiO2 (kimberlite signature)";
            t.scientific_basis = "NRCan Kimberlite Exploration Manual; Harvard Mineralogy Database; Geoscience World. Kimberlites: MgO >15%, SiO2 <35% (CRITICAL), Cr >300 ppm (Cr-spinel), Ni >200 ppm (olivine Ni), Zr elevated (baddeleyite). SiO2 < 35% is the MOST IMPORTANT single discriminator.";
            t.score_contribution = 0.85;

            double raw_mgo = raw(ctx, ElementField::MgO);
            double z_mgo = z(ctx, ElementField::MgO);
            bool pass_c3 = (hasData(raw_mgo) && raw_mgo >= 15.0) || (hasData(z_mgo) && z_mgo >= 2.5);

            double raw_sio2 = raw(ctx, ElementField::SiO2);
            bool pass_c4 = !hasData(raw_sio2) || raw_sio2 < 35.0;

            t.conditions.push_back(makeCondition("DIA-1-C1", "Cr strongly anomalous (Cr-spinel, Cr-pyrope indicator)", "local_z[Cr]", z(ctx, ElementField::Cr), ">=", 2.5));
            t.conditions.push_back(makeCondition("DIA-1-C2", "Ni elevated (forsteritic olivine Ni content)", "local_z[Ni]", z(ctx, ElementField::Ni), ">=", 2.0));
            t.conditions.push_back(makeCondition("DIA-1-C3", "MgO >15% or strongly anomalous — ultramafic signature", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("DIA-1-C4", "SiO2 <35% (kimberlite is SiO2-poor — CRITICAL)", "pass_c4", pass_c4 ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("DIA-1-C5", "Zr elevated (baddeleyite/zircon in kimberlite)", "local_z[Zr]", z(ctx, ElementField::Zr), ">=", 1.5));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE DIA-2
        {
            RuleTrace t;
            t.rule_id = "DIA-2";
            t.rule_name = "Sr-Ba-Nb incompatible element enrichment (kimberlite/carbonatite)";
            t.scientific_basis = "NRCan; Harvard Mineralogy; Geoscience World. Kimberlites are volatile-rich → enriched in Ba (phlogopite), Sr (carbonates), Nb (perovskite/pyrochlore). Also elevated Th (monazite, perovskite).";
            t.score_contribution = 0.55;

            t.conditions.push_back(makeCondition("DIA-2-C1", "Sr anomalous (volatile carbonatite suite)", "local_z[Sr]", z(ctx, ElementField::Sr), ">=", 2.0));
            t.conditions.push_back(makeCondition("DIA-2-C2", "Ba anomalous (phlogopite indicator)", "local_z[Ba]", z(ctx, ElementField::Ba), ">=", 2.0));
            t.conditions.push_back(makeCondition("DIA-2-C3", "Nb elevated — perovskite/pyrochlore (kimberlite)", "local_z[Nb]", z(ctx, ElementField::Nb), ">=", 2.0));
            t.conditions.push_back(makeCondition("DIA-2-C4", "Cr at least weakly elevated", "local_z[Cr]", z(ctx, ElementField::Cr), ">=", 1.5));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE DIA-3
        {
            RuleTrace t;
            t.rule_id = "DIA-3";
            t.rule_name = "Th-La enrichment with low SiO2 (carbonatite-kimberlite association)";
            t.scientific_basis = "NRCan; Applied Geochemistry carbonatite literature. Kimberlites often associated with carbonatites. Circular pipe-like anomaly.";
            t.score_contribution = 0.30;

            double raw_sio2 = raw(ctx, ElementField::SiO2);
            bool pass_c3 = !hasData(raw_sio2) || raw_sio2 < 40.0;

            t.conditions.push_back(makeCondition("DIA-3-C1", "Th anomalous (monazite enrichment)", "local_z[Th]", z(ctx, ElementField::Th), ">=", 2.0));
            t.conditions.push_back(makeCondition("DIA-3-C2", "La anomalous (LREE carbonatite suite)", "local_z[La]", z(ctx, ElementField::La), ">=", 2.0));
            t.conditions.push_back(makeCondition("DIA-3-C3", "SiO2 <40% (carbonatite is SiO2-poor)", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Cr, ElementField::Ni, ElementField::MgO, ElementField::SiO2, ElementField::Zr, ElementField::Sr, ElementField::Ba, ElementField::Nb, ElementField::Th, ElementField::La};
        finalise(result, Target::DIAMOND_KIMBERLITE, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalCopperPorphyry(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Cu)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::COPPER_PORPHYRY;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(2);

        // RULE CU-1
        {
            RuleTrace t;
            t.rule_id = "CU-1";
            t.rule_name = "Porphyry Cu-Mo core zone";
            t.scientific_basis = "SEG Special Publications on porphyry systems; SRK Consulting. Cu+Mo co-anomaly = defining signature of porphyry core (potassic zone). K2O elevated = potassic alteration halo. Sr/Y > 40 in fertile arc magmas.";
            t.score_contribution = 0.85;

            double z_k2o = z(ctx, ElementField::K2O);
            double z_as = z(ctx, ElementField::As);
            bool pass_c3 = (hasData(z_k2o) && z_k2o >= 1.5) || (hasData(z_as) && z_as >= 1.5);

            t.conditions.push_back(makeCondition("CU-1-C1", "Cu strongly elevated (z >= 2.5)", "local_z[Cu]", z(ctx, ElementField::Cu), ">=", 2.5));
            double raw_mo = raw(ctx, ElementField::Mo);
            bool pass_c2 = z(ctx, ElementField::Mo) >= 2.0 && (!hasData(raw_mo) || raw_mo >= 1.0);
            t.conditions.push_back(makeCondition("CU-1-C2", "Mo elevated (z >= 2.0) and raw Mo >= 1.0 ppm", "pass_c2", pass_c2 ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("CU-1-C3", "Potassic alteration (K2O) or intermediate zone pathfinder (As)", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE CU-2
        {
            RuleTrace t;
            t.rule_id = "CU-2";
            t.rule_name = "Porphyry distal Pb-Zn-Ag halo";
            t.scientific_basis = "SEG; Geochemistry Exploration Environment Analysis. Pb+Zn+Ag in outer propylitic zone signals: 'move inward toward Cu-Mo core'. Distal halo = you are OUTSIDE the deposit — vector inward.";
            t.score_contribution = 0.55;

            t.conditions.push_back(makeCondition("CU-2-C1", "Pb elevated in distal propylitic zone", "local_z[Pb]", z(ctx, ElementField::Pb), ">=", 2.0));
            t.conditions.push_back(makeCondition("CU-2-C2", "Zn elevated in distal propylitic zone", "local_z[Zn]", z(ctx, ElementField::Zn), ">=", 2.0));
            t.conditions.push_back(makeCondition("CU-2-C3", "Ag elevated in distal propylitic zone", "local_z[Ag]", z(ctx, ElementField::Ag), ">=", 1.5));
            t.conditions.push_back(makeCondition("CU-2-C4", "Cu weakly elevated — still approaching core", "local_z[Cu]", z(ctx, ElementField::Cu), ">=", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Cu, ElementField::Mo, ElementField::K2O, ElementField::As, ElementField::Pb, ElementField::Zn, ElementField::Ag};
        finalise(result, Target::COPPER_PORPHYRY, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalCopperVMS(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Cu)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::COPPER_VMS;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(1);

        // RULE CU-3
        {
            RuleTrace t;
            t.rule_id = "CU-3";
            t.rule_name = "VMS Cu-Zn-Pb massive sulfide";
            t.scientific_basis = "USGS Mineral Resources; Geoscience World. VMS = volcanogenic massive sulfide. Cu+Zn+Pb all strongly anomalous. Zn/(Zn+Pb) ratio > 0.65 indicates VMS vs MVT style.";
            t.score_contribution = 0.85;

            double ratio = ctx.point->Zn_Pb_ratio;
            bool pass_c4 = !hasData(ratio) || ratio >= 0.65;

            double z_mo = z(ctx, ElementField::Mo);
            bool pass_c5 = !hasData(z_mo) || z_mo < 1.0;

            t.conditions.push_back(makeCondition("CU-3-C1", "Cu strongly elevated (VMS center)", "local_z[Cu]", z(ctx, ElementField::Cu), ">=", 2.5));
            t.conditions.push_back(makeCondition("CU-3-C2", "Zn strongly elevated (VMS massive sulfide)", "local_z[Zn]", z(ctx, ElementField::Zn), ">=", 2.5));
            t.conditions.push_back(makeCondition("CU-3-C3", "Pb elevated (polymetallic massive sulfide)", "local_z[Pb]", z(ctx, ElementField::Pb), ">=", 2.0));
            t.conditions.push_back(makeCondition("CU-3-C4", "Zn/(Zn+Pb) ≥ 0.65 — VMS-style Zn dominance", "Zn_Pb_ratio", ratio, "==", pass_c4 ? ratio : -999.0)); // Custom pass implementation
            t.conditions.push_back(makeCondition("CU-3-C5", "Mo at background levels (z < 1.0) — rules out porphyry core", "pass_c5", pass_c5 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Cu, ElementField::Zn, ElementField::Pb, ElementField::Mo};
        finalise(result, Target::COPPER_VMS, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalLithiumPegmatite(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Rb) && !hasZ(ctx, ElementField::Cs) && !hasZ(ctx, ElementField::Li)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::LITHIUM_PEGMATITE;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(1);

        // RULE LI-1
        {
            RuleTrace t;
            t.rule_id = "LI-1";
            t.rule_name = "LCT pegmatite suite — K/Rb fractionation signature";
            t.scientific_basis = "Open University (UK) LCT pegmatite research; Manitoba Geological Survey; Semantic Scholar Li exploration geochemistry. K/Rb < 30 = highly fractionated granitic melt → LCT pegmatite. Nb/Ta < 8 confirms LCT (not NYF) type. Rb+Cs+Sn+Ta = defining LCT suite. Li itself proxy.";
            t.score_contribution = 0.85;

            double k_rb = ctx.point->K_Rb_ratio;
            bool pass_c3 = !hasData(k_rb) || k_rb < 30.0;

            double nb_ta = ctx.point->Nb_Ta_ratio;
            bool pass_c4 = !hasData(nb_ta) || nb_ta < 8.0;

            double z_sn = z(ctx, ElementField::Sn);
            double z_li = z(ctx, ElementField::Li);
            double z_ta = z(ctx, ElementField::Ta);
            bool pass_c5 = (hasData(z_sn) && z_sn >= 1.5) || (hasData(z_li) && z_li >= 1.5) || (hasData(z_ta) && z_ta >= 1.5);

            t.conditions.push_back(makeCondition("LI-1-C1", "Rb anomalous — LCT fractionation indicator", "local_z[Rb]", z(ctx, ElementField::Rb), ">=", 2.0));
            t.conditions.push_back(makeCondition("LI-1-C2", "Cs anomalous — LCT pegmatite suite", "local_z[Cs]", z(ctx, ElementField::Cs), ">=", 2.0));
            t.conditions.push_back(makeCondition("LI-1-C3", "K/Rb < 30 — highly fractionated system", "K_Rb_ratio", k_rb, "==", pass_c3 ? k_rb : -999.0));
            t.conditions.push_back(makeCondition("LI-1-C4", "Nb/Ta < 8 — LCT type confirmed", "Nb_Ta_ratio", nb_ta, "==", pass_c4 ? nb_ta : -999.0));
            t.conditions.push_back(makeCondition("LI-1-C5", "At least one of Sn, Li, Ta also elevated", "pass_c5", pass_c5 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Rb, ElementField::Cs, ElementField::Li, ElementField::Sn, ElementField::Ta, ElementField::K2O, ElementField::Nb};
        finalise(result, Target::LITHIUM_PEGMATITE, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalLithiumBrine(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Li)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::LITHIUM_BRINE;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(1);

        // RULE LI-2
        {
            RuleTrace t;
            t.rule_id = "LI-2";
            t.rule_name = "Brine/salt flat lithium (low Mg, high Na)";
            t.scientific_basis = "GWM Engineering Finland; 911metallurgist brine geochemistry. Salar-type Li: Mg/Li ratio < 6.5 (economic threshold). High Na, low Mg. Atacama, Tibetan Plateau style deposits.";
            t.score_contribution = 0.55;

            double raw_mgo = raw(ctx, ElementField::MgO);
            double z_mgo = z(ctx, ElementField::MgO);
            bool pass_c2 = !hasData(raw_mgo) || raw_mgo < 0.5 || (hasData(z_mgo) && z_mgo < -1.0);

            double raw_na2o = raw(ctx, ElementField::Na2O);
            bool pass_c3 = (hasData(raw_na2o) && raw_na2o >= 1.5) || z(ctx, ElementField::Na2O) >= 1.5;

            double mg_li = ctx.point->Mg_Li_ratio;
            bool pass_c4 = !hasData(mg_li) || mg_li < 6.5;

            double raw_k2o = raw(ctx, ElementField::K2O);
            double z_k2o = z(ctx, ElementField::K2O);
            bool is_potassic = (hasData(raw_k2o) && raw_k2o >= 4.0) || (hasData(z_k2o) && z_k2o >= 2.0);
            bool pass_c5 = !is_potassic;

            t.conditions.push_back(makeCondition("LI-2-C1", "Li strongly anomalous", "local_z[Li]", z(ctx, ElementField::Li), ">=", 2.5));
            t.conditions.push_back(makeCondition("LI-2-C2", "MgO low (<0.5%) or depleted — brine indicator", "pass_c2", pass_c2 ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("LI-2-C3", "Na2O >= 1.5% or z(Na2O) >= 1.5 — saline environment", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));
            t.conditions.push_back(makeCondition("LI-2-C4", "Mg/Li < 6.5 — economic brine threshold", "Mg_Li_ratio", mg_li, "==", pass_c4 ? mg_li : -999.0));
            t.conditions.push_back(makeCondition("LI-2-C5", "K2O low (<4.0%) — rules out granite weather signature", "pass_c5", pass_c5 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Li, ElementField::MgO, ElementField::Na2O, ElementField::K2O};
        finalise(result, Target::LITHIUM_BRINE, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalREE(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::La) && !hasZ(ctx, ElementField::Ce)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::REE_CARBONATITE;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(2);

        // RULE REE-1
        {
            RuleTrace t;
            t.rule_id = "REE-1";
            t.rule_name = "Carbonatite LREE enrichment (Mountain Pass / Bayan Obo style)";
            t.scientific_basis = "Applied Geochemistry carbonatite reviews; USGS Critical Minerals REE program; Geoscience Frontiers. Carbonatites: SiO2 <10-20%, strongly LREE-enriched (La/Yb >> 10), Nb in pyrochlore, Th/U elevated, P2O5 in apatite.";
            t.score_contribution = 0.85;

            double raw_sio2 = raw(ctx, ElementField::SiO2);
            bool pass_c5 = !hasData(raw_sio2) || raw_sio2 < 20.0;

            t.conditions.push_back(makeCondition("REE-1-C1", "La strongly anomalous — LREE enrichment", "local_z[La]", z(ctx, ElementField::La), ">=", 2.5));
            t.conditions.push_back(makeCondition("REE-1-C2", "Ce strongly anomalous — LREE suite", "local_z[Ce]", z(ctx, ElementField::Ce), ">=", 2.5));
            t.conditions.push_back(makeCondition("REE-1-C3", "Nb elevated — pyrochlore pyrochlore indicator", "local_z[Nb]", z(ctx, ElementField::Nb), ">=", 2.0));
            t.conditions.push_back(makeCondition("REE-1-C4", "Th elevated — monazite/perovskite in carbonatite", "local_z[Th]", z(ctx, ElementField::Th), ">=", 2.0));
            t.conditions.push_back(makeCondition("REE-1-C5", "SiO2 <20% — carbonatite is SiO2-poor", "pass_c5", pass_c5 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE REE-2
        {
            RuleTrace t;
            t.rule_id = "REE-2";
            t.rule_name = "REE-phosphate suite (apatite enrichment)";
            t.scientific_basis = "Applied Geochemistry; apatite is the primary REE carrier in many carbonatite/hydrothermal systems. P2O5 > 1% indicates apatite-rich rock.";
            t.score_contribution = 0.55;

            t.conditions.push_back(makeCondition("REE-2-C1", "La elevated in phosphate suite", "local_z[La]", z(ctx, ElementField::La), ">=", 2.0));
            t.conditions.push_back(makeCondition("REE-2-C2", "Y elevated — heavy REE potential", "local_z[Y]", z(ctx, ElementField::Y), ">=", 2.0));
            t.conditions.push_back(makeCondition("REE-2-C3", "P2O5 elevated — apatite carrier indicator", "local_z[P2O5]", z(ctx, ElementField::P2O5), ">=", 1.5));
            t.conditions.push_back(makeCondition("REE-2-C4", "Sr elevated — common carbonatite suite element", "local_z[Sr]", z(ctx, ElementField::Sr), ">=", 1.5));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::La, ElementField::Ce, ElementField::Nb, ElementField::Th, ElementField::SiO2, ElementField::Y, ElementField::P2O5, ElementField::Sr};
        finalise(result, Target::REE_CARBONATITE, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalZincLead(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Zn) && !hasZ(ctx, ElementField::Pb)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::ZINC_LEAD;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(2);

        // RULE ZN-1
        {
            RuleTrace t;
            t.rule_id = "ZN-1";
            t.rule_name = "SEDEX Zn-Pb-Ba stratiform signature";
            t.scientific_basis = "USGS Mineral Resources; Geoscience World Mineralium Deposita. SEDEX (Sedimentary Exhalative): Zn+Pb+Ba trio. Ba (barite) precipitates at sediment-water interface — STAYS CLOSE TO SOURCE (immobile). Cd strongly correlates with Zn in sphalerite.";
            t.score_contribution = 0.85;

            double z_cd = z(ctx, ElementField::Cd);
            double raw_cd = raw(ctx, ElementField::Cd);
            bool pass_c4 = (hasData(z_cd) && z_cd >= 1.5) || !hasData(raw_cd);

            t.conditions.push_back(makeCondition("ZN-1-C1", "Zn strongly anomalous (SEDEX core)", "local_z[Zn]", z(ctx, ElementField::Zn), ">=", 2.5));
            t.conditions.push_back(makeCondition("ZN-1-C2", "Pb elevated (stratiform sulfide)", "local_z[Pb]", z(ctx, ElementField::Pb), ">=", 2.0));
            t.conditions.push_back(makeCondition("ZN-1-C3", "Ba anomalous — barite near-source indicator", "local_z[Ba]", z(ctx, ElementField::Ba), ">=", 2.0));
            t.conditions.push_back(makeCondition("ZN-1-C4", "Cd elevated — sphalerite Cd/Zn fingerprint", "pass_c4", pass_c4 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        // RULE ZN-2
        {
            RuleTrace t;
            t.rule_id = "ZN-2";
            t.rule_name = "MVT Zn-Pb fluorite-bearing system";
            t.scientific_basis = "USGS; Geoscience World. Mississippi Valley Type (MVT) = carbonate-hosted. F (fluorite) + Pb + Zn + Ag. Less Ba than SEDEX.";
            t.score_contribution = 0.55;

            double z_f = z(ctx, ElementField::F);
            double z_ag = z(ctx, ElementField::Ag);
            bool pass_c3 = (hasData(z_f) && z_f >= 1.5) || (hasData(z_ag) && z_ag >= 1.5);

            t.conditions.push_back(makeCondition("ZN-2-C1", "Zn elevated (carbonate host)", "local_z[Zn]", z(ctx, ElementField::Zn), ">=", 2.0));
            t.conditions.push_back(makeCondition("ZN-2-C2", "Pb elevated (carbonate host)", "local_z[Pb]", z(ctx, ElementField::Pb), ">=", 2.0));
            t.conditions.push_back(makeCondition("ZN-2-C3", "F (fluorite) or Ag elevated — MVT system indicators", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));

            t.fired = true;
            for (const auto& c : t.conditions) {
                if (!c.passed) { t.fired = false; break; }
            }
            traces.push_back(t);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Zn, ElementField::Pb, ElementField::Ba, ElementField::Cd, ElementField::F, ElementField::Ag};
        finalise(result, Target::ZINC_LEAD, traces, ctx, elements_used);
        return result;
    }

    RuleResult RuleEngine::evalGossan(const PointContext& ctx) const {
        if (!hasZ(ctx, ElementField::Fe)) {
            RuleResult r;
            r.sufficient_data = false;
            r.risk_level = RiskLevel::INSUFFICIENT_DATA;
            r.target = Target::GOSSAN;
            return r;
        }

        std::vector<RuleTrace> traces;
        traces.reserve(2);

        // RULE GOS-1
        RuleTrace t1;
        {
            t1.rule_id = "GOS-1";
            t1.rule_name = "Gossan — oxidised sulfide cap (Fe-oxide + residual pathfinders)";
            t1.scientific_basis = "Economic Geology; Australian Geological Survey Organisation; 911metallurgist gossan geochemistry. Goethite/hematite/limonite cap. Immobile residual elements (As, Sb, Bi, Pb, Au, Ag) accumulate. Mobile elements (Cu, Zn) leached into supergene zone below. High Pb residual = IMMATURE gossan (close to sulfide source).";
            t1.score_contribution = 0.85;

            double raw_fe = raw(ctx, ElementField::Fe);
            double z_fe = z(ctx, ElementField::Fe);
            bool pass_c2 = (hasData(raw_fe) && raw_fe >= 10.0) || (hasData(z_fe) && z_fe >= 3.5);

            double z_as = z(ctx, ElementField::As);
            double z_pb = z(ctx, ElementField::Pb);
            bool pass_c3 = (hasData(z_as) && z_as >= 1.5) || (hasData(z_pb) && z_pb >= 2.0);

            double z_sio2 = z(ctx, ElementField::SiO2);
            double raw_sio2 = raw(ctx, ElementField::SiO2);
            bool pass_c4 = (hasData(z_sio2) && z_sio2 >= 1.0) || (hasData(raw_sio2) && raw_sio2 >= 60.0);

            t1.conditions.push_back(makeCondition("GOS-1-C1", "Fe strongly anomalous — iron oxide cap", "local_z[Fe]", z_fe, ">=", 3.0));
            t1.conditions.push_back(makeCondition("GOS-1-C2", "Fe >10% weight — massive iron oxide accumulation", "pass_c2", pass_c2 ? 1.0 : 0.0, "==", 1.0));
            t1.conditions.push_back(makeCondition("GOS-1-C3", "Residual As or Pb — sulfide-derived immobile elements", "pass_c3", pass_c3 ? 1.0 : 0.0, "==", 1.0));
            t1.conditions.push_back(makeCondition("GOS-1-C4", "Siliceous gossan — quartz cap common on massive sulfide", "pass_c4", pass_c4 ? 1.0 : 0.0, "==", 1.0));

            t1.fired = true;
            for (const auto& c : t1.conditions) {
                if (!c.passed) { t1.fired = false; break; }
            }
            traces.push_back(t1);
        }

        // RULE GOS-2
        {
            RuleTrace t2;
            t2.rule_id = "GOS-2";
            t2.rule_name = "Gossan maturity — Pb residual indicates proximity to sulfide source";
            t2.scientific_basis = "Economic Geology; Broken Hill gossan studies. High Pb in gossan = immature = recently oxidised = sulfide source nearby. Low Pb, high Au/As = mature gossan = thoroughly leached.";
            t2.score_contribution = 0.30;

            t2.conditions.push_back(makeCondition("GOS-2-C1", "Gossan detected (GOS-1 fired)", "gos_1_fired", t1.fired ? 1.0 : 0.0, "==", 1.0));
            t2.conditions.push_back(makeCondition("GOS-2-C2", "High Pb residual — IMMATURE gossan, sulfide source proximal", "local_z[Pb]", z(ctx, ElementField::Pb), ">=", 2.5));

            t2.fired = true;
            for (const auto& c : t2.conditions) {
                if (!c.passed) { t2.fired = false; break; }
            }
            traces.push_back(t2);
        }

        RuleResult result;
        std::vector<ElementField> elements_used = {ElementField::Fe, ElementField::As, ElementField::Pb, ElementField::SiO2};
        finalise(result, Target::GOSSAN, traces, ctx, elements_used);
        return result;
    }
}
