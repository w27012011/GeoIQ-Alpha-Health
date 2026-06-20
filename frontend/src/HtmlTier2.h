#pragma once
#include "HtmlCommon.h"
#include <fstream>
#include <vector>

inline void writeTier2(
    const std::string& outputPath,
    const std::vector<GroupedRecord>& groups)
{
    std::ofstream out(outputPath);
    if (!out.is_open()) { std::cerr << "[Tier2] Cannot write: " << outputPath << "\n"; return; }

    out << sharedHead("GeoIQ Research - Advanced Analysis", "Tier 2 Advanced");

    out << R"HTML(  <style>
    .panel { width: 400px; }
    .layer-toggle { display: flex; gap: 6px; margin-bottom: 10px; }
    .layer-btn { flex: 1; padding: 6px 8px; border-radius: 8px; font-size: 0.75rem; font-weight: 600; cursor: pointer; border: 1px solid var(--border); background: transparent; color: var(--text-dim); transition: all 0.18s; text-align: center; }
    .layer-btn.on { background: rgba(56,139,253,0.18); color: var(--accent); border-color: rgba(56,139,253,0.5); }
    .score-bar-track { background: rgba(255,255,255,0.07); border-radius: 3px; height: 4px; width: 80px; display: inline-block; overflow: hidden; }
    .score-bar-fill  { height: 100%; border-radius: 3px; }
    .filter-select { width: 100%; background: var(--bg-input); border: 1px solid var(--border); border-radius: 8px; color: var(--text-main); font-size: 0.78rem; padding: 7px 10px; outline: none; margin-bottom: 8px; cursor: pointer; }
    .conf-pip { width: 14px; height: 6px; border-radius: 2px; background: var(--border); display: inline-block; margin-right: 2px; }
    .conf-pip.on { background: var(--accent); }
    .filter-count { font-size: 0.72rem; color: var(--text-dim); }
  </style>
</head>
<body>
<div id="map"></div>

<div class="map-header">
  <span class="map-header-emoji">&#x2697;&#xFE0F;</span>
  <div>
    <h1>GeoIQ - Advanced Research View</h1>
    <p>Model output: Z-scores &middot; Singularity index &middot; Evidence tables</p>
  </div>
</div>

<div class="tier-nav">
  <a class="tier-btn" href="tier1_public.html">&#x1F9BA; Public Safety</a>
  <span class="tier-btn active">&#x2697;&#xFE0F; Advanced Research</span>
  <a class="tier-btn" href="tier3_auditor.html">&#x1F50D; Full Audit</a>
</div>

<div class="panel" id="mainPanel">
  <div class="panel-header">
    <span class="panel-logo">&#x2697;&#xFE0F;</span>
    <div>
      <div class="panel-title">Research Dashboard</div>
      <div class="panel-subtitle">GeoIQ + GeoHealth Model Outputs</div>
    </div>
  </div>
  <div class="panel-body">

    <div class="card">
      <div class="card-title">Map Layer</div>
      <div class="layer-toggle">
        <button class="layer-btn on" id="btnHealth" onclick="setLayer('health')">&#x1F9BA; Health</button>
        <button class="layer-btn"    id="btnGeo"    onclick="setLayer('geo')">&#x26CF;&#xFE0F; Geology</button>
        <button class="layer-btn"    id="btnBoth"   onclick="setLayer('both')">&#x1F500; Both</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Filter</div>
      <select class="filter-select" id="filterTarget" onchange="applyFilters()">
        <option value="">All Targets</option>
        <option value="ARSENIC_HAZARD">Arsenic Hazard</option>
        <option value="GOLD">Gold</option>
        <option value="COPPER_PORPHYRY">Copper Porphyry</option>
        <option value="LITHIUM_BRINE">Lithium Brine</option>
        <option value="LITHIUM_PEGMATITE">Lithium Pegmatite</option>
        <option value="DIAMOND">Diamond / Kimberlite</option>
        <option value="REE">REE Carbonatite</option>
        <option value="ZINC_LEAD">Zinc-Lead</option>
        <option value="GOSSAN">Gossan</option>
      </select>
      <select class="filter-select" id="filterRisk" onchange="applyFilters()">
        <option value="">All Risk Levels</option>
        <option value="CRITICAL">CRITICAL</option>
        <option value="HIGH">HIGH</option>
        <option value="MEDIUM_HIGH">MEDIUM-HIGH</option>
        <option value="MEDIUM">MEDIUM</option>
        <option value="LOW">LOW</option>
      </select>
      <div class="filter-count" id="filterCount"></div>
    </div>

    <div id="detailArea" style="display:none">
      <hr class="sep"/>
      <div id="detailContent"></div>
    </div>

    <div class="disclaimer">
      &#x2695;&#xFE0F; GeoHealth is a screening tool, NOT a diagnosis.
      Z-scores and singularity indices are computed by GeoIQ.exe and visualised here without recomputation.
    </div>
  </div>
</div>

<script>
)HTML";

    out << "const GROUPS = " << serializeAllGroups(groups, 2) << ";\n";

    out << R"HTML(
var RISK_COLOUR = { CRITICAL:'#ff3333', HIGH:'#ff8800', MEDIUM_HIGH:'#ffcc00', MEDIUM:'#ffe066', LOW:'#22c55e' };
function riskCol(r) { return RISK_COLOUR[r] || '#22c55e'; }
var LOW_RISK_LABEL = 'LOW RISK \u2014 Background level. Continue routine monitoring.';

var map = L.map('map', { preferCanvas: true }).setView([0, 0], 2);
L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
  attribution: '&copy; OpenStreetMap &copy; CARTO', maxZoom: 19, subdomains: 'abcd'
}).addTo(map);

var healthLayer = L.layerGroup();
var geoLayer    = L.layerGroup();
var currentLayer = 'health';
var filteredGroups = GROUPS.slice();

function stateClass(st) {
  if (!st) return 'bg-state';
  if (st === 'ENRICHED') return 'enriched';
  if (st === 'DEPLETED') return 'depleted';
  return 'bg-state';
}

function confPips(cf) {
  var n = Math.round((cf || 0) * 10);
  var h = '';
  for (var i = 0; i < 10; i++) h += '<span class="conf-pip' + (i < n ? ' on' : '') + '"></span>';
  return h;
}

function buildMarkers() {
  healthLayer.clearLayers();
  geoLayer.clearLayers();
  filteredGroups.forEach(function(grp) {
    var hRisk = grp.whr || 'LOW';
    var hMark = L.circleMarker([grp.lat, grp.lon], {
      radius: 8, fillColor: riskCol(hRisk), color: '#0d1117', weight: 1.5, fillOpacity: 0.85
    });
    hMark.on('click', function() { renderDetail(grp); });
    healthLayer.addLayer(hMark);

    var gRisk = grp.wgr || 'LOW';
    var gMark = L.circleMarker([grp.lat, grp.lon], {
      radius: 12, fillColor: 'transparent', color: riskCol(gRisk), weight: 2.5, fillOpacity: 0
    });
    gMark.on('click', function() { renderDetail(grp); });
    geoLayer.addLayer(gMark);
  });

  if (currentLayer === 'health' || currentLayer === 'both') map.addLayer(healthLayer);
  if (currentLayer === 'geo'    || currentLayer === 'both') map.addLayer(geoLayer);
  if (currentLayer === 'health') map.removeLayer(geoLayer);
  if (currentLayer === 'geo')    map.removeLayer(healthLayer);
}

function setLayer(mode) {
  currentLayer = mode;
  document.getElementById('btnHealth').className = 'layer-btn' + (mode === 'health' ? ' on' : '');
  document.getElementById('btnGeo').className    = 'layer-btn' + (mode === 'geo'    ? ' on' : '');
  document.getElementById('btnBoth').className   = 'layer-btn' + (mode === 'both'   ? ' on' : '');
  if (mode === 'health') { map.addLayer(healthLayer); map.removeLayer(geoLayer); }
  else if (mode === 'geo') { map.addLayer(geoLayer); map.removeLayer(healthLayer); }
  else { map.addLayer(healthLayer); map.addLayer(geoLayer); }
}

function applyFilters() {
  var tgtF  = document.getElementById('filterTarget').value;
  var riskF = document.getElementById('filterRisk').value;
  filteredGroups = GROUPS.filter(function(grp) {
    var all = (grp.geo || []).concat(grp.hlt || []);
    var tOk  = !tgtF  || all.some(function(t) { return t.tgt === tgtF; });
    var rOk  = !riskF || grp.wgr === riskF || grp.whr === riskF;
    return tOk && rOk;
  });
  document.getElementById('filterCount').textContent = 'Showing ' + filteredGroups.length + ' of ' + GROUPS.length;
  buildMarkers();
}

function renderDetail(grp) {
  var html = '';
  html += '<div class="detail-section"><div class="detail-label">Location</div>';
  html += '<div class="detail-value">' + grp.lat.toFixed(5) + ', ' + grp.lon.toFixed(5) + '</div>';
  html += '<div style="font-size:0.78rem;color:var(--text-dim)">Geo: ' + (grp.gsid || '-') + ' | Health: ' + (grp.hsid || '-') + '</div></div>';

  // GeoHealth model output
  if (grp.hlt && grp.hlt.length > 0) {
    html += '<div class="model-block"><div class="model-block-title">GeoHealth Model Output</div>';
    grp.hlt.forEach(function(ht) {
      var isLow = ht.rl === 'LOW';
      html += '<div class="target-row"><span style="font-size:0.8rem;font-weight:600">' + ht.tgt.replace(/_/g,' ') + '</span>';
      html += '<span class="badge badge-' + ht.rl + '">' + (isLow ? 'LOW' : ht.rl.replace('_',' ')) + '</span></div>';
      if (!isLow && ht.rules && ht.rules.length) {
        html += '<div style="font-size:0.72rem;color:var(--text-dim);margin:2px 0 4px">Rules: ' + ht.rules.join(', ') + '</div>';
      }
      if (!isLow && ht.ann) html += '<div style="font-size:0.75rem;margin-bottom:6px">' + ht.ann + '</div>';
      if (ht.ev && ht.ev.length) {
        html += '<table class="ev-table"><thead><tr><th>Element</th><th>Raw</th><th>Local Z</th><th>State</th></tr></thead><tbody>';
        ht.ev.forEach(function(ev) {
          var z  = ev.z !== null ? ev.z.toFixed(3) : '-';
          var cl = stateClass(ev.st);
          html += '<tr><td><strong>' + ev.el + '</strong></td>';
          html += '<td>' + (ev.v !== null ? ev.v.toFixed(3) + ' ' + ev.u : '-') + '</td>';
          html += '<td class="' + cl + '">' + z + '</td>';
          html += '<td class="' + cl + '">' + (ev.st || '-') + '</td></tr>';
          if (ev.fm) html += '<tr><td colspan="4" class="ev-formula">' + ev.fm + '</td></tr>';
        });
        html += '</tbody></table>';
      }
    });
    html += '</div>';
  }

  // GeoIQ geology model output
  if (grp.geo && grp.geo.length > 0) {
    html += '<div class="model-block"><div class="model-block-title">GeoIQ Geology Model Output</div>';
    grp.geo.forEach(function(gt) {
      var sc  = (gt.sc || 0) * 100;
      var col = riskCol(gt.rl);
      html += '<div class="target-row"><span style="font-size:0.78rem">' + gt.tgt.replace(/_/g,' ') + '</span>';
      html += '<div style="display:flex;align-items:center;gap:8px">';
      html += '<div class="score-bar-track"><div class="score-bar-fill" style="width:' + sc.toFixed(0) + '%;background:' + col + '"></div></div>';
      html += '<span class="badge badge-' + gt.rl + '" style="font-size:0.65rem">' + gt.rl.replace('_',' ') + '</span></div></div>';
    });
    grp.geo.filter(function(gt) { return gt.rl !== 'LOW'; }).forEach(function(gt) {
      html += '<hr class="sep"/><div style="font-size:0.8rem;font-weight:700;margin-bottom:4px">' + gt.tgt.replace(/_/g,' ') + '</div>';
      html += '<div style="font-size:0.75rem;color:var(--text-dim);margin-bottom:4px">Score: ' + gt.sc.toFixed(3) + ' | Confidence: ' + gt.cf.toFixed(3) + '</div>';
      html += confPips(gt.cf);
      if (gt.rules && gt.rules.length) html += '<div style="font-size:0.72rem;margin:4px 0">Rules: <strong>' + gt.rules.join(', ') + '</strong></div>';
      if (gt.ann) html += '<div style="font-size:0.75rem;margin:4px 0">' + gt.ann + '</div>';
      if (gt.disp) html += '<div style="font-size:0.72rem;color:var(--amber)">Dispersion train detected</div>';
      if (gt.ev && gt.ev.length) {
        html += '<table class="ev-table" style="margin-top:6px"><thead><tr><th>Element</th><th>Raw</th><th>Local Z</th><th>Alpha</th><th>State</th></tr></thead><tbody>';
        gt.ev.forEach(function(ev) {
          var z  = ev.z !== null ? ev.z.toFixed(3) : '-';
          var a  = ev.a !== null ? ev.a.toFixed(3) : '-';
          var cl = stateClass(ev.st);
          html += '<tr><td><strong>' + ev.el + '</strong></td>';
          html += '<td>' + (ev.v !== null ? ev.v.toFixed(3) + ' ' + ev.u : '-') + '</td>';
          html += '<td class="' + cl + '">' + z + '</td>';
          html += '<td style="font-family:var(--font-mono);font-size:0.7rem">' + a + '</td>';
          html += '<td class="' + cl + '">' + (ev.st || '-') + '</td></tr>';
          if (ev.fm) html += '<tr><td colspan="5" class="ev-formula">' + ev.fm + '</td></tr>';
        });
        html += '</tbody></table>';
      }
    });
    html += '</div>';
  }

  if (grp.mine) {
    html += '<div class="card"><div class="card-title">Nearest Known Mine</div>';
    html += '<div style="font-size:0.82rem"><strong>' + grp.mine + '</strong></div>';
    html += '<div style="font-size:0.75rem;color:var(--text-dim)">' + grp.mine_t + ' &middot; ' + (grp.mine_d ? grp.mine_d.toFixed(1) + ' km away' : '') + '</div></div>';
  }

  document.getElementById('detailContent').innerHTML = html;
  document.getElementById('detailArea').style.display = 'block';
}

buildMarkers();
applyFilters();
var allBounds = GROUPS.map(function(g) { return [g.lat, g.lon]; });
if (allBounds.length) map.fitBounds(allBounds, { padding: [40, 40] });
</script>
</body>
</html>
)HTML";

    out.close();
    std::cout << "[Tier2] Written: " << outputPath << "\n";
}
