/** @file ScriptEngine.h
 * @brief Bash benzeri .scard script yorumlayıcı.
 *
 * Desteklenen özellikler:
 *
 * @par Değişkenler:
 * @code
 *   $READER = 0
 *   $KEY    = FFFFFFFFFFFF
 *   load-key -r $READER -k $KEY
 *   echo "Reader is: $READER"
 * @endcode
 *
 * @par Koşullar (son komutun exit code'una göre):
 * @code
 *   connect -r 0
 *   if ok
 *       uid -r 0
 *   elif fail
 *       echo "Connection failed"
 *   else
 *       echo "Other"
 *   fi
 * @endcode
 *
 * @par Döngüler:
 * @code
 *   $SECTOR = 0
 *   while $SECTOR < 16
 *       read-sector -r 0 -s $SECTOR -k $KEY
 *       $SECTOR = $SECTOR + 1
 *   done
 *
 *   for $BLOCK in 0 1 2 3
 *       read -r 0 -p $BLOCK
 *   done
 * @endcode
 *
 * @par Çıktı:
 * @code
 *   echo "Message"
 *   echo "Value: $READER"
 * @endcode
 *
 * @par Hata yönetimi:
 * @code
 *   set stop-on-error true    # hata durumunda dur (varsayılan)
 *   set stop-on-error false   # hatalarda devam et
 * @endcode
 */
#pragma once
#include "Commands/CommandRegistry.h"
#include "App/App.h"
#include "ExitCode.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

// ── Token türleri ─────────────────────────────────────────────────────────────

enum class ScriptBlockType { If, While, For };

struct ScriptBlock {
    ScriptBlockType type;
    std::vector<std::string> lines; ///< Blok içindeki satırlar
    std::string condition;           ///< if/while koşulu
    std::string elseLines;           ///< else bloğu (if için)
};

// ── ScriptEngine ──────────────────────────────────────────────────────────────

/** @brief Bash benzeri .scard script yorumlayıcı. */
class ScriptEngine {
public:
    /** @brief Yorumlayıcı oluştur.
     * @param dispatcher Komut satırı çalıştırıcı (ScriptRunner::runSingle benzeri)
     */
    explicit ScriptEngine(
        std::function<ExitCode(const std::string&)> dispatcher)
        : dispatcher_(std::move(dispatcher))
    {
        // Yerleşik çevre değişkenlerini yükle
        loadEnvVars();
    }

    /** @brief Script metnini satır satır yorumla.
     * @param source Kaynak satırları
     * @return Son exit code
     */
    ExitCode execute(const std::vector<std::string>& lines) {
        lastExitCode_ = ExitCode::Success;
        size_t i = 0;
        while (i < lines.size()) {
            auto result = executeFrom(lines, i);
            i = result.first;
            lastExitCode_ = result.second;
            if (stopOnError_ && lastExitCode_ != ExitCode::Success) break;
        }
        return lastExitCode_;
    }

    /** @brief Tek satır yorumla (interactive mod için). */
    ExitCode executeLine(const std::string& line) {
        return execute({line});
    }

    /** @brief Değişken ata. */
    void setVar(const std::string& name, const std::string& value) {
        vars_[name] = value;
    }

    /** @brief Değişken oku. */
    std::optional<std::string> getVar(const std::string& name) const {
        auto it = vars_.find(name);
        if (it != vars_.end()) return it->second;
        return std::nullopt;
    }

    /** @brief Tüm değişkenleri döndür. */
    const std::map<std::string, std::string>& vars() const { return vars_; }

    /** @brief Son exit code. */
    ExitCode lastExitCode() const { return lastExitCode_; }

private:
    std::function<ExitCode(const std::string&)> dispatcher_;
    std::map<std::string, std::string> vars_;
    ExitCode lastExitCode_ = ExitCode::Success;
    bool stopOnError_ = true;

    // Döngü kontrol sinyalleri — iç loop'tan dış loop'a iletilir
    // ExitCode ile karışmaz, sadece engine içinde kullanılır
    enum class FlowSignal { None, Break, Continue };
    mutable FlowSignal flowSignal_ = FlowSignal::None;

    // Çevre değişkenlerini $SCARDTOOL_* → $READER gibi kısa isimlere yükle
    void loadEnvVars() {
        auto load = [this](const char* env, const std::string& varName) {
            if (const char* v = std::getenv(env))
                vars_[varName] = v;
        };
        load("SCARDTOOL_READER",   "READER");
        load("SCARDTOOL_KEY",      "KEY");
        load("SCARDTOOL_CIPHER",   "CIPHER");
    }

    // (nextLineIndex, exitCode) çifti döner
    std::pair<size_t, ExitCode>
    executeFrom(const std::vector<std::string>& lines, size_t i) {
        if (i >= lines.size()) return {lines.size(), ExitCode::Success};

        std::string raw = trim(lines[i]);

        // Boş satır ve yorum
        if (raw.empty() || raw[0] == '#') return {i+1, ExitCode::Success};

        // Shebang
        if (i == 0 && raw.substr(0,2) == "#!") return {i+1, ExitCode::Success};

        // Değişken atama: $NAME = value  veya  NAME = value
        if (isAssignment(raw)) {
            auto [name, value] = parseAssignment(raw);
            vars_[name] = expand(value);
            return {i+1, ExitCode::Success};
        }

        // echo
        if (startsWith(raw, "echo ") || raw == "echo") {
            std::string msg = raw.size() > 5 ? expand(raw.substr(5)) : "";
            // tırnak işaretlerini temizle
            if (msg.size() >= 2 && msg.front() == '"' && msg.back() == '"')
                msg = msg.substr(1, msg.size()-2);
            std::cout << msg << "\n";
            return {i+1, ExitCode::Success};
        }

        // set stop-on-error true/false
        if (startsWith(raw, "set stop-on-error ")) {
            std::string val = raw.substr(18);
            stopOnError_ = (val == "true" || val == "1");
            return {i+1, ExitCode::Success};
        }

        // if <condition>
        if (startsWith(raw, "if ") || raw == "if ok" || raw == "if fail") {
            return executeIf(lines, i);
        }

        // while <condition>
        if (startsWith(raw, "while ")) {
            return executeWhile(lines, i);
        }

        // for $VAR in <values...>
        if (startsWith(raw, "for ")) {
            return executeFor(lines, i);
        }

        // break / continue — döngü kontrol sinyalleri
        if (raw == "break") {
            flowSignal_ = FlowSignal::Break;
            return {i+1, ExitCode::Success};
        }
        if (raw == "continue") {
            flowSignal_ = FlowSignal::Continue;
            return {i+1, ExitCode::Success};
        }

        // Normal komut — değişkenleri genişlet, çalıştır
        std::string expanded = expand(raw);
        ExitCode ec = dispatcher_(expanded);
        lastExitCode_ = ec;
        return {i+1, ec};
    }

    // ── if <condition> ... [elif ...] [else] fi ──────────────────────────────
    std::pair<size_t, ExitCode>
    executeIf(const std::vector<std::string>& lines, size_t i) {
        std::string condition = trim(lines[i]).substr(3); // "if " sonrası

        // Blokları topla
        std::vector<std::string> thenLines, elseLines;
        bool inElse = false;
        size_t j = i + 1;
        int depth = 1;

        while (j < lines.size()) {
            std::string ln = trim(lines[j]);
            if (startsWith(ln, "if ")) ++depth;
            if (ln == "fi") { --depth; if (depth == 0) { ++j; break; } }
            if (depth == 1 && (ln == "else" || startsWith(ln, "elif "))) {
                inElse = true;
                if (startsWith(ln, "elif ")) {
                    // elif → recursive if
                    elseLines.push_back("if " + ln.substr(5));
                    while (++j < lines.size()) {
                        std::string ll = trim(lines[j]);
                        elseLines.push_back(ll);
                        if (ll == "fi") break;
                    }
                    ++j;
                    break;
                }
                ++j; continue;
            }
            if (!inElse) thenLines.push_back(lines[j]);
            else         elseLines.push_back(lines[j]);
            ++j;
        }

        bool condResult = evalCondition(condition);
        ExitCode ec = ExitCode::Success;
        if (condResult) {
            ec = execute(thenLines);
        } else if (!elseLines.empty()) {
            ec = execute(elseLines);
        }
        return {j, ec};
    }

    // ── while <condition> ... done ───────────────────────────────────────────
    std::pair<size_t, ExitCode>
    executeWhile(const std::vector<std::string>& lines, size_t i) {
        std::string condition = trim(lines[i]).substr(6); // "while " sonrası

        // Blok satırlarını topla
        std::vector<std::string> body;
        size_t j = i + 1;
        int depth = 1;
        while (j < lines.size()) {
            std::string ln = trim(lines[j]);
            // Nested block starters that use "done" as terminator
            if (startsWith(ln, "while ") || startsWith(ln, "for ")) ++depth;
            if (ln == "done") {
                --depth;
                if (depth == 0) { ++j; break; }
            }
            body.push_back(lines[j]);
            ++j;
        }

        static constexpr int MAX_ITERATIONS = 10000;
        int iter = 0;
        ExitCode ec = ExitCode::Success;
        while (evalCondition(condition) && iter++ < MAX_ITERATIONS) {
            flowSignal_ = FlowSignal::None;
            ec = execute(body);
            if (flowSignal_ == FlowSignal::Break) { ec = ExitCode::Success; break; }
            if (flowSignal_ == FlowSignal::Continue) { continue; }
            flowSignal_ = FlowSignal::None;
            if (stopOnError_ && ec != ExitCode::Success) break;
        }
        flowSignal_ = FlowSignal::None; // üst bloğa sızmayı engelle
        if (iter >= MAX_ITERATIONS && !stopOnError_)
            std::cerr << "Warning: while loop exceeded " << MAX_ITERATIONS << " iterations\n";
        return {j, ec};
    }

    // ── for $VAR in val1 val2 ... done ───────────────────────────────────────
    std::pair<size_t, ExitCode>
    executeFor(const std::vector<std::string>& lines, size_t i) {
        // "for $VAR in val1 val2 ..." parse et
        std::string header = trim(lines[i]).substr(4); // "for " sonrası
        std::istringstream iss(header);
        std::string varName, inKw;
        iss >> varName >> inKw;
        if (varName.empty() || inKw != "in") {
            std::cerr << "Error: for syntax: for $VAR in val1 val2 ...\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        // Değerleri topla
        std::vector<std::string> values;
        std::string val;
        while (iss >> val) values.push_back(expand(val));

        // Blok satırlarını topla
        std::vector<std::string> body;
        size_t j = i + 1;
        int depth = 1;
        while (j < lines.size()) {
            std::string ln = trim(lines[j]);
            // Nested block starters that use "done" as terminator
            if (startsWith(ln, "while ") || startsWith(ln, "for ")) ++depth;
            if (ln == "done") {
                --depth;
                if (depth == 0) { ++j; break; }
            }
            body.push_back(lines[j]);
            ++j;
        }

        // $ prefix'ini kaldır
        if (!varName.empty() && varName[0] == '$') varName = varName.substr(1);

        ExitCode ec = ExitCode::Success;
        for (const auto& v : values) {
            vars_[varName] = v;
            flowSignal_ = FlowSignal::None;
            ec = execute(body);
            if (flowSignal_ == FlowSignal::Break) { ec = ExitCode::Success; break; }
            if (flowSignal_ == FlowSignal::Continue) { continue; }
            if (stopOnError_ && ec != ExitCode::Success) break;
        }
        flowSignal_ = FlowSignal::None;
        return {j, ec};
    }

    // ── Koşul değerlendirme ───────────────────────────────────────────────────

    /** @brief Koşul string'ini değerlendir.
     *
     * Desteklenen formlar:
     * - `ok` / `success`       → son komut başarılı mı?
     * - `fail` / `error`       → son komut başarısız mı?
     * - `$VAR == value`        → string eşitlik
     * - `$VAR != value`        → string eşitsizlik
     * - `$VAR < N`             → sayısal karşılaştırma
     * - `$VAR > N`
     * - `$VAR <= N`
     * - `$VAR >= N`
     * - `not <condition>`      → olumsuzlama
     */
    bool evalCondition(const std::string& cond) const {
        std::string c = trim(expand(cond));

        if (c == "ok" || c == "success" || c == "0")
            return lastExitCode_ == ExitCode::Success;
        if (c == "fail" || c == "error" || c == "false")
            return lastExitCode_ != ExitCode::Success;
        if (c == "true" || c == "1")
            return true;
        if (c == "false")
            return false;

        // not <condition>
        if (startsWith(c, "not "))
            return !evalCondition(c.substr(4));

        // Binary comparison: LHS op RHS
        for (const auto& op : {"<=", ">=", "!=", "==", "<", ">"}) {
            auto pos = c.find(op);
            if (pos != std::string::npos) {
                std::string lhs = trim(c.substr(0, pos));
                std::string rhs = trim(c.substr(pos + strlen(op)));
                return compare(lhs, op, rhs);
            }
        }

        // Non-empty string → true
        return !c.empty();
    }

    bool compare(const std::string& lhs, const std::string& op,
                 const std::string& rhs) const {
        // lhs/rhs basit aritmetik içerebilir: "$S + 2" gibi
        std::string lv = trim(evalArithmetic(lhs));
        std::string rv = trim(evalArithmetic(rhs));
        // Sayısal karşılaştırma dene
        try {
            int l = std::stoi(lv), r = std::stoi(rv);
            if (op == "==") return l == r;
            if (op == "!=") return l != r;
            if (op == "<")  return l <  r;
            if (op == ">")  return l >  r;
            if (op == "<=") return l <= r;
            if (op == ">=") return l >= r;
        } catch (...) {
            if (op == "==") return lv == rv;
            if (op == "!=") return lv != rv;
        }
        return false;
    }

    // ── Değişken genişletme ───────────────────────────────────────────────────

    /** @brief $VAR ifadelerini değerleriyle değiştir. */
    std::string expand(const std::string& s) const {
        std::string result;
        result.reserve(s.size());
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '$' && i + 1 < s.size()) {
                // $VAR — harf, rakam veya _ ile devam ederse değişken adı
                size_t start = i + 1;
                size_t end = start;
                while (end < s.size() &&
                       (std::isalnum(static_cast<unsigned char>(s[end])) || s[end] == '_'))
                    ++end;
                if (end > start) {
                    std::string varName = s.substr(start, end - start);
                    auto it = vars_.find(varName);
                    if (it != vars_.end()) result += it->second;
                    else result += "$" + varName; // tanımsız → olduğu gibi bırak
                    i = end;
                    continue;
                }
            }
            result += s[i++];
        }
        return result;
    }

    // ── Atama tespiti ─────────────────────────────────────────────────────────

    bool isAssignment(const std::string& line) const {
        // "$VAR = value" veya "VAR = value"
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) return false;
        std::string lhs = trim(line.substr(0, eq));
        // LHS: $NAME veya NAME (harf/rakam/underscore)
        if (lhs.empty()) return false;
        size_t start = (lhs[0] == '$') ? 1 : 0;
        if (start >= lhs.size()) return false;
        for (size_t i = start; i < lhs.size(); ++i)
            if (!std::isalnum(static_cast<unsigned char>(lhs[i])) && lhs[i] != '_')
                return false;
        return true;
    }

    std::pair<std::string, std::string> parseAssignment(const std::string& line) const {
        size_t eq = line.find('=');
        std::string lhs = trim(line.substr(0, eq));
        std::string rhs = trim(line.substr(eq + 1));
        if (!lhs.empty() && lhs[0] == '$') lhs = lhs.substr(1);

        // Aritmetik: $VAR = $VAR + 1
        rhs = evalArithmetic(rhs);
        return {lhs, rhs};
    }

    /** @brief Basit aritmetik ifade değerlendir: "N + M", "N - M". */
    std::string evalArithmetic(const std::string& expr) const {
        // Önce değişkenleri genişlet
        std::string e = expand(trim(expr));
        // Basit binary op: N +|- M
        for (const auto& op : {"+", "-", "*", "/"}) {
            // Son oluşumu bul (sağdan assosiative için)
            auto pos = e.rfind(' '); // boşluklu operator
            if (pos == std::string::npos) continue;
            // "N op M" biçimi
            std::istringstream iss(e);
            std::string tok1, opStr, tok2;
            if (!(iss >> tok1 >> opStr >> tok2)) continue;
            if (opStr != op) continue;
            try {
                double a = std::stod(tok1), b = std::stod(tok2);
                double r = 0;
                if (opStr == "+") r = a + b;
                else if (opStr == "-") r = a - b;
                else if (opStr == "*") r = a * b;
                else if (opStr == "/" && b != 0) r = a / b;
                else return e; // bölme sıfıra
                // Integer çıktısı
                if (r == static_cast<int>(r))
                    return std::to_string(static_cast<int>(r));
                return std::to_string(r);
            } catch (...) { return e; }
        }
        return e;
    }

    // ── Yardımcılar ───────────────────────────────────────────────────────────
    static std::string trim(const std::string& s) {
        auto st = s.find_first_not_of(" \t\r\n");
        if (st == std::string::npos) return "";
        auto en = s.find_last_not_of(" \t\r\n");
        return s.substr(st, en - st + 1);
    }
    static bool startsWith(const std::string& s, const std::string& pfx) {
        return s.size() >= pfx.size() && s.substr(0, pfx.size()) == pfx;
    }
};
