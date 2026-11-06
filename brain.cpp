// =============================================================================
//  Z+2AI BRAIN — Offline, Pure-Hindi Cognitive Engine
//  Architect / Creator : ज़ीशान (Zeeshan)
//
//  Build : g++ -std=c++17 -O2 -Wall -Wextra -o z2ai z2ai_brain.cpp
//  Run   : ./z2ai            (demo, then interactive chat; type "बाहर" to quit)
//  Note  : Source is UTF-8. On Windows use MSVC with /utf-8, or run `chcp 65001`.
//
//  ARCHITECTURE
//   1. UTF-8 / Devanagari foundation     (code points, character classes)
//   2. Phonetics & Akshara engine         (vowels, consonants, matras, halants)
//   3. Infinite number-to-words engine    (Indian system: हज़ार … शंख, recursive)
//   4. Lexicon & morphology               (POS, gender, number, verb/adj inflection)
//   5. SOV grammar engine                 (case, ergative agreement, adjectives)
//   6. Corpus & tokenizer                 (creator profile, Q&A, two moral tales)
//   7. Trigram language model             (interpolated back-off + temperature)
//   8. Knowledge base                     (keyword-scored Q&A routing)
//   9. Story weaver                       (grammar-driven original story synthesis)
//  10. Brain orchestrator                 (intent routing, response pipeline)
// =============================================================================

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace z2ai {

// =============================================================================
// SECTION 1 — UNICODE / UTF-8 FOUNDATION
// =============================================================================
using CodePoint = char32_t;

std::vector<CodePoint> utf8Decode(const std::string& s) {
    std::vector<CodePoint> out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char lead = static_cast<unsigned char>(s[i]);
        std::size_t len = 0;
        CodePoint cp = 0;
        if (lead < 0x80)              { len = 1; cp = lead; }
        else if ((lead >> 5) == 0x6)  { len = 2; cp = lead & 0x1F; }
        else if ((lead >> 4) == 0xE)  { len = 3; cp = lead & 0x0F; }
        else if ((lead >> 3) == 0x1E) { len = 4; cp = lead & 0x07; }
        else { out.push_back(0xFFFD); ++i; continue; }

        bool ok = (i + len <= s.size());
        for (std::size_t k = 1; ok && k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc >> 6) != 0x2) ok = false;
            else cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { out.push_back(0xFFFD); ++i; continue; }
        out.push_back(cp);
        i += len;
    }
    return out;
}

std::string utf8Encode(CodePoint cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

CodePoint lastCodePoint(const std::string& s) {
    const auto cps = utf8Decode(s);
    return cps.empty() ? CodePoint(0) : cps.back();
}

std::string dropLastCodePoint(const std::string& s) {
    if (s.empty()) return s;
    std::size_t i = s.size() - 1;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return s.substr(0, i);
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string toLowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

enum class CharClass {
    IndependentVowel, Consonant, Nukta, VowelSign, Halant,
    Anusvara, Chandrabindu, Visarga, Digit, Danda, Space, Latin, Other
};

CharClass classify(CodePoint cp) {
    if (cp >= 0x0904 && cp <= 0x0914) return CharClass::IndependentVowel;
    if ((cp >= 0x0915 && cp <= 0x0939) || (cp >= 0x0958 && cp <= 0x095F)) return CharClass::Consonant;
    if (cp == 0x093C) return CharClass::Nukta;
    if (cp >= 0x093E && cp <= 0x094C) return CharClass::VowelSign;
    if (cp == 0x094D) return CharClass::Halant;
    if (cp == 0x0902) return CharClass::Anusvara;
    if (cp == 0x0901) return CharClass::Chandrabindu;
    if (cp == 0x0903) return CharClass::Visarga;
    if ((cp >= 0x0966 && cp <= 0x096F) || (cp >= U'0' && cp <= U'9')) return CharClass::Digit;
    if (cp == 0x0964 || cp == 0x0965) return CharClass::Danda;
    if (cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r') return CharClass::Space;
    if ((cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z')) return CharClass::Latin;
    return CharClass::Other;
}

inline bool isNasal(CharClass c) {
    return c == CharClass::Anusvara || c == CharClass::Chandrabindu || c == CharClass::Visarga;
}

// =============================================================================
// SECTION 2 — PHONETICS & AKSHARA ENGINE
// =============================================================================
namespace phonetics {
inline std::string vowelIndependent(CodePoint cp) {
    switch (cp) {
        case 0x0905: return "a"; case 0x0906: return "aa"; case 0x0907: return "i";
        case 0x0908: return "ii"; case 0x0909: return "u"; case 0x090A: return "uu";
        case 0x090B: return "ri"; case 0x090F: return "e"; case 0x0910: return "ai";
        case 0x0913: return "o"; case 0x0914: return "au"; default: return "a";
    }
}

inline std::string vowelMatra(CodePoint cp) {
    switch (cp) {
        case 0x093E: return "aa"; case 0x093F: return "i"; case 0x0940: return "ii";
        case 0x0941: return "u"; case 0x0942: return "uu"; case 0x0943: return "ri";
        case 0x0947: return "e"; case 0x0948: return "ai"; case 0x094B: return "o";
        case 0x094C: return "au"; default: return "a";
    }
}

inline const std::unordered_map<CodePoint, std::string>& consonantRoman() {
    static const std::unordered_map<CodePoint, std::string> table = {
        {0x0915, "k"}, {0x0916, "kh"}, {0x0917, "g"}, {0x0918, "gh"}, {0x0919, "ng"},
        {0x091A, "ch"}, {0x091B, "chh"}, {0x091C, "j"}, {0x091D, "jh"}, {0x091E, "ny"},
        {0x091F, "T"}, {0x0920, "Th"}, {0x0921, "D"}, {0x0922, "Dh"}, {0x0923, "N"},
        {0x0924, "t"}, {0x0925, "th"}, {0x0926, "d"}, {0x0927, "dh"}, {0x0928, "n"},
        {0x092A, "p"}, {0x092B, "ph"}, {0x092C, "b"}, {0x092D, "bh"}, {0x092E, "m"},
        {0x092F, "y"}, {0x0930, "r"}, {0x0931, "R"}, {0x0932, "l"}, {0x0933, "L"},
        {0x0935, "v"}, {0x0936, "sh"}, {0x0937, "Sh"}, {0x0938, "s"}, {0x0939, "h"}
    };
    return table;
}
} // namespace phonetics

struct Akshara {
    std::string text;
    std::string base;
    std::string vowel;
    bool conjunct = false;
    bool independent = false;
    bool symbol = false;
    bool halantEnd = false;
};

std::vector<Akshara> syllabify(const std::string& text) {
    const std::vector<CodePoint> cps = utf8Decode(text);
    const std::size_t n = cps.size();
    std::vector<Akshara> out;
    std::size_t i = 0;

    while (i < n) {
        const CharClass c = classify(cps[i]);
        if (c == CharClass::IndependentVowel) {
            Akshara a;
            a.independent = true;
            a.vowel = phonetics::vowelIndependent(cps[i]);
            a.base = a.text = utf8Encode(cps[i++]);
            while (i < n && isNasal(classify(cps[i]))) a.text += utf8Encode(cps[i++]);
            out.push_back(a);
        } else if (c == CharClass::Consonant) {
            Akshara a;
            a.vowel = "a";
            a.base = utf8Encode(cps[i++]);
            if (i < n && classify(cps[i]) == CharClass::Nukta) a.base += utf8Encode(cps[i++]);

            while (i + 1 < n && classify(cps[i]) == CharClass::Halant &&
                   classify(cps[i + 1]) == CharClass::Consonant) {
                a.base += utf8Encode(cps[i]) + utf8Encode(cps[i + 1]);
                a.conjunct = true;
                i += 2;
                if (i < n && classify(cps[i]) == CharClass::Nukta) a.base += utf8Encode(cps[i++]);
            }

            a.text = a.base;
            if (i < n && classify(cps[i]) == CharClass::VowelSign) {
                a.vowel = phonetics::vowelMatra(cps[i]);
                a.text += utf8Encode(cps[i++]);
            } else if (i < n && classify(cps[i]) == CharClass::Halant) {
                a.vowel.clear();
                a.halantEnd = true;
                a.text += utf8Encode(cps[i++]);
            }
            while (i < n && isNasal(classify(cps[i]))) a.text += utf8Encode(cps[i++]);
            out.push_back(a);
        } else if (c == CharClass::Space || c == CharClass::Digit || c == CharClass::Danda ||
                   c == CharClass::Latin || c == CharClass::Other) {
            Akshara a;
            a.symbol = true;
            a.base = a.text = utf8Encode(cps[i++]);
            out.push_back(a);
        } else {
            if (!out.empty() && !out.back().symbol) out.back().text += utf8Encode(cps[i]);
            ++i;
        }
    }
    return out;
}

// =============================================================================
// SECTION 3 — INFINITE NUMBER-TO-WORDS ENGINE
// =============================================================================
namespace numbers {
const std::array<std::string, 100> kUnder100 = {{
    "शून्य","एक","दो","तीन","चार","पाँच","छह","सात","आठ","नौ",
    "दस","ग्यारह","बारह","तेरह","चौदह","पंद्रह","सोलह","सत्रह","अठारह","उन्नीस",
    "बीस","इक्कीस","बाईस","तेईस","चौबीस","पच्चीस","छब्बीस","सत्ताईस","अट्ठाईस","उनतीस",
    "तीस","इकतीस","बत्तीस","तैंतीस","चौंतीस","पैंतीस","छत्तीस","सैंतीस","अड़तीस","उनतालीस",
    "चालीस","इकतालीस","बयालीस","तैंतालीस","चवालीस","पैंतालीस","छियालीस","सैंतालीस","अड़तालीस","उनचास",
    "पचास","इक्यावन","बावन","तिरेपन","चौवन","pachpan","छप्पन","सत्तावन","अट्ठावन","उनसठ",
    "साठ","इकसठ","बासठ","तिरसठ","चौंसठ","पैंसठ","छियासठ","सड़सठ","अड़सठ","उनहत्तर",
    "सत्तर","इकहत्तर","बहत्तर","तिहत्तर","चौहत्तर","पचहत्तर","छिहत्तर","सतहत्तर","अठहत्तर","उन्यासी",
    "अस्सी","इक्यासी","बयासी","तिरासी","चौरासी","पचासी","छियासी","सत्तासी","अट्ठासी","नवासी",
    "नब्बे","इक्यानवे","बानवे","तिरानवे","चौरानवे","पंचानवे","छियानवे","सत्तानवे","अट्ठानवे","निन्यानवे"
}};

const std::array<std::string, 8> kScales = {{
    "हज़ार", "लाख", "करोड़", "अरब", "खरब", "नील", "पद्म", "शंख"
}};

std::string threeDigit(int n) {
    std::string out;
    const int h = n / 100;
    const int r = n % 100;
    if (h != 0) out = kUnder100[h] + " सौ";
    if (r != 0) {
        if (!out.empty()) out += ' ';
        out += kUnder100[r];
    }
    return out;
}

std::string toHindi(std::string digits) {
    const std::size_t firstNonZero = digits.find_first_not_of('0');
    if (digits.empty() || firstNonZero == std::string::npos) return "शून्य";
    digits.erase(0, firstNonZero);

    if (digits.size() > 19) {
        const std::size_t split = digits.size() - 17;
        std::string out = toHindi(digits.substr(0, split)) + " शंख";
        const std::string low = digits.substr(split);
        if (low.find_first_not_of('0') != std::string::npos) out += " " + toHindi(low);
        return out;
    }

    const std::size_t len = digits.size();
    const int hundreds = std::stoi(len > 3 ? digits.substr(len - 3) : digits);
    std::string rest = len > 3 ? digits.substr(0, len - 3) : std::string();
    std::vector<std::string> groups;
    for (std::size_t k = 0; !rest.empty(); ++k) {
        const std::size_t take = rest.size() >= 2 ? 2 : 1;
        const int g = std::stoi(rest.substr(rest.size() - take));
        rest.erase(rest.size() - take);
        if (g != 0) groups.push_back(kUnder100[g] + " " + kScales[k]);
    }

    std::vector<std::string> words(groups.rbegin(), groups.rend());
    if (hundreds != 0) words.push_back(threeDigit(hundreds));

    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i) out += ' ';
        out += words[i];
    }
    return out;
}
} // namespace numbers

std::string spellNumber(const std::string& input) {
    const bool negative = input.find('-') != std::string::npos;
    std::string digits;
    for (CodePoint cp : utf8Decode(input)) {
        if (cp >= U'0' && cp <= U'9') digits += static_cast<char>(cp);
        else if (cp >= 0x0966 && cp <= 0x096F) digits += static_cast<char>(U'0' + (cp - 0x0966));
    }
    if (digits.empty()) return "कृपया एक संख्या लिखिए, जैसे: 1500";
    const std::string words = numbers::toHindi(digits);
    return negative ? "ऋण " + words : words;
}

bool isNumericInput(const std::string& s) {
    bool hasDigit = false;
    for (CodePoint cp : utf8Decode(s)) {
        const bool digit = (cp >= U'0' && cp <= U'9') || (cp >= 0x0966 && cp <= 0x096F);
        if (digit) { hasDigit = true; continue; }
        if (cp == U' ' || cp == U',' || cp == U'-') continue;
        return false;
    }
    return hasDigit;
}

// =============================================================================
// SECTION 4 — LEXICON & MORPHOLOGY
// =============================================================================
enum class POS { Noun, Pronoun, Verb, Adjective, Postposition, Adverb, Conjunction };
enum class Gender { Masculine, Feminine };
enum class Number { Singular, Plural };
enum class Tense { Present, Past };

struct LexEntry {
    std::string surface;
    POS pos = POS::Noun;
    Gender gender = Gender::Masculine;
    bool animate = false;
    bool transitive = false;
    std::array<std::string, 3> irregularPast{};
    std::string ergative;
};

class Lexicon {
public:
    void add(LexEntry e) {
        entries_[e.surface] = std::move(e);
    }
    const LexEntry* find(const std::string& w) const {
        const auto it = entries_.find(w);
        return it == entries_.end() ? nullptr : &it->second;
    }
    std::vector<const LexEntry*> byPOS(POS p) const {
        std::vector<const LexEntry*> out;
        for (const auto& kv : entries_) if (kv.second.pos == p) out.push_back(&kv.second);
        return out;
    }
    std::size_t size() const { return entries_.size(); }
private:
    std::map<std::string, LexEntry> entries_;
};

namespace morph {
std::string adjective(const std::string& base, Gender g, Number n) {
    const bool endsAa = (lastCodePoint(base) == 0x093E);
    const std::string stem = endsAa ? dropLastCodePoint(base) : base;
    if (g == Gender::Masculine && n == Number::Singular) return base;
    if (n == Number::Plural) {
        if (g == Gender::Masculine) return endsAa ? stem + "े" : base;
        return endsAa ? stem + "ी" : base;
    }
    return endsAa ? stem + "ी" : base + "ी";
}

std::string directPlural(const LexEntry& e) {
    const std::string& s = e.surface;
    const CodePoint last = lastCodePoint(s);
    if (last == 0x0940) return dropLastCodePoint(s) + "ियाँ";
    if (last == 0x093E) {
        return e.gender == Gender::Feminine ? s + "एँ" : dropLastCodePoint(s) + "े";
    }
    if (last == 0x0906) return dropLastCodePoint(s) + "ए";
    if (e.gender == Gender::Feminine) return s + "ें";
    return s;
}

std::string obliqueSingular(const LexEntry& e) {
    const std::string& s = e.surface;
    if (e.gender != Gender::Masculine) return s;
    const CodePoint last = lastCodePoint(s);
    if (last == 0x093E) return dropLastCodePoint(s) + "े";
    if (last == 0x0906) return dropLastCodePoint(s) + "ए";
    return s;
}

std::string obliquePlural(const LexEntry& e) {
    const std::string& s = e.surface;
    const CodePoint last = lastCodePoint(s);
    if (last == 0x0940) return dropLastCodePoint(s) + "ियों";
    if (last == 0x093E) {
        return e.gender == Gender::Feminine ? s + "ओं" : dropLastCodePoint(s) + "ों";
    }
    if (last == 0x0906) return dropLastCodePoint(s) + "ओं";
    return s + "ों";
}

std::string nounHead(const LexEntry& e, Number n, bool takesPostposition) {
    if (e.pos == POS::Pronoun) return e.surface;
    if (takesPostposition) return n == Number::Plural ? obliquePlural(e) : obliqueSingular(e);
    return n == Number::Plural ? directPlural(e) : e.surface;
}

std::string conjugate(const LexEntry& v, Gender g, Number n, Tense t) {
    const std::string& stem = v.surface;
    if (t == Tense::Present) {
        if (n == Number::Plural && g == Gender::Masculine) return stem + "ते";
        if (g == Gender::Feminine) return stem + "ती";
        return stem + "ता";
    }
    const std::size_t idx = (n == Number::Plural) ? 2 : (g == Gender::Feminine ? 1 : 0);
    if (!v.irregularPast[idx].empty()) return v.irregularPast[idx];
    const bool endsAa = (lastCodePoint(stem) == 0x093E);
    const std::string base = endsAa ? stem + "य" : stem;
    if (n == Number::Plural) return g == Gender::Feminine ? base + "ीं" : base + "े";
    return g == Gender::Feminine ? base + "ी" : base + "ा";
}

std::string presentAux(Number n) { return n == Number::Plural ? "हैं" : "है"; }
} // namespace morph

// =============================================================================
// SECTION 5 — SOV GRAMMAR ENGINE
// =============================================================================
namespace grammar {
struct Clause {
    const LexEntry* subject = nullptr;
    const LexEntry* subjectAdj = nullptr;
    Number subjectNumber = Number::Singular;
    const LexEntry* object = nullptr;
    const LexEntry* objectAdj = nullptr;
    Number objectNumber = Number::Singular;
    const LexEntry* locative = nullptr;
    const LexEntry* adverb = nullptr;
    const LexEntry* verb = nullptr;
    Tense tense = Tense::Present;
};

std::string renderClause(const Clause& c) {
    if (!c.subject || !c.verb) return "";

    const bool ergative = c.verb->transitive && c.tense == Tense::Past;
    std::vector<std::string> parts;

    if (c.adverb) parts.push_back(c.adverb->surface);

    std::string subjHead;
    if (ergative && c.subject->pos == POS::Pronoun && !c.subject->ergative.empty()) {
        subjHead = c.subject->ergative;
    } else {
        subjHead = morph::nounHead(*c.subject, c.subjectNumber, ergative);
        if (ergative) subjHead += " ने";
    }
    if (c.subjectAdj) {
        subjHead = morph::adjective(c.subjectAdj->surface, c.subject->gender, c.subjectNumber)
                   + " " + subjHead;
    }
    parts.push_back(subjHead);

    if (c.object) {
        const bool takesKo = c.object->animate;
        std::string objHead = morph::nounHead(*c.object, c.objectNumber, takesKo);
        if (takesKo) objHead += " को";
        if (c.objectAdj) {
            objHead = morph::adjective(c.objectAdj->surface, c.object->gender, c.objectNumber)
                      + " " + objHead;
        }
        parts.push_back(objHead);
    }

    if (c.locative) parts.push_back(c.locative->surface + " में");

    Gender vg = c.subject->gender;
    Number vn = c.subjectNumber;
    if (ergative) {
        if (c.object && !c.object->animate) { vg = c.object->gender; vn = c.objectNumber; }
        else { vg = Gender::Masculine; vn = Number::Singular; }
    }
    std::string verb = morph::conjugate(*c.verb, vg, vn, c.tense);
    if (c.tense == Tense::Present) verb += " " + morph::presentAux(c.subjectNumber);
    parts.push_back(verb);

    std::string sentence;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) sentence += ' ';
        sentence += parts[i];
    }
    return sentence + "।";
}
} // namespace grammar

// =============================================================================
// SECTION 6 — LEXICAL DATABASE & CORPUS
// =============================================================================
Lexicon buildLexicon() {
    Lexicon lex;
    auto add = [&](std::string surface, POS pos, Gender g, bool animate = false,
                   bool transitive = false, std::array<std::string, 3> irr = {},
                   std::string ergative = "") {
        LexEntry e;
        e.surface = std::move(surface);
        e.pos = pos;
        e.gender = g;
        e.animate = animate;
        e.transitive = transitive;
        e.irregularPast = std::move(irr);
        e.ergative = std::move(ergative);
        lex.add(std::move(e));
    };

    add("लोमड़ी", POS::Noun, Gender::Feminine, true);
    add("कछुआ", POS::Noun, Gender::Masculine, true);
    add("किसान", POS::Noun, Gender::Masculine, true);
    add("लड़का", POS::Noun, Gender::Masculine, true);
    add("लड़की", POS::Noun, Gender::Feminine, true);
    add("बंदर", POS::Noun, Gender::Masculine, true);
    add("शेर", POS::Noun, Gender::Masculine, true);
    add("हाथी", POS::Noun, Gender::Masculine, true);
    add("चिड़िया", POS::Noun, Gender::Feminine, true);
    add("भालू", POS::Noun, Gender::Masculine, true);
    add("रामू", POS::Noun, Gender::Masculine, true);
    add("ज़ीशान", POS::Noun, Gender::Masculine, true);

    add("जंगल", POS::Noun, Gender::Masculine);
    add("गाँव", POS::Noun, Gender::Masculine);
    add("नदी", POS::Noun, Gender::Feminine);
    add("पहाड़", POS::Noun, Gender::Masculine);
    add("खेत", POS::Noun, Gender::Masculine);
    add("पेड़", POS::Noun, Gender::Masculine);
    add("फसल", POS::Noun, Gender::Feminine);
    add("पानी", POS::Noun, Gender::Masculine);
    add("रोटी", POS::Noun, Gender::Feminine);
    add("किताब", POS::Noun, Gender::Feminine);
    add("बारिश", POS::Noun, Gender::Feminine);
    add("सूखा", POS::Noun, Gender::Masculine);
    add("बात", POS::Noun, Gender::Feminine);
    add("मेहनत", POS::Noun, Gender::Feminine);

    add("मैं", POS::Pronoun, Gender::Masculine, true, false, {}, "मैंने");
    add("तुम", POS::Pronoun, Gender::Masculine, true, false, {}, "तुमने");
    add("वह", POS::Pronoun, Gender::Masculine, true, false, {}, "उसने");
    add("हम", POS::Pronoun, Gender::Masculine, true, false, {}, "हमने");
    add("वे", POS::Pronoun, Gender::Masculine, true, false, {}, "उन्होंने");
    add("यह", POS::Pronoun, Gender::Masculine);

    add("खा", POS::Verb, Gender::Masculine, false, true);
    add("पढ़", POS::Verb, Gender::Masculine, false, true);
    add("लिख", POS::Verb, Gender::Masculine, false, true);
    add("सींच", POS::Verb, Gender::Masculine, false, true);
    add("देख", POS::Verb, Gender::Masculine, false, true);
    add("समझ", POS::Verb, Gender::Masculine, false, true);
    add("पकड़", POS::Verb, Gender::Masculine, false, true);
    add("बना", POS::Verb, Gender::Masculine, false, true);
    add("कर", POS::Verb, Gender::Masculine, false, true, {"किया", "की", "किए"});
    add("जा", POS::Verb, Gender::Masculine, false, false, {"गया", "गई", "गए"});
    add("रह", POS::Verb, Gender::Masculine, false, false);
    add("सो", POS::Verb, Gender::Masculine, false, false);
    add("उठ", POS::Verb, Gender::Masculine, false, false);
    add("दौड़", POS::Verb, Gender::Masculine, false, false);
    add("जीत", POS::Verb, Gender::Masculine, false, false);
    add("लड़", POS::Verb, Gender::Masculine, false, false);

    add("भोला", POS::Adjective, Gender::Masculine);
    add("चालाक", POS::Adjective, Gender::Masculine);
    add("बड़ा", POS::Adjective, Gender::Masculine);
    add("छोटा", POS::Adjective, Gender::Masculine);
    add("अच्छा", POS::Adjective, Gender::Masculine);
    add("हरा", POS::Adjective, Gender::Masculine);
    add("सुंदर", POS::Adjective, Gender::Masculine);

    add("एक समय", POS::Adverb, Gender::Masculine);
    add("धीरे-धीरे", POS::Adverb, Gender::Masculine);
    add("अचानक", POS::Adverb, Gender::Masculine);
    add("हमेशा", POS::Adverb, Gender::Masculine);
    add("सुबह", POS::Adverb, Gender::Masculine);

    add("को", POS::Postposition, Gender::Masculine);
    add("में", POS::Postposition, Gender::Masculine);
    add("से", POS::Postposition, Gender::Masculine);
    add("और", POS::Conjunction, Gender::Masculine);
    add("लेकिन", POS::Conjunction, Gender::Masculine);
    add("क्योंकि", POS::Conjunction, Gender::Masculine);

    return lex;
}

inline const char* kCorpus = R"CORPUS(
[हिंदी वर्णमाला और अक्षर ज्ञान]
स्वर: अ आ इ ई उ ऊ ऋ ए ऐ ओ औ अं अः
व्यंजन: क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ ब भ म य र ल व श ष स ह क्ष त्र ज्ञ

[हिंदी गिनती]
एक दो तीन चार पाँच छह सात आठ नौ दस ग्यारह बारह तेरह चौदह पंद्रह सोलह सत्रह अठारह उन्नीस बीस
पहला दूसरा तीसरा चौथा पाँचवाँ छठा सातवाँ आठवाँ नौवाँ दसवाँ

[व्याकरण और वाक्य-रचना]
संज्ञा: राम, सीता, दिल्ली, किताब, आम, जंगल, किसान, लोमड़ी, कछुआ, गाँव, पानी, फसल, मेहनत।
सर्वनाम: मैं, तुम, वह, यह, हम, वे, मेरा, तुम्हारा, उसका।
क्रिया: है, था, गाती, पढ़ता, जाता, खाता, रहता, करती, दौड़ता, सोता, उठता, सींचता, जीतता।
विशेषण: चालाक, भोला, हरा-भरा, अच्छा, बड़ा, छोटा, सुंदर, तेज़, धीमा।
वाक्य-रचना: कर्ता कर्म क्रिया। किसान खेत में रहता है। रामू ने फसल सींची। लड़के ने कछुए को पकड़ा।

[क्रिएटर और प्रश्नोत्तर प्रोफ़ाइल]
यूजर: तुम्हारा नाम क्या है? एआई: मेरा नाम Z+2AI है, जिसे ज़ीशान ने शून्य से डिज़ाइन और विकसित किया है।
यूजर: तुम्हें किसने बनाया है? एआई: मुझे मेरे मालिक और डेवलपर ज़ीशान ने बनाया है।
यूजर: तुम्हारे रचयिता कौन हैं? एआई: मेरे रचयिता ज़ीशान हैं, जो इस सिस्टम के मुख्य प्रोग्रामर हैं।
यूजर: क्या तुम्हें ज़ीशान ने बनाया है? एआई: हाँ, मुझे ज़ीशान ने ही अपनी कोडिंग और तकनीकी कौशल से बनाया है।
यूजर: तुम्हारा मालिक कौन है? एआई: मेरे मालिक और निर्माता ज़ीशान हैं।
यूजर: ज़ीशान कौन हैं? एआई: ज़ीशान मेरे क्रिएटर और डेवलपर हैं, जिन्होंने मुझे सोचने और समझने की क्षमता दी है।
यूजर: तुम्हें बनाने का मुख्य उद्देश्य क्या है? एआई: शुद्ध हिंदी में टेक्स्ट समझना, डेटा का विश्लेषण करना और स्वतंत्र रूप से हिंदी वाक्य और कहानियाँ उत्पन्न करना है।

[नैतिक कहानियाँ]
एक समय की बात है, एक घने जंगल में एक चालाक लोमड़ी और एक भोला कछुआ रहते थे। कछुआ बहुत मेहनती था और हमेशा अपने काम में लगा रहता था। एक दिन लोमड़ी ने कछुए से कहा कि वह उससे तेज दौड़ सकती है। दोनों ने दौड़ लगाने का फैसला किया। कछुआ धीरे-धीरे लेकिन लगातार चलता रहा और अंत में दौड़ जीत गया। इससे यह शिक्षा मिलती है कि निरंतर प्रयास और धैर्य से बड़ी से बड़ी सफलता हासिल की जा सकती है।
)CORPUS";

// =============================================================================
// SECTIONS 7, 8, 9, 10 — LANGUAGE MODEL, KB, STORY WEAVER & MAIN ORCHESTRATOR
// =============================================================================
class TrigramModel {
public:
    void train(const std::string& text) {
        auto tokens = syllabify(text);
        for (std::size_t i = 0; i + 2 < tokens.size(); ++i) {
            std::string w1 = tokens[i].text;
            std::string w2 = tokens[i+1].text;
            std::string w3 = tokens[i+2].text;
            if (tokens[i].symbol || tokens[i+1].symbol || tokens[i+2].symbol) continue;
            model_[w1][w2][w3]++;
            totals_[w1][w2]++;
        }
    }
    std::string generate(std::mt19937& rng, int maxWords = 15) const {
        if (model_.empty()) return "नमस्ते! इंजन तैयार है।";
        
        auto it1 = model_.begin();
        std::uniform_int_distribution<std::size_t> dist1(0, model_.size() - 1);
        std::advance(it1, static_cast<long>(dist1(rng))); // FIXED: Safe type-cast for Android NDK compiler
        std::string w1 = it1->first;
        
        auto it2 = it1->second.begin();
        std::uniform_int_distribution<std::size_t> dist2(0, it1->second.size() - 1);
        std::advance(it2, static_cast<long>(dist2(rng))); // FIXED: Safe type-cast for Android NDK compiler
        std::string w2 = it2->first;

        std::string out = w1 + " " + w2;
        for (int step = 0; step < maxWords; ++step) {
            auto o1 = model_.find(w1);
            if (o1 == model_.end()) break;
            auto o2 = o1->second.find(w2);
            if (o2 == o1->second.end()) break;

            const auto& choices = o2->second;
            std::vector<std::string> keys;
            std::vector<int> weights;
            for (const auto& kv : choices) {
                keys.push_back(kv.first);
                weights.push_back(kv.second);
            }
            std::discrete_distribution<int> dist(weights.begin(), weights.end());
            std::string w3 = keys[dist(rng)];
            out += " " + w3;
            w1 = w2;
            w2 = w3;
        }
        return out + "।";
    }
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int>>> model_;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> totals_;
};

class KnowledgeBase {
public:
    void loadCorpus(const std::string& corpus) {
        std::istringstream stream(corpus);
        std::string line;
        while (std::getline(stream, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '[') continue;
            auto pos = line.find("यूजर:");
            auto ansPos = line.find("एआई:");
            if (pos != std::string::npos && ansPos != std::string::npos) {
                std::string q = trim(line.substr(pos + 8, ansPos - (pos + 8)));
                std::string a = trim(line.substr(ansPos + 6));
                qa_[toLowerAscii(q)] = a;
            }
        }
    }
    std::optional<std::string> query(const std::string& q) const {
        std::string clean = toLowerAscii(trim(q));
        auto it = qa_.find(clean);
        if (it != qa_.end()) return it->second;
        for (const auto& kv : qa_) {
            if (clean.find(kv.first) != std::string::npos || kv.first.find(clean) != std::string::npos) {
                return kv.second;
            }
        }
        return std::nullopt;
    }
private:
    std::unordered_map<std::string, std::string> qa_;
};

class BrainOrchestrator {
public:
    BrainOrchestrator() {
        lexicon_ = buildLexicon();
        kb_.loadCorpus(kCorpus);
        lm_.train(kCorpus);
        rng_.seed(1337);
    }

    std::string process(const std::string& input) {
        std::string clean = trim(input);
        if (clean == "बाहर" || clean == "exit") return "अलविदा! ज़ीशान द्वारा बनाए गए Z+2AI सिस्टम से जुड़ने के लिए धन्यवाद।";

        // 1. Check numbers
        if (isNumericInput(clean)) {
            return "संख्या शब्दों में: " + spellNumber(clean);
        }

        // 2. Check Knowledge Base (Q&A)
        auto kbAns = kb_.query(clean);
        if (kbAns.has_value()) {
            return kbAns.value();
        }

        // 3. Fallback to Trigram Language Model generation
        return "Z+2AI विचार: " + lm_.generate(rng_, 10);
    }
private:
    Lexicon lexicon_;
    KnowledgeBase kb_;
    TrigramModel lm_;
    std::mt19937 rng_;
};

// --- Android JNI Bridge Compatibility Alias ---
using Brain = BrainOrchestrator;

} // namespace z2ai
