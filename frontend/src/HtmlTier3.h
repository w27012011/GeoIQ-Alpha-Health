#pragma once
#include "HtmlCommon.h"
#include <fstream>
#include <vector>

inline void writeTier3(
    const std::string& outputPath,
    const std::vector<GroupedRecord>& groups,
    const std::vector<MineRef>& mineRefs)
{
    std::ofstream out(outputPath);
    if (!out.is_open()) { std::cerr << "[Tier3] Cannot write: " << outputPath << "\n"; return; }

    out << sharedHead("GeoIQ Full Audit - Auditor View", "Tier 3 Auditor");

    out << R"HTML(  <style>
    .panel { width: 420px; }
    .query-box { width: 100%; background: var(--bg-input); border: 1px solid var(--border); border-radius: 8px; color: var(--text-main); font-family: var(--font-mono); font-size: 0.78rem; padding: 8px 12px; outline: none; transition: border-color 0.2s; resize: vertical; min-height: 52px; }
    .query-box:focus { border-color: var(--border-hi); }
    .query-btn { width: 100%; padding: 8px; border-radius: 8px; margin-top: 6px; background: rgba(56,139,253,0.18); color: var(--accent); border: 1px solid rgba(56,139,253,0.4); font-size: 0.8rem; font-weight: 700; cursor: pointer; font-family: var(--font-main); transition: all 0.18s; }
    .query-btn:hover { background: rgba(56,139,253,0.28); }
    .raw-el-grid { display: flex; flex-wrap: wrap; gap: 4px; margin-top: 4px; }
    .raw-el-chip { font-family: var(--font-mono); font-size: 0.7rem; padding: 2px 8px; border-radius: 4px; border: 1px solid var(--border); background: rgba(255,255,255,0.03); color: var(--text-main); }
    .raw-el-chip.elevated  { border-color: rgba(255,136,0,0.5); color: var(--orange); background: rgba(255,136,0,0.07); }
    .raw-el-chip.critical-el { border-color: rgba(255,51,51,0.5); color: var(--red); background: rgba(255,51,51,0.07); }
    .all-targets-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 4px; margin-top: 6px; }
    .target-cell { background: rgba(255,255,255,0.02); border: 1px solid var(--border); border-radius: 6px; padding: 5px 8px; font-size: 0.72rem; }
    .target-cell-name { color: var(--text-dim); font-size: 0.65rem; text-transform: uppercase; letter-spacing: 0.05em; }
    .hash-display { font-family: var(--font-mono); font-size: 0.65rem; color: var(--text-dim); word-break: break-all; margin-top: 4px; }
    .mine-line-info { background: rgba(255,215,0,0.06); border: 1px solid rgba(255,215,0,0.2); border-radius: 6px; padding: 6px 10px; font-size: 0.78rem; margin-bottom: 4px; }
    .results-count { font-size: 0.72rem; color: var(--text-dim); margin-bottom: 6px; }
  </style>
</head>
<body>
<div id="map"></div>

<div class="map-header">
  <span class="map-header-emoji">&#x1F50D;</span>
  <div>
    <h1>GeoIQ - Full Audit View</h1>
    <p>Complete geological &amp; environmental dataset &middot; Mine proximity overlay</p>
  </div>
</div>

<div class="tier-nav">
  <a class="tier-btn" href="tier1_public.html">&#x1F9BA; Public Safety</a>
  <a class="tier-btn" href="tier2_advanced.html">&#x2697;&#xFE0F; Advanced Research</a>
  <span class="tier-btn active">&#x1F50D; Full Audit</span>
</div>

<div class="panel" id="mainPanel">
  <div class="panel-header">
    <span class="panel-logo">&#x1F50D;</span>
    <div>
      <div class="panel-title">Full Audit Dashboard</div>
      <div class="panel-subtitle">All Targets &middot; All Elements &middot; Mine Proximity</div>
    </div>
  </div>
  <div class="panel-body">

    <div class="card">
      <div class="card-title">Filter Query (WHERE-style)</div>
      <textarea class="query-box" id="queryBox" placeholder='target = "GOLD" AND risk = "CRITICAL"'></textarea>
      <button class="query-btn" onclick="runQuery()">&#x25B6; Run Filter</button>
      <div class="results-count" id="queryCount"></div>
      <div style="font-size:0.68rem;color:var(--text-dim);margin-top:4px">
        Fields: target &middot; risk &middot; score &middot; mine &middot; gsid &middot; hsid
      </div>
    </div>

    <div class="card">
      <div class="card-title">Overlay</div>
      <div class="toggle-group">
        <button class="toggle-pill on" id="togHealth" onclick="toggleLayer('health')">&#x1F9BA; Health</button>
        <button class="toggle-pill on" id="togGeo"    onclick="toggleLayer('geo')">&#x26CF;&#xFE0F; Geology</button>
        <button class="toggle-pill on" id="togMines"  onclick="toggleLayer('mines')">&#x2692;&#xFE0F; Mine Lines</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Dataset Summary</div>
      <div class="stat-grid">
        <div class="stat-box"><div class="stat-big" id="stTotal">-</div><div class="stat-lbl">Total Locations</div></div>
        <div class="stat-box"><div class="stat-big" style="color:var(--red)"    id="stCrit">-</div><div class="stat-lbl">CRITICAL Flags</div></div>
        <div class="stat-box"><div class="stat-big" style="color:var(--orange)" id="stHigh">-</div><div class="stat-lbl">HIGH Flags</div></div>
        <div class="stat-box"><div class="stat-big" style="color:var(--gold)"   id="stMines">-</div><div class="stat-lbl">Mine Matches</div></div>
      </div>
    </div>

    <div id="detailArea" style="display:none">
      <hr class="sep"/>
      <div id="detailContent"></div>
    </div>

    <div class="disclaimer">
      &#x2695;&#xFE0F; GeoHealth is a screening tool, NOT a medical diagnosis.
      All risk assessments use WHO/ATSDR reference values.
    </div>
  </div>
</div>

<script>
)HTML";

    out << "var GROUPS = " << serializeAllGroups(groups, 3) << ";\n";

    // Serialize mine refs
    out << "var MINE_REFS = [\n";
    for (size_t i = 0; i < mineRefs.size(); ++i) {
        const auto& mr = mineRefs[i];
        if (i) out << ",\n";
        out << "{\"sid\":\"" << jsEscape(mr.sample_id) << "\","
            << "\"lat\":" << jsFmt(mr.lat) << ","
            << "\"lon\":" << jsFmt(mr.lon) << ","
            << "\"tgt\":\"" << jsEscape(mr.target) << "\","
            << "\"rl\":\"" << mr.risk_level << "\","
            << "\"sc\":" << jsFmt(mr.score, 3) << ","
            << "\"mine\":\"" << jsEscape(mr.mine_name) << "\","
            << "\"mine_t\":\"" << jsEscape(mr.mine_target) << "\","
            << "\"dist\":" << jsFmt(mr.distance_km, 2) << "}";
    }
    out << "\n];\n";

    // The JS uses R"HTML(...)HTML" — safe because JS content never contains )HTML"
    out << R"HTML(
var RISK_COLOUR = { CRITICAL:'#ff3333', HIGH:'#ff8800', MEDIUM_HIGH:'#ffcc00', MEDIUM:'#ffe066', LOW:'#22c55e' };
function riskCol(r) { return RISK_COLOUR[r] || '#22c55e'; }
var WHO_LIMITS = { As:0.01, Pb:0.01, Hg:0.006, Cd:0.003, Cr:0.05, F:1.5 };
var MAJOR_EL   = ['Fe','Al','SiO2','MgO','K2O','Na2O','CaO','P2O5','S','LOI'];

var map = L.map('map', { preferCanvas: true }).setView([0, 0], 2);
L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
  attribution: '&copy; OpenStreetMap &copy; CARTO', maxZoom: 19, subdomains: 'abcd'
}).addTo(map);

var healthLayer = L.layerGroup().addTo(map);
var geoLayer    = L.layerGroup().addTo(map);
var mineLayer   = L.layerGroup().addTo(map);
var layerState  = { health: true, geo: true, mines: true };
var activeGroups = GROUPS.slice();

function buildMap() {
  healthLayer.clearLayers();
  geoLayer.clearLayers();
  mineLayer.clearLayers();

  activeGroups.forEach(function(grp) {
    var hRisk = grp.whr || 'LOW';
    var hMark = L.circleMarker([grp.lat, grp.lon], {
      radius: hRisk === 'CRITICAL' ? 12 : hRisk === 'HIGH' ? 9 : 7,
      fillColor: riskCol(hRisk), color: '#0a0f14', weight: 1.5, fillOpacity: 0.9
    });
    hMark.on('click', function() { renderDetail(grp); });
    healthLayer.addLayer(hMark);

    var gRisk = grp.wgr || 'LOW';
    if (gRisk !== 'LOW') {
      var gMark = L.circleMarker([grp.lat, grp.lon], {
        radius: 14, fillColor: 'transparent', color: riskCol(gRisk), weight: 2.5, fillOpacity: 0
      });
      gMark.on('click', function() { renderDetail(grp); });
      geoLayer.addLayer(gMark);
    }
  });

  // Mine proximity lines
  var drawn = {};
  MINE_REFS.forEach(function(mr) {
    if (drawn[mr.sid]) return;
    var grp = null;
    for (var i = 0; i < GROUPS.length; i++) { if (GROUPS[i].gsid === mr.sid) { grp = GROUPS[i]; break; } }
    if (!grp) return;
    drawn[mr.sid] = true;
    // Draw a short dashed line bearing north (approximate — actual mine coords not stored)
    var endLat = grp.lat + (mr.dist / 111.0);
    var endLon = grp.lon;
    var line = L.polyline([[grp.lat, grp.lon], [endLat, endLon]], {
      color: '#ffd700', weight: 1.5, dashArray: '5 8', opacity: 0.6
    });
    line.bindTooltip(mr.mine + ' (' + mr.dist.toFixed(1) + ' km)', { sticky: true });
    mineLayer.addLayer(line);
    var mineIcon = L.divIcon({ className:'', html:'<div style="font-size:11px">&#x26CF;&#xFE0F;</div>', iconSize:[16,16], iconAnchor:[8,8] });
    mineLayer.addLayer(L.marker([endLat, endLon], { icon: mineIcon }).bindTooltip(mr.mine + ' (' + mr.mine_t + ', ' + mr.dist.toFixed(1) + ' km)'));
  });

  var crit  = activeGroups.filter(function(g) { return g.wgr === 'CRITICAL' || g.whr === 'CRITICAL'; }).length;
  var high  = activeGroups.filter(function(g) { return g.wgr === 'HIGH'     || g.whr === 'HIGH';     }).length;
  var mines = activeGroups.filter(function(g) { return !!g.mine; }).length;
  document.getElementById('stTotal').textContent = activeGroups.length;
  document.getElementById('stCrit').textContent  = crit;
  document.getElementById('stHigh').textContent  = high;
  document.getElementById('stMines').textContent = mines;
}

function toggleLayer(name) {
  layerState[name] = !layerState[name];
  var id  = 'tog' + name.charAt(0).toUpperCase() + name.slice(1);
  var el  = document.getElementById(id);
  el.className = 'toggle-pill' + (layerState[name] ? ' on' : '');
  var lyr = name === 'health' ? healthLayer : name === 'geo' ? geoLayer : mineLayer;
  if (layerState[name]) map.addLayer(lyr); else map.removeLayer(lyr);
}

// Simple WHERE-style filter — no JS regex literals used (avoids raw string risk)
function parseConditions(raw) {
  var conditions = [];
  var parts = raw.toUpperCase().split(' AND ');
  parts.forEach(function(part) {
    part = part.trim();
    // Check for equality: FIELD = "VALUE"
    var eqPos = part.indexOf('=');
    if (eqPos > 0) {
      var field = part.substring(0, eqPos).trim().toLowerCase();
      var val   = part.substring(eqPos + 1).trim().replace(/^["']|["']$/g, '').toLowerCase();
      var isNum = !isNaN(parseFloat(val));
      conditions.push({ field: field, op: '=', val: val, isNum: isNum });
    }
    // Check for comparison: FIELD > NUMBER
    var ops = ['>=','<=','>','<'];
    for (var i = 0; i < ops.length; i++) {
      var op  = ops[i];
      var pos = part.indexOf(op);
      if (pos > 0) {
        var f = part.substring(0, pos).trim().toLowerCase();
        var v = part.substring(pos + op.length).trim();
        conditions.push({ field: f, op: op, val: v, isNum: true });
        break;
      }
    }
  });
  return conditions;
}

function matchesCondition(grp, cond) {
  var all = (grp.geo || []).concat(grp.hlt || []);
  var f   = cond.field;
  var v   = cond.val;
  if (f === 'target') return all.some(function(t) { return t.tgt.toLowerCase() === v; });
  if (f === 'risk')   return grp.wgr.toLowerCase() === v || grp.whr.toLowerCase() === v;
  if (f === 'mine')   return grp.mine && grp.mine.toLowerCase().indexOf(v) >= 0;
  if (f === 'gsid')   return (grp.gsid || '').toLowerCase() === v;
  if (f === 'hsid')   return (grp.hsid || '').toLowerCase() === v;
  if (f === 'score') {
    var sc = 0;
    all.forEach(function(t) { if (t.sc > sc) sc = t.sc; });
    var nv = parseFloat(v);
    if (cond.op === '>')  return sc >  nv;
    if (cond.op === '<')  return sc <  nv;
    if (cond.op === '>=') return sc >= nv;
    if (cond.op === '<=') return sc <= nv;
    if (cond.op === '=')  return sc === nv;
  }
  return true;
}

function runQuery() {
  var raw = document.getElementById('queryBox').value.trim();
  if (!raw) { activeGroups = GROUPS.slice(); buildMap(); return; }
  var conds = parseConditions(raw);
  activeGroups = GROUPS.filter(function(grp) {
    return conds.every(function(c) { return matchesCondition(grp, c); });
  });
  document.getElementById('queryCount').textContent = activeGroups.length + ' of ' + GROUPS.length + ' locations match';
  buildMap();
}

function stateClass(st) {
  if (!st) return 'bg-state';
  if (st === 'ENRICHED') return 'enriched';
  if (st === 'DEPLETED') return 'depleted';
  return 'bg-state';
}

function elUnit(el) {
  if (el === 'Au') return 'ppb';
  if (MAJOR_EL.indexOf(el) >= 0) return '%';
  if (el === 'pH') return 'pH'; if (el === 'Eh') return 'mV';
  return 'ppm';
}

function renderDetail(grp) {
  var html = '';
  html += '<div class="detail-section"><div class="detail-label">Sample IDs</div>';
  html += '<div class="detail-value mono">Geology: ' + (grp.gsid || '-') + '<br/>Health: ' + (grp.hsid || '-') + '</div>';
  html += '<div style="font-size:0.78rem;color:var(--text-dim)">Coords: ' + grp.lat.toFixed(5) + ', ' + grp.lon.toFixed(5);
  if (grp.has_h && grp.hd) html += ' &middot; Pair: ' + grp.hd.toFixed(2) + ' km';
  html += '</div></div>';

  // All raw elements
  if (grp.raw && Object.keys(grp.raw).length > 0) {
    html += '<div class="model-block"><div class="model-block-title">All Raw Element Values</div><div class="raw-el-grid">';
    Object.keys(grp.raw).forEach(function(el) {
      var val    = grp.raw[el];
      var whoLim = WHO_LIMITS[el];
      var cls    = '';
      if (whoLim && val > whoLim * 10) cls = 'critical-el';
      else if (whoLim && val > whoLim) cls = 'elevated';
      var u = elUnit(el);
      html += '<div class="raw-el-chip ' + cls + '" title="' + el + ': ' + val + ' ' + u + '">' + el + ': ' + (typeof val === 'number' ? val.toFixed(3) : val) + '</div>';
    });
    html += '</div></div>';
  }

  // All geology targets
  if (grp.geo && grp.geo.length > 0) {
    html += '<div class="model-block"><div class="model-block-title">All Geology Targets Scored</div><div class="all-targets-grid">';
    grp.geo.forEach(function(gt) {
      html += '<div class="target-cell"><div class="target-cell-name">' + gt.tgt.replace(/_/g,' ') + '</div>';
      html += '<span class="badge badge-' + gt.rl + '" style="font-size:0.65rem">' + gt.rl.replace('_',' ') + '</span>';
      html += '<div style="font-size:0.7rem;color:var(--text-dim);margin-top:2px">Score: ' + gt.sc.toFixed(3) + '</div>';
      if (gt.rules && gt.rules.length) html += '<div style="font-size:0.65rem;color:var(--text-dim)">' + gt.rules.join(', ') + '</div>';
      html += '</div>';
    });
    html += '</div>';

    // Full evidence for elevated targets
    grp.geo.filter(function(gt) { return gt.rl !== 'LOW' && gt.ev && gt.ev.length; }).forEach(function(gt) {
      html += '<hr class="sep"/><div style="font-size:0.78rem;font-weight:700;margin-bottom:4px">' + gt.tgt.replace(/_/g,' ') + ' Evidence</div>';
      html += '<table class="ev-table"><thead><tr><th>Element</th><th>Raw Value</th><th>Local Z</th><th>Alpha</th><th>State</th></tr></thead><tbody>';
      gt.ev.forEach(function(ev) {
        var cl = stateClass(ev.st);
        html += '<tr><td><strong>' + ev.el + '</strong></td>';
        html += '<td>' + (ev.v !== null ? ev.v.toFixed(4) + ' ' + ev.u : '-') + '</td>';
        html += '<td class="' + cl + '">' + (ev.z !== null ? ev.z.toFixed(4) : '-') + '</td>';
        html += '<td style="font-family:var(--font-mono);font-size:0.68rem">' + (ev.a !== null ? ev.a.toFixed(4) : '-') + '</td>';
        html += '<td class="' + cl + '">' + (ev.st || 'INVALID') + '</td></tr>';
        if (ev.fm) html += '<tr><td colspan="5" class="ev-formula">' + ev.fm + '</td></tr>';
      });
      html += '</tbody></table>';
      if (gt.ann) html += '<div style="font-size:0.75rem;margin-top:5px;color:var(--text-dim)">' + gt.ann + '</div>';
    });
    html += '</div>';
  }

  // Health targets
  if (grp.hlt && grp.hlt.length > 0) {
    html += '<div class="model-block"><div class="model-block-title">Health Targets</div>';
    grp.hlt.forEach(function(ht) {
      html += '<div class="target-row"><strong style="font-size:0.8rem">' + ht.tgt.replace(/_/g,' ') + '</strong>';
      html += '<span class="badge badge-' + ht.rl + '">' + ht.rl.replace('_',' ') + '</span></div>';
      if (ht.ann) html += '<div style="font-size:0.75rem;margin:3px 0">' + ht.ann + '</div>';
      if (ht.ev && ht.ev.length) {
        html += '<table class="ev-table"><thead><tr><th>Element</th><th>Raw</th><th>Local Z</th><th>Formula</th></tr></thead><tbody>';
        ht.ev.forEach(function(ev) {
          html += '<tr><td><strong>' + ev.el + '</strong></td>';
          html += '<td>' + (ev.v !== null ? ev.v.toFixed(4) + ' ' + ev.u : '-') + '</td>';
          html += '<td>' + (ev.z !== null ? ev.z.toFixed(4) : '-') + '</td>';
          html += '<td class="ev-formula">' + (ev.fm || '-') + '</td></tr>';
        });
        html += '</tbody></table>';
      }
    });
    html += '</div>';
  }

  // Mine proximity
  if (grp.mine) {
    html += '<div class="mine-line-info">&#x2692;&#xFE0F; <strong>' + grp.mine + '</strong><br/>';
    html += '<span style="color:var(--text-dim)">' + grp.mine_t + ' &middot; ' + (grp.mine_d ? grp.mine_d.toFixed(2) + ' km' : '-') + '</span></div>';
  }

  // Audit hash (djb2)
  var rawStr = JSON.stringify(grp);
  var hash = 5381;
  for (var i = 0; i < rawStr.length; i++) { hash = ((hash << 5) + hash) + rawStr.charCodeAt(i); hash |= 0; }
  var hashHex = (hash >>> 0).toString(16).padStart(8,'0').toUpperCase();
  html += '<div style="margin-top:8px"><div class="card-title">Audit Hash</div>';
  html += '<div class="hash-display">DJB2-' + hashHex + ' (' + grp.id + ')</div></div>';

  document.getElementById('detailContent').innerHTML = html;
  document.getElementById('detailArea').style.display = 'block';
}

buildMap();
var allBounds = GROUPS.map(function(g) { return [g.lat, g.lon]; });
if (allBounds.length) map.fitBounds(allBounds, { padding: [40, 40] });
document.getElementById('queryCount').textContent = GROUPS.length + ' locations loaded';
</script>
</body>
</html>
)HTML";

    out.close();
    std::cout << "[Tier3] Written: " << outputPath << "\n";
}
