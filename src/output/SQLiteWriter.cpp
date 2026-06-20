#include "output/SQLiteWriter.h"
#include "output/KnownMines.h"
#include "utils/sqlite3.h"
#include "utils/Logger.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include <iostream>
#include <sstream>

namespace GeoIQ {

    bool SQLiteWriter::write(const std::string& db_path, const std::vector<PredictionRecord>& records, bool is_geology) {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            Logger::getInstance().error("SQLiteWriter", "Failed to open database: " + db_path + " - " + sqlite3_errmsg(db));
            if (db) sqlite3_close(db);
            return false;
        }

        Logger::getInstance().info("SQLiteWriter", "Opened database successfully: " + db_path);

        // Performance optimizations
        sqlite3_exec(db, "PRAGMA synchronous = OFF;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

        // 1. DDL Tables creation
        const char* ddl_samples = 
            "CREATE TABLE IF NOT EXISTS samples ("
            "    sample_id TEXT PRIMARY KEY,"
            "    lat REAL,"
            "    lon REAL,"
            "    sample_type TEXT,"
            "    horizon TEXT,"
            "    elevation_m REAL,"
            "    depth_m REAL,"
            "    data_quality_note TEXT"
            ");";

        const char* ddl_predictions = 
            "CREATE TABLE IF NOT EXISTS predictions ("
            "    prediction_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    sample_id TEXT,"
            "    target TEXT,"
            "    risk_level TEXT,"
            "    score REAL,"
            "    confidence REAL,"
            "    spatial_consistency REAL,"
            "    dispersion_detected INTEGER,"
            "    annotation TEXT,"
            "    final_score_formula TEXT,"
            "    risk_level_derivation TEXT,"
            "    FOREIGN KEY(sample_id) REFERENCES samples(sample_id)"
            ");";

        const char* ddl_evidence = 
            "CREATE TABLE IF NOT EXISTS evidence ("
            "    evidence_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    prediction_id INTEGER,"
            "    element TEXT,"
            "    raw_value REAL,"
            "    local_z REAL,"
            "    regional_z REAL,"
            "    alpha REAL,"
            "    singularity_state TEXT,"
            "    formula TEXT,"
            "    FOREIGN KEY(prediction_id) REFERENCES predictions(prediction_id)"
            ");";

        const char* ddl_known_mines = 
            "CREATE TABLE IF NOT EXISTS known_mines ("
            "    mine_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    name TEXT UNIQUE,"
            "    country_state TEXT,"
            "    lat REAL,"
            "    lon REAL,"
            "    classification TEXT,"
            "    target_type TEXT"
            ");";

        const char* ddl_mine_proximity = 
            "CREATE TABLE IF NOT EXISTS mine_proximity ("
            "    prediction_id INTEGER,"
            "    mine_id INTEGER,"
            "    distance_km REAL,"
            "    PRIMARY KEY (prediction_id, mine_id),"
            "    FOREIGN KEY(prediction_id) REFERENCES predictions(prediction_id),"
            "    FOREIGN KEY(mine_id) REFERENCES known_mines(mine_id)"
            ");";

        char* err_msg = nullptr;
        if (sqlite3_exec(db, ddl_samples, nullptr, nullptr, &err_msg) != SQLITE_OK ||
            sqlite3_exec(db, ddl_predictions, nullptr, nullptr, &err_msg) != SQLITE_OK ||
            sqlite3_exec(db, ddl_evidence, nullptr, nullptr, &err_msg) != SQLITE_OK ||
            sqlite3_exec(db, ddl_known_mines, nullptr, nullptr, &err_msg) != SQLITE_OK ||
            sqlite3_exec(db, ddl_mine_proximity, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            
            std::string err_str = err_msg ? err_msg : "Unknown DDL error";
            Logger::getInstance().error("SQLiteWriter", "Failed to create tables: " + err_str);
            if (err_msg) sqlite3_free(err_msg);
            sqlite3_close(db);
            return false;
        }
        if (err_msg) sqlite3_free(err_msg);

        // 2. Seed Known Mines
        sqlite3_stmt* stmt_mine = nullptr;
        rc = sqlite3_prepare_v2(db, 
            "INSERT OR IGNORE INTO known_mines (name, country_state, lat, lon, classification, target_type) "
            "VALUES (?, ?, ?, ?, ?, ?);", -1, &stmt_mine, nullptr);
        
        if (rc == SQLITE_OK) {
            sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
            for (const auto& mine : SEED_MINES) {
                sqlite3_bind_text(stmt_mine, 1, mine.name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt_mine, 2, mine.state_country.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt_mine, 3, mine.lat);
                sqlite3_bind_double(stmt_mine, 4, mine.lon);
                sqlite3_bind_text(stmt_mine, 5, mine.classification.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt_mine, 6, mine.target_type.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt_mine);
                sqlite3_reset(stmt_mine);
            }
            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
            sqlite3_finalize(stmt_mine);
            Logger::getInstance().info("SQLiteWriter", "Seeded known mines database.");
        } else {
            Logger::getInstance().warn("SQLiteWriter", "Failed to prepare mine seed statement: " + std::string(sqlite3_errmsg(db)));
        }

        // 3. Write Prediction Records
        sqlite3_stmt* stmt_sample = nullptr;
        sqlite3_stmt* stmt_pred = nullptr;
        sqlite3_stmt* stmt_ev = nullptr;
        sqlite3_stmt* stmt_prox = nullptr;

        sqlite3_prepare_v2(db, 
            "INSERT OR IGNORE INTO samples (sample_id, lat, lon, sample_type, horizon, elevation_m, depth_m, data_quality_note) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt_sample, nullptr);

        sqlite3_prepare_v2(db, 
            "INSERT INTO predictions (sample_id, target, risk_level, score, confidence, spatial_consistency, dispersion_detected, annotation, final_score_formula, risk_level_derivation) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt_pred, nullptr);

        sqlite3_prepare_v2(db, 
            "INSERT INTO evidence (prediction_id, element, raw_value, local_z, regional_z, alpha, singularity_state, formula) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt_ev, nullptr);

        sqlite3_prepare_v2(db, 
            "INSERT OR IGNORE INTO mine_proximity (prediction_id, mine_id, distance_km) "
            "VALUES (?, ?, ?);", -1, &stmt_prox, nullptr);

        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        int sample_count = 0;
        int prediction_count = 0;

        for (const auto& rec : records) {
            // Bind Sample
            sqlite3_bind_text(stmt_sample, 1, rec.sample_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt_sample, 2, rec.lat);
            sqlite3_bind_double(stmt_sample, 3, rec.lon);
            sqlite3_bind_text(stmt_sample, 4, sampleTypeToString(rec.sample_type).c_str(), -1, SQLITE_TRANSIENT);
            // Horizon helper
            std::string hor_str = "UNKNOWN";
            if (!rec.results.empty() && !rec.results[0].evidence.empty()) {
                // Horizon is in sample point metadata, let's keep it simple
            }
            sqlite3_bind_text(stmt_sample, 5, hor_str.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt_sample, 6, rec.elevation_m);
            sqlite3_bind_double(stmt_sample, 7, rec.depth_m);
            // Cumulative note
            std::string cum_note = "";
            for (const auto& r : rec.results) {
                if (!r.data_quality_note.empty()) {
                    cum_note += r.data_quality_note + " ";
                }
            }
            sqlite3_bind_text(stmt_sample, 8, cum_note.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt_sample);
            sqlite3_reset(stmt_sample);
            sample_count++;

            for (const auto& res : rec.results) {
                // Bind Prediction
                sqlite3_bind_text(stmt_pred, 1, rec.sample_id.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt_pred, 2, Stats::targetToString(res.target).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt_pred, 3, Stats::riskLevelToString(res.risk_level).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt_pred, 4, res.score);
                sqlite3_bind_double(stmt_pred, 5, res.confidence);
                sqlite3_bind_double(stmt_pred, 6, res.spatial_consistency);
                sqlite3_bind_int(stmt_pred, 7, rec.dispersion_detected ? 1 : 0);
                sqlite3_bind_text(stmt_pred, 8, res.annotation.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt_pred, 9, res.final_score_formula.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt_pred, 10, res.risk_level_derivation.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt_pred);

                sqlite_int64 pred_id = sqlite3_last_insert_rowid(db);
                sqlite3_reset(stmt_pred);
                prediction_count++;

                // Bind Evidence
                for (const auto& ev : res.evidence) {
                    sqlite3_bind_int64(stmt_ev, 1, pred_id);
                    sqlite3_bind_text(stmt_ev, 2, elementFieldToString(ev.element).c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_double(stmt_ev, 3, ev.raw_value);
                    sqlite3_bind_double(stmt_ev, 4, ev.local_z);
                    sqlite3_bind_double(stmt_ev, 5, ev.regional_z);
                    sqlite3_bind_double(stmt_ev, 6, ev.alpha);
                    sqlite3_bind_text(stmt_ev, 7, Stats::singularityStateToString(ev.singularity_state).c_str(), -1, SQLITE_TRANSIENT);
                    // Generate JIT formula string
                    std::string form = Math::formulaString(elementFieldToString(ev.element), ev.raw_value, ev.log_value, ev.local_median_log, ev.local_stddev_log, ev.local_z);
                    sqlite3_bind_text(stmt_ev, 8, form.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(stmt_ev);
                    sqlite3_reset(stmt_ev);
                }

                // Cross-reference with mines database if anomaly is detected (is_geology)
                if (is_geology && (res.risk_level == RiskLevel::MEDIUM_HIGH || 
                                   res.risk_level == RiskLevel::HIGH || 
                                   res.risk_level == RiskLevel::CRITICAL)) {
                    
                    // We run Haversine distance checks against the 60+ SEED_MINES in memory.
                    // This matches the target type where applicable (or general check)
                    std::string target_type_str = Stats::targetToString(res.target);
                    for (size_t mine_idx = 0; mine_idx < SEED_MINES.size(); ++mine_idx) {
                        const auto& mine = SEED_MINES[mine_idx];
                        
                        // Calculate distance
                        double dist = Math::haversine(rec.lat, rec.lon, mine.lat, mine.lon);
                        if (dist <= 100.0) { // Within 100km
                            // Insert row into mine_proximity (mine_id is 1-indexed based on SEED_MINES order)
                            sqlite3_bind_int64(stmt_prox, 1, pred_id);
                            sqlite3_bind_int64(stmt_prox, 2, static_cast<sqlite_int64>(mine_idx + 1));
                            sqlite3_bind_double(stmt_prox, 3, dist);
                            sqlite3_step(stmt_prox);
                            sqlite3_reset(stmt_prox);
                        }
                    }
                }
            }
        }

        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

        sqlite3_finalize(stmt_sample);
        sqlite3_finalize(stmt_pred);
        sqlite3_finalize(stmt_ev);
        sqlite3_finalize(stmt_prox);
        sqlite3_close(db);

        Logger::getInstance().info("SQLiteWriter", "Saved " + std::to_string(sample_count) + " samples, " + std::to_string(prediction_count) + " predictions.");
        return true;
    }

}
