/**
 * @file ArgParser.h
 * @brief GNU getopt tarzı komut satırı argüman ayrıştırıcı.
 *
 * Short (-r), long (--reader), short+long (-r 0, --reader=0),
 * birleşik flagler (-jv), -- end-of-options desteklenir.
 */
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <sstream>

// ════════════════════════════════════════════════════════════════════════════════
// ArgParser — GNU getopt-style argument parser
//
// Desteklenen formlar:
//   -j                   short flag
//   -r 0                 short option + value (space)
//   -r0                  short option + value (adjacent)
//   --json               long flag
//   --reader 0           long option + value (space)
//   --reader=0           long option + value (equals)
//
// Kullanım:
//   ArgDef defs[] = {
//       {"reader", 'r', true },   // --reader/-r, değer alır
//       {"json",   'j', false},   // --json/-j,   flag
//   };
//   auto parsed = ArgParser::parse(argc, argv, defs);
//   auto r = parsed.get("reader");  // optional<string>
//   bool j = parsed.has("json");
// ════════════════════════════════════════════════════════════════════════════════

struct ArgDef {
    std::string longName;   // "reader"
    char        shortName;  // 'r'  (0 = yok)
    bool        takesValue; // true → değer alır
};

struct ParsedArgs {
    std::string              subcommand;   // ilk positional
    std::map<std::string, std::string> opts;   // long name → value
    std::set<std::string>    flags;        // long name (değersiz)
    std::vector<std::string> positional;  // subcommand hariç kalan

    std::optional<std::string> get(const std::string& longName) const {
        auto it = opts.find(longName);
        if (it != opts.end()) return it->second;
        return std::nullopt;
    }

    std::string getOr(const std::string& longName,
                      const std::string& defaultVal) const {
        return get(longName).value_or(defaultVal);
    }

    bool has(const std::string& longName) const {
        return flags.count(longName) > 0 || opts.count(longName) > 0;
    }
};

class ArgParser {
public:
    static ParsedArgs parse(int argc, char* argv[],
                            const std::vector<ArgDef>& defs) {
        ParsedArgs result;
        bool subCmdSet = false;
        int i = 1; // argv[0] = binary adı

        while (i < argc) {
            std::string tok = argv[i];

            if (tok == "--") { // end of options
                ++i;
                while (i < argc) {
                    if (!subCmdSet) { result.subcommand = argv[i]; subCmdSet = true; }
                    else result.positional.push_back(argv[i]);
                    ++i;
                }
                break;
            }

            if (tok.size() > 2 && tok[0] == '-' && tok[1] == '-') {
                // long option: --name veya --name=value
                std::string name;
                std::optional<std::string> value;

                auto eq = tok.find('=', 2);
                if (eq != std::string::npos) {
                    name  = tok.substr(2, eq - 2);
                    value = tok.substr(eq + 1);
                } else {
                    name = tok.substr(2);
                }

                const ArgDef* def = findLong(defs, name);
                if (!def) throw std::invalid_argument("Unknown option: --" + name);

                if (def->takesValue) {
                    if (!value) {
                        if (i + 1 >= argc)
                            throw std::invalid_argument("--" + name + " requires a value");
                        value = argv[++i];
                    }
                    result.opts[name] = *value;
                } else {
                    result.flags.insert(name);
                }

            } else if (tok.size() >= 2 && tok[0] == '-' && tok[1] != '-') {
                // short option(s): -j, -r0, -r 0, -jrv (combined flags)
                size_t pos = 1;
                while (pos < tok.size()) {
                    char sc = tok[pos];
                    const ArgDef* def = findShort(defs, sc);
                    if (!def) throw std::invalid_argument(
                        std::string("Unknown option: -") + sc);

                    if (def->takesValue) {
                        std::string val;
                        if (pos + 1 < tok.size()) {
                            val = tok.substr(pos + 1); // -r0
                        } else {
                            if (i + 1 >= argc)
                                throw std::invalid_argument(
                                    std::string("-") + sc + " requires a value");
                            val = argv[++i]; // -r 0
                        }
                        result.opts[def->longName] = val;
                        break; // value tüketti, sonraki char'a bakmayalım
                    } else {
                        result.flags.insert(def->longName);
                    }
                    ++pos;
                }

            } else {
                // positional
                if (!subCmdSet) { result.subcommand = tok; subCmdSet = true; }
                else result.positional.push_back(tok);
            }

            ++i;
        }

        return result;
    }

    // String → ParsedArgs (interactive / script satırı için)
    static ParsedArgs parseLine(const std::string& line,
                                const std::vector<ArgDef>& defs) {
        auto tokens = tokenize(line);
        if (tokens.empty()) return {};

        // fake argc/argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>("scardtool")); // argv[0]
        for (auto& t : tokens) argv.push_back(const_cast<char*>(t.c_str()));
        return parse(static_cast<int>(argv.size()),
                     argv.data(), defs);
    }

    // Basit tokenizer — boşluk + quoted string desteği
    static std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::string cur;
        bool inQuote = false;
        char quoteChar = 0;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (inQuote) {
                if (c == quoteChar) { inQuote = false; }
                else cur += c;
            } else if (c == '"' || c == '\'') {
                inQuote = true; quoteChar = c;
            } else if (c == ' ' || c == '\t') {
                if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) tokens.push_back(cur);
        return tokens;
    }

private:
    static const ArgDef* findLong(const std::vector<ArgDef>& defs,
                                   const std::string& name) {
        for (auto& d : defs) if (d.longName == name) return &d;
        return nullptr;
    }
    static const ArgDef* findShort(const std::vector<ArgDef>& defs, char c) {
        for (auto& d : defs) if (d.shortName == c) return &d;
        return nullptr;
    }
};
