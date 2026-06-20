#pragma once
#include <string>
#include <sstream>
#include "DataTypes.h"

inline std::string jsEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += (char)c; break;
        }
    }
    return out;
}

inline std::string jsFmt(double v, int prec = 6) {
    if (v == NO_DATA_VAL || v != v) return "null";
    std::ostringstream ss;
    ss << std::fixed;
    ss.precision(prec);
    ss << v;
    std::string s = ss.str();
    if (s.find('.') != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

// Use R"HTML(...)HTML" delimiter throughout to avoid )"  closing raw strings when
// HTML attributes like onclick="func()" or JS regex like "([^"]+)" appear.
inline std::string sharedHead(const std::string& title, const std::string& tier) {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>)HTML" + title + R"HTML(</title>
  <meta name="description" content="GeoIQ )HTML" + tier + R"HTML( - Environmental and geological data analysis."/>
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.5.3/dist/MarkerCluster.css"/>
  <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.5.3/dist/MarkerCluster.Default.css"/>
  <script src="https://unpkg.com/leaflet.markercluster@1.5.3/dist/leaflet.markercluster.js"></script>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700;800&family=JetBrains+Mono:wght@400;700&display=swap" rel="stylesheet"/>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    :root {
      --bg-deep:    #0d1117;
      --bg-card:    rgba(22, 27, 34, 0.92);
      --bg-input:   rgba(13,17,23,0.7);
      --border:     rgba(48,54,61,0.8);
      --border-hi:  rgba(88,166,255,0.4);
      --text-main:  #c9d1d9;
      --text-dim:   #8b949e;
      --text-bright:#e6edf3;
      --accent:     #388bfd;
      --green:      #22c55e;
      --yellow:     #ffe066;
      --amber:      #ffcc00;
      --orange:     #ff8800;
      --red:        #ff3333;
      --gold:       #ffd700;
      --font-main:  'Outfit', -apple-system, BlinkMacSystemFont, sans-serif;
      --font-mono:  'JetBrains Mono', 'Fira Code', monospace;
    }
    html, body { height: 100%; background: var(--bg-deep); color: var(--text-main); font-family: var(--font-main); overflow: hidden; }
    #map { position: absolute; inset: 0; z-index: 0; }
    .leaflet-container { background: #0a0f14; }
    .panel {
      position: absolute; top: 12px; right: 12px; width: 360px;
      max-height: calc(100vh - 24px);
      background: var(--bg-card); backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--border); border-radius: 16px;
      box-shadow: 0 8px 40px rgba(0,0,0,0.6), 0 0 0 1px rgba(255,255,255,0.03);
      z-index: 900; display: flex; flex-direction: column; overflow: hidden;
    }
    .panel-header { padding: 14px 18px 12px; border-bottom: 1px solid var(--border); display: flex; align-items: center; gap: 12px; flex-shrink: 0; }
    .panel-logo { font-size: 1.4em; }
    .panel-title { font-size: 1.05rem; font-weight: 700; color: var(--text-bright); }
    .panel-subtitle { font-size: 0.72rem; color: var(--text-dim); margin-top: 1px; }
    .panel-body { overflow-y: auto; flex: 1; padding: 14px 16px; scrollbar-width: thin; scrollbar-color: var(--border) transparent; }
    .card { background: rgba(255,255,255,0.03); border: 1px solid var(--border); border-radius: 10px; padding: 12px 14px; margin-bottom: 10px; }
    .card-title { font-size: 0.72rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-dim); margin-bottom: 8px; }
    .badge { display: inline-block; padding: 2px 10px; border-radius: 99px; font-size: 0.72rem; font-weight: 700; letter-spacing: 0.05em; text-transform: uppercase; }
    .badge-CRITICAL    { background: rgba(255,51,51,0.18);   color: var(--red);    border: 1px solid rgba(255,51,51,0.4);   }
    .badge-HIGH        { background: rgba(255,136,0,0.18);   color: var(--orange); border: 1px solid rgba(255,136,0,0.4);   }
    .badge-MEDIUM_HIGH { background: rgba(255,204,0,0.18);   color: var(--amber);  border: 1px solid rgba(255,204,0,0.4);   }
    .badge-MEDIUM      { background: rgba(255,224,102,0.18); color: var(--yellow); border: 1px solid rgba(255,224,102,0.4); }
    .badge-LOW         { background: rgba(34,197,94,0.15);   color: var(--green);  border: 1px solid rgba(34,197,94,0.4);   }
    .map-header { position: absolute; top: 12px; left: 60px; z-index: 900; background: var(--bg-card); backdrop-filter: blur(16px); border: 1px solid var(--border); border-radius: 12px; padding: 10px 18px; box-shadow: 0 4px 24px rgba(0,0,0,0.5); display: flex; align-items: center; gap: 12px; }
    .map-header-emoji { font-size: 1.5em; }
    .map-header h1 { font-size: 1.1rem; font-weight: 800; color: var(--text-bright); }
    .map-header p  { font-size: 0.72rem; color: var(--text-dim); }
    .tier-nav { position: absolute; bottom: 24px; left: 50%; transform: translateX(-50%); z-index: 900; display: flex; gap: 8px; background: var(--bg-card); border: 1px solid var(--border); border-radius: 99px; padding: 6px 10px; backdrop-filter: blur(16px); }
    .tier-btn { padding: 6px 18px; border-radius: 99px; font-size: 0.78rem; font-weight: 600; cursor: pointer; border: none; color: var(--text-dim); background: transparent; transition: all 0.2s; text-decoration: none; display: inline-flex; align-items: center; gap: 6px; }
    .tier-btn:hover  { color: var(--text-bright); background: rgba(255,255,255,0.06); }
    .tier-btn.active { background: var(--accent); color: #fff; }
    .detail-section { margin-bottom: 12px; }
    .detail-label { font-size: 0.68rem; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-dim); margin-bottom: 4px; }
    .detail-value { font-size: 0.88rem; color: var(--text-bright); }
    .detail-value.mono { font-family: var(--font-mono); font-size: 0.8rem; }
    .ev-table { width: 100%; border-collapse: collapse; font-size: 0.75rem; margin-top: 6px; }
    .ev-table th { text-align: left; color: var(--text-dim); font-weight: 600; padding: 3px 6px; border-bottom: 1px solid var(--border); }
    .ev-table td { padding: 3px 6px; color: var(--text-main); vertical-align: top; }
    .ev-table tr:hover td { background: rgba(255,255,255,0.03); }
    .enriched { color: #f85149; }
    .depleted  { color: #58a6ff; }
    .bg-state  { color: var(--text-dim); }
    .disclaimer { font-size: 0.68rem; color: var(--text-dim); background: rgba(255,255,255,0.02); border: 1px solid var(--border); border-radius: 8px; padding: 8px 12px; margin-top: 10px; line-height: 1.5; }
    .sep { border: none; border-top: 1px solid var(--border); margin: 10px 0; }
    .legend-row { display: flex; align-items: center; gap: 10px; margin-bottom: 6px; font-size: 0.8rem; }
    .legend-dot { width: 12px; height: 12px; border-radius: 50%; flex-shrink: 0; }
    .filter-input { width: 100%; background: var(--bg-input); border: 1px solid var(--border); border-radius: 8px; color: var(--text-main); font-family: var(--font-mono); font-size: 0.78rem; padding: 8px 12px; outline: none; transition: border-color 0.2s; margin-bottom: 8px; }
    .filter-input:focus { border-color: var(--border-hi); }
    .panel-body::-webkit-scrollbar { width: 4px; }
    .panel-body::-webkit-scrollbar-thumb { background: var(--border); border-radius: 4px; }
    .toggle-group { display: flex; gap: 6px; flex-wrap: wrap; margin-bottom: 10px; }
    .toggle-pill { padding: 4px 14px; border-radius: 99px; font-size: 0.75rem; font-weight: 600; cursor: pointer; border: 1px solid var(--border); color: var(--text-dim); background: transparent; transition: all 0.18s; }
    .toggle-pill.on { background: rgba(56,139,253,0.18); color: var(--accent); border-color: rgba(56,139,253,0.5); }
    .stat-big { font-size: 2rem; font-weight: 800; color: var(--text-bright); line-height: 1; }
    .stat-lbl { font-size: 0.7rem; color: var(--text-dim); margin-top: 2px; }
    .stat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .stat-box { background: rgba(255,255,255,0.03); border: 1px solid var(--border); border-radius: 8px; padding: 10px 12px; }
    .ev-formula { font-family: var(--font-mono); font-size: 0.7rem; color: var(--text-dim); margin-top: 2px; word-break: break-all; }
    .model-block { background: rgba(255,255,255,0.02); border: 1px solid var(--border); border-radius: 8px; padding: 10px 12px; margin-bottom: 8px; }
    .model-block-title { font-size: 0.72rem; font-weight: 700; text-transform: uppercase; letter-spacing: 0.08em; color: var(--accent); margin-bottom: 6px; }
    .target-row { display: flex; align-items: center; justify-content: space-between; padding: 5px 0; border-bottom: 1px solid rgba(48,54,61,0.5); }
    .target-row:last-child { border-bottom: none; }
  </style>
)HTML";
}

// Serialize one GroupedRecord as compact JS object with tier-based filtering:
// tier == 1: Public Safety (Health only, no geology targets, no mine proximities, no raw z-scores/formulas)
// tier == 2: Advanced Research (Geology + Health, no mine proximities)
// tier == 3: Full Audit (Include everything)
inline std::string serializeGroupToJS(const GroupedRecord& grp, int tier) {
    std::ostringstream o;
    o << "{";
    o << "\"id\":\"" << jsEscape(grp.group_id) << "\",";
    o << "\"gsid\":\"" << jsEscape(grp.geo_sample_id) << "\",";
    o << "\"lat\":" << jsFmt(grp.geo_lat, 6) << ",";
    o << "\"lon\":" << jsFmt(grp.geo_lon, 6) << ",";
    
    // Suppress geology risk for Tier 1
    if (tier == 1) {
        o << "\"wgr\":\"LOW\",";
    } else {
        o << "\"wgr\":\"" << grp.worst_geology_risk << "\",";
    }
    o << "\"whr\":\"" << grp.worst_health_risk  << "\",";
    o << "\"has_h\":" << (grp.has_health_pair ? "true" : "false") << ",";
    
    if (grp.has_health_pair) {
        o << "\"hsid\":\"" << jsEscape(grp.health_sample_id) << "\",";
        o << "\"hd\":" << jsFmt(grp.pair_distance_km, 2) << ",";
    }
    
    // Only Tier 3 includes mine proximity details
    if (tier == 3 && !grp.nearest_mine_name.empty()) {
        o << "\"mine\":\"" << jsEscape(grp.nearest_mine_name) << "\",";
        o << "\"mine_t\":\"" << jsEscape(grp.nearest_mine_target) << "\",";
        o << "\"mine_d\":" << jsFmt(grp.nearest_mine_dist_km, 2) << ",";
    }
    
    // Serialize raw elemental values
    o << "\"raw\":{";
    bool first = true;
    for (const auto& re : grp.raw_elements) {
        if (!first) o << ","; first = false;
        o << "\"" << jsEscape(re.first) << "\":" << jsFmt(re.second, 4);
    }
    o << "},";
    
    // Geology targets (completely stripped for Tier 1)
    o << "\"geo\":[";
    if (tier > 1) {
        for (size_t i = 0; i < grp.geology_targets.size(); ++i) {
            const auto& t = grp.geology_targets[i];
            if (i) o << ",";
            o << "{\"tgt\":\"" << jsEscape(t.target) << "\","
              << "\"rl\":\"" << t.risk_level << "\","
              << "\"sc\":" << jsFmt(t.score, 3) << ","
              << "\"cf\":" << jsFmt(t.confidence, 3) << ","
              << "\"ann\":\"" << jsEscape(t.annotation) << "\","
              << "\"rules\":[";
            for (size_t r = 0; r < t.triggered_rules.size(); ++r) {
                if (r) o << ",";
                o << "\"" << jsEscape(t.triggered_rules[r]) << "\"";
            }
            o << "],\"sc_cons\":" << jsFmt(t.spatial_consistency, 3) << ","
              << "\"disp\":" << (t.dispersion_detected ? "true" : "false") << ","
              << "\"ev\":[";
            for (size_t e = 0; e < t.evidence.size(); ++e) {
                const auto& ev = t.evidence[e];
                if (e) o << ",";
                o << "{\"el\":\"" << jsEscape(ev.element) << "\","
                  << "\"u\":\"" << jsEscape(ev.unit) << "\","
                  << "\"v\":" << jsFmt(ev.raw_value, 4) << ","
                  << "\"z\":" << jsFmt(ev.local_z, 4) << ","
                  << "\"a\":" << jsFmt(ev.alpha, 4) << ","
                  << "\"st\":\"" << jsEscape(ev.singularity_state) << "\","
                  << "\"fm\":\"" << jsEscape(ev.formula) << "\"}";
            }
            o << "]}";
        }
    }
    o << "],";
    
    // Health targets
    o << "\"hlt\":[";
    for (size_t i = 0; i < grp.health_targets.size(); ++i) {
        const auto& t = grp.health_targets[i];
        if (i) o << ",";
        o << "{\"tgt\":\"" << jsEscape(t.target) << "\","
          << "\"rl\":\"" << t.risk_level << "\","
          << "\"sc\":" << jsFmt(t.score, 3) << ","
          << "\"ann\":\"" << jsEscape(t.annotation) << "\","
          << "\"rules\":[";
        for (size_t r = 0; r < t.triggered_rules.size(); ++r) {
            if (r) o << ",";
            o << "\"" << jsEscape(t.triggered_rules[r]) << "\"";
        }
        o << "],\"ev\":[";
        for (size_t e = 0; e < t.evidence.size(); ++e) {
            const auto& ev = t.evidence[e];
            if (e) o << ",";
            o << "{\"el\":\"" << jsEscape(ev.element) << "\","
              << "\"u\":\"" << jsEscape(ev.unit) << "\","
              << "\"v\":" << jsFmt(ev.raw_value, 4) << ",";
            
            // Strip out raw Z-scores and formulas for Tier 1
            if (tier == 1) {
                o << "\"z\":null,\"fm\":\"\"}";
            } else {
                o << "\"z\":" << jsFmt(ev.local_z, 4) << ","
                  << "\"fm\":\"" << jsEscape(ev.formula) << "\"}";
            }
        }
        o << "]}";
    }
    o << "]}";
    return o.str();
}

inline std::string serializeAllGroups(const std::vector<GroupedRecord>& groups, int tier) {
    std::ostringstream o;
    o << "[\n";
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i) o << ",\n";
        o << serializeGroupToJS(groups[i], tier);
    }
    o << "\n]";
    return o.str();
}
