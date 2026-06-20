#pragma once
// =============================================================================
// JsonParser.h  —  GeoIQ HTML Frontend Generator
// Minimal, dependency-free JSON reader for the specific schemas
// produced by GeoIQ.exe (predictions.geojson, cross_referenced_anomalies.json,
// toxicology_database.json).
//
// Does NOT implement a full JSON spec parser — only what is needed:
//   - Object with string/number/bool/null/array/nested-object values
//   - Arrays of objects
//   - Handles escaped characters in strings
// =============================================================================
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include <memory>

// ─── JSON Value Types ─────────────────────────────────────────────────────────
enum class JType { Null, Bool, Number, String, Array, Object };

struct JValue;
using JObject = std::unordered_map<std::string, JValue>;
using JArray  = std::vector<JValue>;

struct JValue {
    JType type = JType::Null;
    bool        b = false;
    double      n = 0.0;
    std::string s;
    std::shared_ptr<JArray>  arr;
    std::shared_ptr<JObject> obj;

    bool isNull()   const { return type == JType::Null;   }
    bool isBool()   const { return type == JType::Bool;   }
    bool isNumber() const { return type == JType::Number; }
    bool isString() const { return type == JType::String; }
    bool isArray()  const { return type == JType::Array;  }
    bool isObject() const { return type == JType::Object; }

    double      asDouble(double def = 0.0)       const { return isNumber() ? n  : def; }
    bool        asBool  (bool   def = false)     const { return isBool()   ? b  : def; }
    std::string asString(const std::string& def = "") const { return isString() ? s : def; }
    const JArray&  asArray()  const { static JArray  e; return arr ? *arr : e; }
    const JObject& asObject() const { static JObject e; return obj ? *obj : e; }

    // Convenience: get key from object, returns null JValue if missing
    const JValue& get(const std::string& key) const {
        static JValue null_val;
        if (!isObject() || !obj) return null_val;
        auto it = obj->find(key);
        return (it != obj->end()) ? it->second : null_val;
    }
};

// ─── Parser State ─────────────────────────────────────────────────────────────
struct JsonParser {
    const char* p;
    const char* end;

    explicit JsonParser(const std::string& src)
        : p(src.data()), end(src.data() + src.size()) {}

    void skipWS() {
        while (p < end && std::isspace((unsigned char)*p)) ++p;
    }

    bool expect(char c) {
        skipWS();
        if (p < end && *p == c) { ++p; return true; }
        return false;
    }

    std::string parseString() {
        // Assumes current char is '"'
        ++p; // skip opening "
        std::string result;
        result.reserve(64);
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) break;
                switch (*p) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'u':  // skip 4 hex chars
                        result += '?';
                        for (int i = 0; i < 4 && p+1 < end; ++i) ++p;
                        break;
                    default:   result += *p;   break;
                }
            } else {
                result += *p;
            }
            ++p;
        }
        if (p < end) ++p; // skip closing "
        return result;
    }

    JValue parseValue() {
        skipWS();
        if (p >= end) return JValue{};

        JValue v;

        if (*p == '"') {
            v.type = JType::String;
            v.s = parseString();
        } else if (*p == '{') {
            v.type = JType::Object;
            v.obj = std::make_shared<JObject>(parseObject());
        } else if (*p == '[') {
            v.type = JType::Array;
            v.arr = std::make_shared<JArray>(parseArray());
        } else if (*p == 't') {
            // true
            p += 4;
            v.type = JType::Bool; v.b = true;
        } else if (*p == 'f') {
            // false
            p += 5;
            v.type = JType::Bool; v.b = false;
        } else if (*p == 'n') {
            // null
            p += 4;
            v.type = JType::Null;
        } else {
            // number
            char* ep = nullptr;
            v.n = std::strtod(p, &ep);
            p = ep;
            v.type = JType::Number;
        }
        return v;
    }

    JObject parseObject() {
        JObject obj;
        ++p; // skip '{'
        skipWS();
        if (p < end && *p == '}') { ++p; return obj; }
        while (p < end) {
            skipWS();
            if (*p != '"') break;
            std::string key = parseString();
            skipWS();
            if (p < end && *p == ':') ++p;
            obj[key] = parseValue();
            skipWS();
            if (p < end && *p == ',') { ++p; continue; }
            if (p < end && *p == '}') { ++p; break; }
        }
        return obj;
    }

    JArray parseArray() {
        JArray arr;
        ++p; // skip '['
        skipWS();
        if (p < end && *p == ']') { ++p; return arr; }
        while (p < end) {
            skipWS();
            if (*p == ']') { ++p; break; }
            arr.push_back(parseValue());
            skipWS();
            if (p < end && *p == ',') { ++p; continue; }
            if (p < end && *p == ']') { ++p; break; }
        }
        return arr;
    }

    JValue parse() {
        skipWS();
        return parseValue();
    }
};

// ─── Convenience free function ─────────────────────────────────────────────
inline JValue parseJson(const std::string& src) {
    JsonParser parser(src);
    return parser.parse();
}
