#pragma once
#include "HtmlCommon.h"
#include <fstream>
#include <vector>

inline void writeTier1(
    const std::string& outputPath,
    const std::vector<GroupedRecord>& groups,
    const std::string& toxJsonRaw)
{
    std::ofstream out(outputPath);
    if (!out.is_open()) { std::cerr << "[Tier1] Cannot write: " << outputPath << "\n"; return; }

    out << sharedHead("GeoHealth - Community Safety View", "Tier 1 Public Safety");

    // Use R"HTML(...)HTML" delimiter - safe against onclick="func()" patterns
    out << R"HTML(  <style>
    .panel { width: 380px; }
    .action-card { border-radius: 10px; padding: 12px 14px; margin-bottom: 10px; border: 1px solid; }
    .action-card.critical { background: rgba(255,51,51,0.1);  border-color: rgba(255,51,51,0.4);  }
    .action-card.high     { background: rgba(255,136,0,0.1);  border-color: rgba(255,136,0,0.4);  }
    .action-card.medium   { background: rgba(255,204,0,0.08); border-color: rgba(255,204,0,0.3);  }
    .action-card.low      { background: rgba(34,197,94,0.08); border-color: rgba(34,197,94,0.3);  }
    .action-title { font-size: 0.9rem; font-weight: 700; margin-bottom: 6px; }
    .action-title.critical { color: var(--red);    }
    .action-title.high     { color: var(--orange); }
    .action-title.medium   { color: var(--amber);  }
    .action-title.low      { color: var(--green);  }
    .action-body { font-size: 0.8rem; line-height: 1.55; color: var(--text-main); }
    .symptom-list { margin: 6px 0 0 12px; }
    .symptom-list li { margin-bottom: 3px; font-size: 0.78rem; }
    .el-bar { margin-bottom: 8px; }
    .el-bar-label { display: flex; justify-content: space-between; font-size: 0.75rem; margin-bottom: 3px; }
    .el-bar-track { background: rgba(255,255,255,0.07); border-radius: 4px; height: 6px; overflow: hidden; }
    .el-bar-fill  { height: 100%; border-radius: 4px; transition: width 0.4s; }
    .info-guide { background: rgba(56,139,253,0.06); border: 1px solid rgba(56,139,253,0.2); border-radius: 8px; padding: 10px 12px; margin-bottom: 10px; }
    .info-guide h4 { font-size: 0.78rem; color: var(--accent); margin-bottom: 6px; font-weight: 700; }
    .info-guide p  { font-size: 0.75rem; line-height: 1.5; color: var(--text-main); }
  </style>
</head>
<body>
<div id="map"></div>

<div class="map-header">
  <span class="map-header-emoji">&#x1F9BA;</span>
  <div>
    <h1>GeoHealth - Community Safety</h1>
    <p>Environmental contamination risk - Click any marker for details</p>
  </div>
</div>

<div class="tier-nav">
  <span class="tier-btn active">&#x1F9BA; Public Safety</span>
  <a class="tier-btn" href="tier2_advanced.html">&#x2697;&#xFE0F; Advanced Research</a>
  <a class="tier-btn" href="tier3_auditor.html">&#x1F50D; Full Audit</a>
</div>

<div class="panel" id="mainPanel">
  <div class="panel-header">
    <span class="panel-logo">&#x1F30D;</span>
    <div>
      <div class="panel-title">GeoHealth Dashboard</div>
      <div class="panel-subtitle">Public Safety View &middot; WHO/EPA Standards</div>
    </div>
  </div>
  <div class="panel-body" id="panelBody">

    <div class="card" id="summaryCard">
      <div class="card-title">Area Health Summary</div>
      <div class="stat-grid">
        <div class="stat-box"><div class="stat-big" id="statTotal">-</div><div class="stat-lbl">Locations Monitored</div></div>
        <div class="stat-box"><div class="stat-big" style="color:var(--red)" id="statElev">-</div><div class="stat-lbl">Elevated Risk Sites</div></div>
      </div>
    </div>

    <div class="info-guide">
      <h4>&#x1F4CB; How to Read Environmental Test Results</h4>
      <p>
        <strong>ppm</strong> = parts per million = mg/L in water or mg/kg in soil.<br/>
        <strong>ppb</strong> = parts per billion = micrograms per litre.<br/>
        <strong>WHO limit for Arsenic:</strong> 0.01 ppm (10 ppb) in drinking water.<br/>
        If your test shows "As: 0.05 mg/L", that is 5x above the WHO guideline.
      </p>
    </div>

    <div class="card">
      <div class="card-title">Risk Level Legend</div>
      <div class="legend-row"><div class="legend-dot" style="background:#ff3333"></div><span>CRITICAL - Immediate action required</span></div>
      <div class="legend-row"><div class="legend-dot" style="background:#ff8800"></div><span>HIGH - Do not use this water source</span></div>
      <div class="legend-row"><div class="legend-dot" style="background:#ffcc00"></div><span>MEDIUM-HIGH - Caution, verify filtration</span></div>
      <div class="legend-row"><div class="legend-dot" style="background:#ffe066"></div><span>MEDIUM - Monitor, test regularly</span></div>
      <div class="legend-row"><div class="legend-dot" style="background:#22c55e"></div><span>LOW RISK - Routine monitoring</span></div>
    </div>

    <div id="detailArea" style="display:none">
      <hr class="sep"/>
      <div class="card-title" style="margin-bottom:8px">&#x1F4CD; Selected Location</div>
      <div id="detailContent"></div>
    </div>

    <div class="disclaimer">
      &#x2695;&#xFE0F; <strong>Medical Disclaimer:</strong> GeoHealth is a community screening tool.
      It is NOT a medical diagnosis. All risk assessments are based on WHO/ATSDR reference values.
      Consult a qualified health professional before clinical decisions.
    </div>
  </div>
</div>

<script>
)HTML";

    out << "const GROUPS = " << serializeAllGroups(groups, 1) << ";\n";
    out << "const TOXICOLOGY_DB = " << toxJsonRaw << ";\n";

    out << R"HTML(
const RISK_COLOUR = { CRITICAL:'#ff3333', HIGH:'#ff8800', MEDIUM_HIGH:'#ffcc00', MEDIUM:'#ffe066', LOW:'#22c55e' };
function riskCol(r) { return RISK_COLOUR[r] || '#22c55e'; }
// CONTRACT-01: NEVER output "SAFE"
const LOW_RISK_LABEL = 'LOW RISK \u2014 Background level. Continue routine monitoring.';

const TOX = {};
if (TOXICOLOGY_DB && TOXICOLOGY_DB.toxicants) {
  TOXICOLOGY_DB.toxicants.forEach(function(t) { TOX[t.element] = t; });
}

function getActionAdvice(element, riskLevel, rawValue) {
  var tx = TOX[element];
  var who = tx ? tx.who_guideline : null;
  if (riskLevel === 'CRITICAL' || riskLevel === 'HIGH') {
    var illness  = tx && tx.outbreaks && tx.outbreaks.length ? tx.outbreaks[0].illness : 'chemical toxicity';
    var symptoms = tx && tx.outbreaks && tx.outbreaks.length ? tx.outbreaks[0].symptoms : [];
    var ref      = tx && tx.outbreaks && tx.outbreaks.length ? tx.outbreaks[0].reference : '';
    return { cls: riskLevel === 'CRITICAL' ? 'critical' : 'high',
      label:     riskLevel === 'CRITICAL' ? 'CRITICAL HEALTH ALERT' : 'HIGH RISK - DO NOT DRINK',
      immediate: 'DO NOT use this water source for drinking or cooking. Report to local health authority immediately.',
      longterm:  'Install certified removal filtration. Arrange for laboratory water test. Seek alternative supply.',
      illness: illness, symptoms: symptoms, ref: ref, multiple: who ? (rawValue / who).toFixed(0) : null };
  } else if (riskLevel === 'MEDIUM_HIGH' || riskLevel === 'MEDIUM') {
    return { cls: riskLevel === 'MEDIUM_HIGH' ? 'high' : 'medium',
      label:     'ELEVATED RISK - CAUTION',
      immediate: 'Verify water source has appropriate filtration. Avoid unfiltered use for infants and pregnant women.',
      longterm:  'Schedule a certified laboratory test within 30 days.',
      illness: null, symptoms: [], ref: '', multiple: null };
  }
  return { cls: 'low', label: LOW_RISK_LABEL,
    immediate: 'Values are within background reference levels. Continue routine monitoring.',
    longterm:  'No action required. Re-test annually.',
    illness: null, symptoms: [], ref: '', multiple: null };
}

function renderDetail(grp) {
  var html = '';
  html += '<div class="detail-section"><div class="detail-label">Coordinates</div>';
  html += '<div class="detail-value">' + grp.lat.toFixed(5) + ', ' + grp.lon.toFixed(5) + '</div></div>';
  var whr = grp.whr || 'LOW';
  html += '<div class="detail-section"><div class="detail-label">Worst Health Risk</div>';
  html += '<span class="badge badge-' + whr + '">' + whr.replace('_',' ') + '</span></div>';

  if (grp.hlt && grp.hlt.length > 0) {
    grp.hlt.forEach(function(ht) {
      var el   = ht.ev && ht.ev.length > 0 ? ht.ev[0].el : ht.tgt.replace('_HAZARD','');
      var rawV = ht.ev && ht.ev.length > 0 ? ht.ev[0].v  : null;
      var adv  = getActionAdvice(el, ht.rl, rawV);
      var tx   = TOX[el];
      var who  = tx ? tx.who_guideline : null;
      html += '<div class="action-card ' + adv.cls + '">';
      html += '<div class="action-title ' + adv.cls + '">' + adv.label + '</div>';
      html += '<div class="action-body">';
      if (ht.ev && ht.ev.length > 0) {
        ht.ev.forEach(function(ev) {
          var pct = who && ev.v ? Math.min((ev.v / (who * 10)) * 100, 100) : 0;
          var col = riskCol(ht.rl);
          html += '<div class="el-bar">';
          html += '<div class="el-bar-label"><span><strong>' + ev.el + '</strong>: ';
          html += ev.v !== null ? ev.v.toFixed(3) + ' ' + ev.u : 'N/A';
          html += '</span><span style="color:var(--text-dim)">';
          html += who ? 'WHO: ' + who + ' mg/L' : '';
          html += '</span></div>';
          html += '<div class="el-bar-track"><div class="el-bar-fill" style="width:' + pct.toFixed(0) + '%;background:' + col + '"></div></div>';
          html += '</div>';
          if (who && ev.v) {
            var mult = (ev.v / who).toFixed(1);
            if (parseFloat(mult) > 1) {
              html += '<p style="font-size:0.75rem;color:' + col + ';margin:-4px 0 8px 0">&#9888;&#65039; ' + mult + 'x above WHO guideline</p>';
            }
          }
        });
      }
      if (adv.illness) {
        html += '<p><strong>Associated illness:</strong> ' + adv.illness + '</p>';
        if (adv.symptoms && adv.symptoms.length) {
          html += '<ul class="symptom-list">';
          adv.symptoms.forEach(function(s) { html += '<li>' + s + '</li>'; });
          html += '</ul>';
        }
        if (adv.ref) html += '<p style="margin-top:5px;color:var(--text-dim);font-size:0.72rem">' + adv.ref + '</p>';
      }
      html += '<hr class="sep" style="margin:8px 0"/>';
      html += '<p><strong>Immediate:</strong> ' + adv.immediate + '</p>';
      html += '<p style="margin-top:5px"><strong>Long-term:</strong> ' + adv.longterm + '</p>';
      html += '</div></div>';
    });
  } else {
    html += '<div class="action-card low"><div class="action-title low">' + LOW_RISK_LABEL + '</div>';
    html += '<div class="action-body">No elevated health hazards at this location. Continue routine monitoring.</div></div>';
  }

  document.getElementById('detailContent').innerHTML = html;
  document.getElementById('detailArea').style.display = 'block';
}

var map = L.map('map', { zoomControl: true, preferCanvas: true }).setView([0, 0], 2);
L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
  attribution: '&copy; OpenStreetMap &copy; CARTO', maxZoom: 19, subdomains: 'abcd'
}).addTo(map);

var cluster = L.markerClusterGroup({ maxClusterRadius: 40, chunkedLoading: true });
var bounds = [];

GROUPS.forEach(function(grp) {
  var risk = grp.whr !== 'LOW' ? grp.whr : grp.wgr;
  var col  = riskCol(risk);
  var r    = risk === 'CRITICAL' ? 12 : risk === 'HIGH' ? 9 : 7;
  var marker = L.circleMarker([grp.lat, grp.lon], {
    radius: r, fillColor: col, color: '#0d1117', weight: 1.5, fillOpacity: 0.88
  });
  marker.on('click', function() { renderDetail(grp); });
  cluster.addLayer(marker);
  bounds.push([grp.lat, grp.lon]);
});

map.addLayer(cluster);
if (bounds.length > 0) map.fitBounds(bounds, { padding: [40, 40] });

var elevated = GROUPS.filter(function(g) { return g.whr !== 'LOW' || g.wgr !== 'LOW'; });
document.getElementById('statTotal').textContent = GROUPS.length;
document.getElementById('statElev').textContent  = elevated.length;
</script>
</body>
</html>
)HTML";

    out.close();
    std::cout << "[Tier1] Written: " << outputPath << "\n";
}
