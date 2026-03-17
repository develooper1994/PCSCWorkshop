/** @file ScriptEngine.h
 * @brief Bash benzeri .scard script yorumlayıcı.
 *
 * Tam dokümantasyon: SCRIPT.md
 *
 * @par Desteklenen yapılar:
 *   - Değişkenler: `$VAR = value`
 *   - Aritmetik: `+ - * / %`  (int64_t, overflow korumalı)
 *   - Bit işlemleri: `& | ^ ~ << >>`
 *   - Sayı biçimleri: ondalık, `0xFF` hex, `0b1010` binary
 *   - Boolean: `true` / `false`, `&&` / `||` / `not`
 *   - Karşılaştırma: `== != < > <= >=`
 *   - Exit code: `if ok` / `if fail`
 *   - Koşul: `if / elif / else / fi`
 *   - Döngüler: `while COND ... done`, `for $V in a b c ... done`
 *   - Döngü kontrolü: `break`, `continue`
 *   - Çıktı: `echo "msg $VAR"`
 *   - Davranış: `set stop-on-error true|false`
 *   - Yorum: `# ...`
 *   - Format fonksiyonları: `byte1() byte2() byte4() byte8() hex() len() concat() byte_at()`
 *   - Byte dizileri: `bytes(N)` alloc, `buf_set()` yaz, `buf_get()` oku, `buf_len()` uzunluk
 *   - Byte kontrolü: `assert_len($V, N)` APDU öncesi güvenli kontrol
 *
 * @par Değişkenler:
 * @code
 *   $READER = 0
 *   $KEY    = FFFFFFFFFFFF
 *   load-key -r $READER -k $KEY
 *   echo "Reader: $READER"
 * @endcode
 *
 * @par Bit işlemleri:
 * @code
 *   $FLAGS = 0b10110011
 *   $MASK  = $FLAGS & 0x0F   # alt nibble
 *   $HIGH  = $FLAGS >> 4     # üst nibble
 *   $FLAG  = $FLAGS | 0x01   # bit set
 *   $FLAG  = $FLAGS ^ 0x01   # bit toggle
 *   $INV   = ~$FLAGS         # bitwise NOT
 * @endcode
 *
 * @par Boolean ve mantıksal:
 * @code
 *   $A = 5
 *   if $A > 3 && $A < 10
 *       echo "aralıkta"
 *   fi
 *   if not $DONE || $ERROR == 0
 *       echo "devam"
 *   fi
 * @endcode
 *
 * @par Döngüler:
 * @code
 *   $S = 0
 *   while $S < 16
 *       read-sector -r 0 -s $S -k $KEY
 *       $S = $S + 1
 *   done
 *
 *   for $BLK in 0 1 2
 *       read -r 0 -p $BLK
 *   done
 * @endcode
 *
 * @par Hata yönetimi:
 * @code
 *   set stop-on-error false   # hatalar görmezden gel
 *   $MAX = 9223372036854775807
 *   $MAX = $MAX + 1           # stderr: overflow, $MAX değişmez
 *   $X = 10 / 0               # stderr: div by zero, $X değişmez
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
#include <cstdint>
#include <cstdio>
#include <iomanip>

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
    std::map<std::string, std::string> vars_;        ///< String değişkenler
    std::map<std::string, std::vector<uint8_t>> bufs_; ///< ByteArray değişkenler
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
        // Sağ taraf fonksiyon çağrısı olabilir: $X = byte1(0xFF)
        if (isAssignment(raw)) {
            auto [name, value] = parseAssignment(raw);
            // Önce ByteArray atama dene: $X = bytes(16) veya $X = $BUF
            if (startsWith(value, "bytes(")) {
                // bytes(N) — N sıfır byte oluştur
                auto inner = extractParenArg(value, "bytes");
                if (inner.has_value()) {
                    auto nOpt = safeParseInt(expand(*inner));
                    if (nOpt.has_value() && nOpt.value() > 0 && nOpt.value() <= 65536) {
                        bufs_[name] = std::vector<uint8_t>(static_cast<size_t>(nOpt.value()), 0);
                        vars_.erase(name); // string versiyonunu temizle
                        return {i+1, ExitCode::Success};
                    }
                    std::cerr << "Script error: bytes() invalid size\n";
                    return {i+1, ExitCode::GeneralError};
                }
            }
            // Diğer taraf buf değişken mi? ($DST = $SRC kopyalama)
            if (!value.empty() && value[0] == '$') {
                std::string srcName = value.substr(1);
                auto bit = bufs_.find(srcName);
                if (bit != bufs_.end()) {
                    bufs_[name] = bit->second;
                    vars_.erase(name);
                    return {i+1, ExitCode::Success};
                }
            }
            // Builtin fonksiyon çağrısı: byte1(), byte2(), hex(), len(), concat(), buf_get(), buf_as_hex()...
            std::string evaled = evalBuiltinFn(value);
            vars_[name] = evaled;
            bufs_.erase(name); // buf versiyonunu temizle
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

        // ── assert_len — APDU öncesi byte uzunluk güvenlik kontrolü ──────────
        // assert_len($VAR, N) — $VAR byte uzunluğu N değilse hata ver ve dur
        if (startsWith(raw, "assert_len ")) {
            return execAssertLen(raw, i);
        }

        // ── Buf komutları (ByteArray) ──────────────────────────────────────────
        if (startsWith(raw, "buf_set ")  || startsWith(raw, "buf_set("))  return execBufSet(raw, i);
        if (startsWith(raw, "buf_fill ") || startsWith(raw, "buf_fill(")) return execBufFill(raw, i);
        if (startsWith(raw, "buf_copy ") || startsWith(raw, "buf_copy(")) return execBufCopy(raw, i);
        if (startsWith(raw, "buf_print "))                                 return execBufPrint(raw, i);

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

        // Boolean literals
        if (c == "true"  || c == "1") return true;
        if (c == "false" || c == "0") return false;

        // Exit code koşulları
        if (c == "ok" || c == "success")
            return lastExitCode_ == ExitCode::Success;
        if (c == "fail" || c == "error")
            return lastExitCode_ != ExitCode::Success;

        // Logical AND: "cond1 && cond2"
        {
            auto pos = c.find(" && ");
            if (pos != std::string::npos)
                return evalCondition(c.substr(0, pos)) &&
                       evalCondition(c.substr(pos + 4));
        }
        // Logical OR: "cond1 || cond2"
        {
            auto pos = c.find(" || ");
            if (pos != std::string::npos)
                return evalCondition(c.substr(0, pos)) ||
                       evalCondition(c.substr(pos + 4));
        }

        // not <condition>
        if (startsWith(c, "not "))
            return !evalCondition(c.substr(4));

        // Binary comparison: LHS op RHS (uzun operatörler önce)
        for (const auto& op : {"<=", ">=", "!=", "==", "<", ">"}) {
            auto pos = c.find(op);
            if (pos != std::string::npos) {
                std::string lhs = trim(c.substr(0, pos));
                std::string rhs = trim(c.substr(pos + strlen(op)));
                return compare(lhs, op, rhs);
            }
        }

        // Sayısal değer: 0 = false, diğerleri = true
        auto intVal = safeParseInt(c);
        if (intVal.has_value()) return intVal.value() != 0;

        // Non-empty string → true
        return !c.empty();
    }

    bool compare(const std::string& lhs, const std::string& op,
                 const std::string& rhs) const {
        // lhs/rhs aritmetik/bit ifadesi içerebilir
        std::string lv = trim(evalArithmetic(lhs));
        std::string rv = trim(evalArithmetic(rhs));

        // Sayısal karşılaştırma (int64_t)
        auto la = safeParseInt(lv), ra = safeParseInt(rv);
        if (la.has_value() && ra.has_value()) {
            int64_t l = la.value(), r = ra.value();
            if (op == "==") return l == r;
            if (op == "!=") return l != r;
            if (op == "<")  return l <  r;
            if (op == ">")  return l >  r;
            if (op == "<=") return l <= r;
            if (op == ">=") return l >= r;
        }
        // String karşılaştırma
        if (op == "==") return lv == rv;
        if (op == "!=") return lv != rv;
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
                    // Buf değişken mi? → hex string olarak genişlet
                    auto bit = bufs_.find(varName);
                    if (bit != bufs_.end()) {
                        result += bufToHex(bit->second);
                    } else {
                        auto it = vars_.find(varName);
                        if (it != vars_.end()) result += it->second;
                        else result += "$" + varName;
                    }
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
        // evalBuiltinFn aritmetiği de kapsar
        rhs = evalBuiltinFn(rhs);
        return {lhs, rhs};
    }

    /** @brief Basit aritmetik ifade değerlendir: "N + M", "N - M". */
    /** @brief Aritmetik ve bit ifadelerini değerlendir.
     *
     * Desteklenen operatörler (öncelik sırasıyla):
     * - Bit: << >> & ^ |
     * - Aritmetik: * / %
     * - Aritmetik: + -
     *
     * Tüm işlemler int64_t ile yapılır; overflow durumunda hata mesajı verilir.
     * Bölme sıfıra ve sıfır ile modülo hatalarına karşı güvenlidir.
     *
     * Örnekler:
     *   $N = 3 + 4        # 7
     *   $N = 0xFF & 0x0F  # 15
     *   $N = 1 << 4       # 16
     *   $N = ~0           # -1 (bitwise NOT)
     */
    std::string evalArithmetic(const std::string& expr) const {
        // Değişkenleri genişlet
        std::string e = expand(trim(expr));
        if (e.empty()) return e;

        // Tek token: 0x / 0b literal → decimal'e dönüştür
        {
            auto v = safeParseInt(e);
            // Sadece hex/bin prefix'li literal'ları dönüştür (decimal zaten decimal)
            if (v.has_value() && e.size() >= 2 && e[0] == '0' &&
                (e[1] == 'x' || e[1] == 'X' || e[1] == 'b' || e[1] == 'B'))
                return std::to_string(v.value());
        }

        // Unary NOT: ~ operatörü ( "~ N" biçimi)
        if (e.size() >= 2 && e[0] == '~') {
            std::string operand = trim(e.substr(1));
            auto val = safeParseInt(operand);
            if (val.has_value()) {
                int64_t r = ~val.value();
                return std::to_string(r);
            }
            return e;
        }

        // Tokenize: LHS op RHS — boşlukla ayrılmış 3 token
        std::istringstream iss(e);
        std::string tok1, opStr, tok2;
        if (!(iss >> tok1 >> opStr >> tok2)) return e;

        // Desteklenen binary operatörler (öncelik burada önemli değil — kullanıcı
        // parantez kullanamaz, zincir ifade değil tek binary op bekleniyor)
        static const std::vector<std::string> ops = {
            "<<", ">>", "&", "^", "|", "+", "-", "*", "/", "%"
        };
        bool opFound = false;
        for (auto& o : ops) {
            if (opStr == o) { opFound = true; break; }
        }
        if (!opFound) return e;

        // Hex literal desteği: 0xFF, 0b1010
        auto parseVal = [this](const std::string& s) -> std::optional<int64_t> {
            std::string v = trim(expand(s));
            if (v.empty()) return std::nullopt;
            // Hex: 0x...
            if (v.size() >= 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) {
                try { return (int64_t)std::stoull(v.substr(2), nullptr, 16); }
                catch (...) { return std::nullopt; }
            }
            // Bin: 0b...
            if (v.size() >= 2 && v[0] == '0' && (v[1] == 'b' || v[1] == 'B')) {
                try { return (int64_t)std::stoull(v.substr(2), nullptr, 2); }
                catch (...) { return std::nullopt; }
            }
            return safeParseInt(v);
        };

        auto aOpt = parseVal(tok1);
        auto bOpt = parseVal(tok2);
        if (!aOpt.has_value() || !bOpt.has_value()) return e;

        int64_t a = aOpt.value(), b = bOpt.value();
        int64_t r = 0;

        // Overflow kontrolü için sabitler
        static constexpr int64_t I64_MAX = INT64_MAX;
        static constexpr int64_t I64_MIN = INT64_MIN;

        if (opStr == "+") {
            // Overflow check: (a > 0 && b > MAX-a) || (a < 0 && b < MIN-a)
            if ((b > 0 && a > I64_MAX - b) || (b < 0 && a < I64_MIN - b)) {
                std::cerr << "Script error: integer overflow in '" << e << "'\n";
                return e;
            }
            r = a + b;
        } else if (opStr == "-") {
            if ((b < 0 && a > I64_MAX + b) || (b > 0 && a < I64_MIN + b)) {
                std::cerr << "Script error: integer underflow in '" << e << "'\n";
                return e;
            }
            r = a - b;
        } else if (opStr == "*") {
            if (a != 0 && b != 0) {
                if ((a > 0 && b > 0 && a > I64_MAX / b) ||
                    (a < 0 && b < 0 && a < I64_MAX / b) ||
                    (a > 0 && b < 0 && b < I64_MIN / a) ||
                    (a < 0 && b > 0 && a < I64_MIN / b)) {
                    std::cerr << "Script error: integer overflow in '" << e << "'\n";
                    return e;
                }
            }
            r = a * b;
        } else if (opStr == "/") {
            if (b == 0) {
                std::cerr << "Script error: division by zero in '" << e << "'\n";
                return e;
            }
            if (a == I64_MIN && b == -1) {
                std::cerr << "Script error: integer overflow in division\n";
                return e;
            }
            r = a / b;
        } else if (opStr == "%") {
            if (b == 0) {
                std::cerr << "Script error: modulo by zero in '" << e << "'\n";
                return e;
            }
            r = a % b;
        } else if (opStr == "<<") {
            if (b < 0 || b >= 64) {
                std::cerr << "Script error: invalid shift amount " << b << "\n";
                return e;
            }
            r = a << b;
        } else if (opStr == ">>") {
            if (b < 0 || b >= 64) {
                std::cerr << "Script error: invalid shift amount " << b << "\n";
                return e;
            }
            r = a >> b;
        } else if (opStr == "&") {
            r = a & b;
        } else if (opStr == "^") {
            r = a ^ b;
        } else if (opStr == "|") {
            r = a | b;
        }

        return std::to_string(r);
    }

    /** @brief Güvenli int64_t parse — boolean, hex (0x), binary (0b) dahil. */
    static std::optional<int64_t> safeParseInt(const std::string& s) {
        if (s.empty())    return std::nullopt;
        if (s == "true")  return 1;
        if (s == "false") return 0;
        // Hex: 0x...
        if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            try {
                size_t pos;
                uint64_t v = std::stoull(s.substr(2), &pos, 16);
                if (pos == s.size() - 2)
                    return static_cast<int64_t>(v);
            } catch (...) {}
            return std::nullopt;
        }
        // Binary: 0b...
        if (s.size() >= 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
            try {
                size_t pos;
                uint64_t v = std::stoull(s.substr(2), &pos, 2);
                if (pos == s.size() - 2)
                    return static_cast<int64_t>(v);
            } catch (...) {}
            return std::nullopt;
        }
        // Decimal integer
        try {
            size_t pos;
            int64_t v = std::stoll(s, &pos);
            if (pos == s.size()) return v;
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }

    // ────────────────────────────────────────────────────────────────────────────
    // ── FORMAT FONKSİYONLARI ────────────────────────────────────────────────────
    // ────────────────────────────────────────────────────────────────────────────

    /** @brief Yerleşik fonksiyon çağrısını değerlendir.
     *
     * Sözdizimi: fn_name(arg)  veya  fn_name(arg1, arg2)
     *
     * | Fonksiyon       | Açıklama                            | Örnek                 |
     * |-----------------|-------------------------------------|-----------------------|
     * | byte1(N)        | 1 byte BE hex (2 karakter)          | byte1(10)  → "0A"     |
     * | byte2(N)        | 2 byte BE hex (4 karakter)          | byte2(10)  → "000A"   |
     * | byte4(N)        | 4 byte BE hex (8 karakter)          | byte4(10)  → "0000000A"|
     * | byte8(N)        | 8 byte BE hex (16 karakter)         | byte8(10)  → "000000000000000A"|
     * | hex(N)          | Minimum hex (padding yok)           | hex(10)    → "A"      |
     * | hex_pad(N,W)    | W karakter genişliğinde hex         | hex_pad(10,4) → "000A"|
     * | len(S)          | Hex string byte sayısı              | len("0AFF") → "2"     |
     * | concat(A,B)     | İki hex string'i birleştir          | concat("0A","FF")→"0AFF"|
     * | byte_at(S,N)    | Hex string'in N. byte'ını al        | byte_at("0AFF",1)→"FF"|
     * | buf_as_hex($V)  | ByteArray'i hex string'e dönüştür  | —                     |
     * | buf_get($V,N)   | ByteArray'in N. byte'ını hex al     | buf_get($B,0)→"5A"   |
     * | buf_len($V)     | ByteArray byte sayısını al          | buf_len($B)→"16"     |
     */
    std::string evalBuiltinFn(const std::string& expr) const {
        std::string e = expand(trim(expr));

        // Fonksiyon çağrısı: name(args)
        auto parenPos = e.find('(');
        if (parenPos != std::string::npos && e.back() == ')') {
            std::string fn   = trim(e.substr(0, parenPos));
            std::string args = e.substr(parenPos + 1, e.size() - parenPos - 2);

            // Argümanları ayır (virgülle)
            auto splitArgs = [](const std::string& a) {
                std::vector<std::string> parts;
                std::string cur;
                int depth = 0;
                for (char c : a) {
                    if (c == '(') ++depth;
                    else if (c == ')') --depth;
                    else if (c == ',' && depth == 0) {
                        parts.push_back(trim_s(cur));
                        cur.clear();
                        continue;
                    }
                    cur += c;
                }
                if (!cur.empty()) parts.push_back(trim_s(cur));
                return parts;
            };

            auto argList = splitArgs(args);
            // Argümanlar lazy expand: buf fonksiyonları ham $VAR adı ister,
            // numeric fonksiyonlar expand ister. Her fn kendi expand'ını yapar.
            // Sayısal argümanlar için shortcut: ne $BUF ne de buf fn ise expand et.
            auto resolveNumArg = [this](const std::string& arg) -> std::string {
                // ByteArray değişken referansı ise ham bırak
                if (!arg.empty() && arg[0] == '$') {
                    std::string vn = arg.substr(1);
                    if (bufs_.count(vn)) return arg; // ham bırak
                }
                // Nested fn çağrısı ise evalBuiltinFn ile çöz
                if (arg.find('(') != std::string::npos)
                    return trim_s(evalBuiltinFn(arg));
                // Normal: expand et
                return expand(arg);
            };

            // ── byte formatları ────────────────────────────────────────────
            if ((fn == "byte1" || fn == "byte2" || fn == "byte4" || fn == "byte8")
                && argList.size() == 1)
            {
                auto vOpt = safeParseInt(resolveNumArg(argList[0]));
                if (!vOpt.has_value()) return e;
                int64_t v = vOpt.value();
                int nBytes = (fn == "byte1") ? 1 : (fn == "byte2") ? 2 : (fn == "byte4") ? 4 : 8;

                // Range kontrolü
                int64_t maxVal = (nBytes == 8) ? INT64_MAX :
                                 (int64_t(1) << (nBytes * 8)) - 1;
                int64_t minVal = -(int64_t(1) << (nBytes * 8 - 1));
                (void)minVal; // signed handling: bit truncation ile al
                if (nBytes < 8 && (v > maxVal || v < -(maxVal+1))) {
                    std::cerr << "Script warning: " << fn << "(" << v
                              << ") truncated to " << nBytes << " byte(s)\n";
                }
                std::ostringstream oss;
                // Big-endian, nBytes * 2 hex karakter
                uint64_t uv = static_cast<uint64_t>(v);
                uint64_t mask = (nBytes == 8) ? UINT64_MAX : ((uint64_t(1) << (nBytes * 8)) - 1);
                uv &= mask;
                oss << std::hex << std::uppercase
                    << std::setfill('0') << std::setw(nBytes * 2) << uv;
                return oss.str();
            }

            // ── hex(N) — minimum hex gösterim ─────────────────────────────
            if (fn == "hex" && argList.size() == 1) {
                auto vOpt = safeParseInt(resolveNumArg(argList[0]));
                if (!vOpt.has_value()) return e;
                uint64_t uv = static_cast<uint64_t>(vOpt.value());
                std::ostringstream oss;
                oss << std::hex << std::uppercase << uv;
                return oss.str();
            }

            // ── hex_pad(N, width) — sabit genişlikte hex ──────────────────
            if (fn == "hex_pad" && argList.size() == 2) {
                auto vOpt = safeParseInt(resolveNumArg(argList[0]));
                auto wOpt = safeParseInt(resolveNumArg(argList[1]));
                if (!vOpt.has_value() || !wOpt.has_value()) return e;
                uint64_t uv = static_cast<uint64_t>(vOpt.value());
                int w = static_cast<int>(wOpt.value());
                if (w < 1 || w > 16) {
                    std::cerr << "Script error: hex_pad width must be 1-16\n";
                    return e;
                }
                std::ostringstream oss;
                oss << std::hex << std::uppercase << std::setfill('0') << std::setw(w) << uv;
                return oss.str();
            }

            // ── len(S) — hex string byte sayısı ───────────────────────────
            // len("AABBCC") = 3  (2 hex karakter = 1 byte)
            if (fn == "len" && argList.size() == 1) {
                std::string val = resolveNumArg(argList[0]);
                // Tırnak temizle
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                    val = val.substr(1, val.size() - 2);
                // ByteArray değişken mi?
                if (!val.empty() && val[0] == '$') {
                    auto bit = bufs_.find(val.substr(1));
                    if (bit != bufs_.end())
                        return std::to_string(bit->second.size());
                }
                if (val.size() % 2 != 0) {
                    std::cerr << "Script warning: len() odd hex string length\n";
                    return std::to_string((val.size() + 1) / 2);
                }
                return std::to_string(val.size() / 2);
            }

            // ── concat(A, B) — hex string birleştirme ─────────────────────
            if (fn == "concat" && argList.size() == 2) {
                std::string a = resolveNumArg(argList[0]);
                std::string b = resolveNumArg(argList[1]);
                for (auto* s : {&a, &b}) {
                    if (s->size() >= 2 && s->front() == '"' && s->back() == '"')
                        *s = s->substr(1, s->size() - 2);
                }
                return a + b;
            }

            // ── byte_at(S, N) — hex string'in N. byte'ı ─────────────────
            if (fn == "byte_at" && argList.size() == 2) {
                std::string val = resolveNumArg(argList[0]);
                auto idxOpt = safeParseInt(resolveNumArg(argList[1]));
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                    val = val.substr(1, val.size() - 2);
                if (!idxOpt.has_value()) return e;
                size_t idx = static_cast<size_t>(idxOpt.value());
                if (idx * 2 + 2 > val.size()) {
                    std::cerr << "Script error: byte_at index out of range\n";
                    return "00";
                }
                return val.substr(idx * 2, 2);
            }

            // ── buf_as_hex($V) — ByteArray → hex string ───────────────────
            if (fn == "buf_as_hex" && argList.size() == 1) {
                std::string varRef = trim(expand(argList[0]));
                // "$VAR" veya "VAR"
                std::string vname = (!varRef.empty() && varRef[0] == '$')
                    ? varRef.substr(1) : varRef;
                auto bit = bufs_.find(vname);
                if (bit == bufs_.end()) {
                    std::cerr << "Script error: buf_as_hex: no buf named " << vname << "\n";
                    return "";
                }
                return bufToHex(bit->second);
            }

            // ── buf_get($V, N) — ByteArray'in N. byte'ını al ─────────────
            if (fn == "buf_get" && argList.size() == 2) {
                std::string varRef = trim(expand(argList[0]));
                std::string vname = (!varRef.empty() && varRef[0] == '$')
                    ? varRef.substr(1) : varRef;
                auto idxOpt = safeParseInt(expand(argList[1]));
                auto bit = bufs_.find(vname);
                if (bit == bufs_.end()) {
                    std::cerr << "Script error: buf_get: no buf named " << vname << "\n";
                    return "00";
                }
                if (!idxOpt.has_value()) return "00";
                size_t idx = static_cast<size_t>(idxOpt.value());
                if (idx >= bit->second.size()) {
                    std::cerr << "Script error: buf_get index " << idx << " out of range\n";
                    return "00";
                }
                std::ostringstream oss;
                oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                    << static_cast<int>(bit->second[idx]);
                return oss.str();
            }

            // ── buf_len($V) — ByteArray uzunluğu ─────────────────────────
            if (fn == "buf_len" && argList.size() == 1) {
                std::string varRef = trim(expand(argList[0]));
                std::string vname = (!varRef.empty() && varRef[0] == '$')
                    ? varRef.substr(1) : varRef;
                auto bit = bufs_.find(vname);
                if (bit == bufs_.end()) return "0";
                return std::to_string(bit->second.size());
            }
        }

        // Fonksiyon değilse: aritmetik/bit eval sonra string bırak
        return evalArithmetic(e);
    }

    // ────────────────────────────────────────────────────────────────────────────
    // ── BYTEARAY KOMUTLARI ───────────────────────────────────────────────────────
    // ────────────────────────────────────────────────────────────────────────────

    /** @brief buf_set $BUF INDEX HEXVAL — ByteArray'e byte yaz.
     *
     * Örnekler:
     *   buf_set $DATA 0 0x5A      # index 0'a 0x5A yaz
     *   buf_set $DATA 1 $BYTE     # değişkenden yaz
     */
    std::pair<size_t, ExitCode> execBufSet(const std::string& raw, size_t i) {
        // "buf_set $BUF INDEX VALUE"
        std::istringstream iss(raw);
        std::string cmd, varRef, idxStr, valStr;
        iss >> cmd >> varRef >> idxStr >> valStr;
        std::string vname = (!varRef.empty() && varRef[0] == '$')
            ? varRef.substr(1) : varRef;
        auto bit = bufs_.find(vname);
        if (bit == bufs_.end()) {
            std::cerr << "Script error: buf_set: '" << vname << "' is not a ByteArray\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        auto idxOpt = safeParseInt(expand(idxStr));
        auto valOpt = safeParseInt(expand(valStr));
        if (!idxOpt.has_value() || !valOpt.has_value()) {
            std::cerr << "Script error: buf_set: invalid index or value\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        size_t idx = static_cast<size_t>(idxOpt.value());
        if (idx >= bit->second.size()) {
            std::cerr << "Script error: buf_set index " << idx
                      << " out of range (size=" << bit->second.size() << ")\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        bit->second[idx] = static_cast<uint8_t>(valOpt.value() & 0xFF);
        return {i+1, ExitCode::Success};
    }

    /** @brief buf_fill $BUF VALUE — tüm byte'lara aynı değeri yaz. */
    std::pair<size_t, ExitCode> execBufFill(const std::string& raw, size_t i) {
        std::istringstream iss(raw);
        std::string cmd, varRef, valStr;
        iss >> cmd >> varRef >> valStr;
        std::string vname = (!varRef.empty() && varRef[0] == '$')
            ? varRef.substr(1) : varRef;
        auto bit = bufs_.find(vname);
        if (bit == bufs_.end()) {
            std::cerr << "Script error: buf_fill: '" << vname << "' is not a ByteArray\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        auto valOpt = safeParseInt(expand(valStr));
        if (!valOpt.has_value()) {
            std::cerr << "Script error: buf_fill: invalid value\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        std::fill(bit->second.begin(), bit->second.end(),
                  static_cast<uint8_t>(valOpt.value() & 0xFF));
        return {i+1, ExitCode::Success};
    }

    /** @brief buf_copy $DST DST_OFF $SRC SRC_OFF LENGTH — bölge kopyalama.
     *
     * Örnek: src'nin 2. byte'ından başlayarak 4 byte'ı dst'nin 0. konumuna kopyala:
     *   buf_copy $DST 0 $SRC 2 4
     */
    std::pair<size_t, ExitCode> execBufCopy(const std::string& raw, size_t i) {
        std::istringstream iss(raw);
        std::string cmd, dstRef, dstOffStr, srcRef, srcOffStr, lenStr;
        iss >> cmd >> dstRef >> dstOffStr >> srcRef >> srcOffStr >> lenStr;
        auto resolve = [this](const std::string& ref) -> std::vector<uint8_t>* {
            std::string vname = (!ref.empty() && ref[0] == '$') ? ref.substr(1) : ref;
            auto it = bufs_.find(vname);
            return (it != bufs_.end()) ? &it->second : nullptr;
        };
        auto* dst = resolve(dstRef);
        auto* src = resolve(srcRef);
        if (!dst || !src) {
            std::cerr << "Script error: buf_copy: source or destination is not a ByteArray\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        auto dOpt = safeParseInt(expand(dstOffStr));
        auto sOpt = safeParseInt(expand(srcOffStr));
        auto lOpt = safeParseInt(expand(lenStr));
        if (!dOpt || !sOpt || !lOpt) {
            std::cerr << "Script error: buf_copy: invalid offsets/length\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        size_t dOff = static_cast<size_t>(dOpt.value());
        size_t sOff = static_cast<size_t>(sOpt.value());
        size_t len  = static_cast<size_t>(lOpt.value());
        if (sOff + len > src->size() || dOff + len > dst->size()) {
            std::cerr << "Script error: buf_copy: out of bounds "
                      << "(src_size=" << src->size()
                      << " dst_size=" << dst->size() << ")\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        std::copy(src->begin() + sOff, src->begin() + sOff + len,
                  dst->begin() + dOff);
        return {i+1, ExitCode::Success};
    }

    /** @brief buf_print $BUF — ByteArray içeriğini hex + ASCII döküm yaz. */
    std::pair<size_t, ExitCode> execBufPrint(const std::string& raw, size_t i) {
        std::istringstream iss(raw);
        std::string cmd, varRef;
        iss >> cmd >> varRef;
        std::string vname = (!varRef.empty() && varRef[0] == '$') ? varRef.substr(1) : varRef;
        auto bit = bufs_.find(vname);
        if (bit == bufs_.end()) {
            std::cerr << "Script error: buf_print: '" << vname << "' is not a ByteArray\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        const auto& buf = bit->second;
        // hex dump, 16 byte per line
        for (size_t j = 0; j < buf.size(); j += 16) {
            std::printf("  %04zX  ", j);
			const size_t end = (std::min)(j + 16, buf.size());
			for (size_t k = j; k < end; ++k) {
				std::printf("%02hhX ", buf[k]);
			}

			// padding
			for (size_t k = buf.size(); k < j + 16; ++k)
				std::printf("   ");

			std::printf("  |");

			for (size_t k = j; k < end; ++k) {
				unsigned char c = static_cast<unsigned char>(buf[k]);
				std::printf("%c", std::isprint(c) ? c : '.');
			}
            std::printf("|\n");
        }
        std::printf("  [%zu byte]\n", buf.size());
        return {i+1, ExitCode::Success};
    }

    // ────────────────────────────────────────────────────────────────────────────
    // ── ASSERT_LEN — APDU öncesi güvenli byte uzunluk kontrolü ──────────────────
    // ────────────────────────────────────────────────────────────────────────────

    /** @brief assert_len $VAR N — uzunluk yanlışsa scripti hatayla durdur.
     *
     * APDU gönderiminden önce payload uzunluğunu doğrular.
     * Yanlış uzunluk karta sessiz veri bozulması gönderir — kritik hata.
     *
     * Örnekler:
     *   assert_len $PAYLOAD 16    # Mifare Classic blok: tam 16 byte
     *   assert_len $CMD 7         # DESFire komut: tam 7 byte
     *   assert_len $KEY 24        # 3DES key: tam 24 byte
     */
    std::pair<size_t, ExitCode> execAssertLen(const std::string& raw, size_t i) {
        // "assert_len $VAR N"
        std::istringstream iss(raw);
        std::string cmd, varRef, expStr;
        iss >> cmd >> varRef >> expStr;
        if (varRef.empty() || expStr.empty()) {
            std::cerr << "Script error: assert_len syntax: assert_len $VAR N\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        std::string vname = (!varRef.empty() && varRef[0] == '$') ? varRef.substr(1) : varRef;
        auto expOpt = safeParseInt(expand(expStr));
        if (!expOpt.has_value()) {
            std::cerr << "Script error: assert_len: invalid expected length\n";
            return {i+1, ExitCode::InvalidArgs};
        }
        size_t expected = static_cast<size_t>(expOpt.value());
        size_t actual = 0;

        // ByteArray mı?
        auto bit = bufs_.find(vname);
        if (bit != bufs_.end()) {
            actual = bit->second.size();
        } else {
            // String değişken: hex uzunluğu
            auto it = vars_.find(vname);
            if (it == vars_.end()) {
                std::cerr << "Script error: assert_len: variable '$"
                          << vname << "' not defined\n";
                return {i+1, ExitCode::InvalidArgs};
            }
            const auto& hex = it->second;
            if (hex.size() % 2 != 0) {
                std::cerr << "Script error: assert_len: '$" << vname
                          << "' has odd hex length (" << hex.size() << " chars)\n";
                return {i+1, ExitCode::GeneralError};
            }
            actual = hex.size() / 2;
        }

        if (actual != expected) {
            std::cerr << "Script error: assert_len FAILED: '$" << vname
                      << "' is " << actual << " byte(s), expected "
                      << expected << "\n"
                      << "  Hint: use byte1/byte2/byte4/concat to build exact-size payloads\n";
            return {i+1, ExitCode::GeneralError};
        }
        return {i+1, ExitCode::Success};
    }

    // ── Yardımcılar ───────────────────────────────────────────────────────────

    /** @brief ByteArray'i uppercase hex string'e çevir. */
    static std::string bufToHex(const std::vector<uint8_t>& buf) {
        std::string r;
        r.reserve(buf.size() * 2);
        for (uint8_t b : buf) {
            r += "0123456789ABCDEF"[b >> 4];
            r += "0123456789ABCDEF"[b & 0xF];
        }
        return r;
    }

    /** @brief Parantez içini çıkar: "fn(arg)" → "arg" */
    static std::optional<std::string> extractParenArg(
        const std::string& expr, const std::string& fnName)
    {
        std::string prefix = fnName + "(";
        if (!startsWith(expr, prefix)) return std::nullopt;
        if (expr.back() != ')') return std::nullopt;
        return expr.substr(prefix.size(), expr.size() - prefix.size() - 1);
    }

    static std::string trim_s(const std::string& s) {
        auto st = s.find_first_not_of(" \t\r\n");
        if (st == std::string::npos) return "";
        auto en = s.find_last_not_of(" \t\r\n");
        return s.substr(st, en - st + 1);
    }

    static std::string trim(const std::string& s) { return trim_s(s); }

    static bool startsWith(const std::string& s, const std::string& pfx) {
        return s.size() >= pfx.size() && s.substr(0, pfx.size()) == pfx;
    }
};
