// =============================================================================
//  Z+2AI BRAIN — Unified Single-File Offline Hindi Cognitive Engine
//  Architect / Creator : ज़ीशान (Zeeshan)
//  Optimized for Samsung Galaxy A12 (4GB RAM, Sequential Step-by-Step Pipeline)
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
#include <deque>
#include <functional>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace z2ai {

// =============================================================================
// SECTION 1 — CORE UTILITIES & MEMORY ARENA
// =============================================================================

class Arena {
public:
    explicit Arena(std::size_t size = 16384) : buffer_(size), offset_(0) {}

    void* allocate(std::size_t size) {
        std::size_t aligned_size = (size + 7) & ~7;
        if (offset_ + aligned_size > buffer_.size()) {
            return nullptr;
        }
        void* ptr = &buffer_[offset_];
        offset_ += aligned_size;
        return ptr;
    }

    void reset() {
        offset_ = 0;
    }

private:
    std::vector<uint8_t> buffer_;
    std::size_t offset_;
};

using CodePoint = char32_t;

inline std::vector<CodePoint> utf8Decode(const std::string& s) {
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

inline std::string utf8Encode(CodePoint cp) {
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

inline CodePoint lastCodePoint(const std::string& s) {
    const auto cps = utf8Decode(s);
    return cps.empty() ? CodePoint(0) : cps.back();
}

inline std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\nंः।?,!");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\nंः।?,!");
    return s.substr(b, e - b + 1);
}

inline std::string toLowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<std::string> tokenizeWords(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string w;
    while (iss >> w) {
        std::string cleaned = trim(w);
        if (!cleaned.empty()) {
            words.push_back(cleaned);
        }
    }
    return words;
}

// =============================================================================
// SECTION 2 — NUMBERS & DOMAIN DATA LIBRARIES
// =============================================================================

namespace numbers {
const std::array<std::string, 100> kUnder100 = {{
    "शून्य","एक","दो","तीन","चार","पाँच","छह","सात","आठ","नौ",
    "दस","ग्यारह","बारह","तेरह","चौदह","पंद्रह","सोलह","सत्रह","अठारह","उन्नीस",
    "बीस","इक्कीस","बाईस","तेईस","चौबीस","पच्चीस","छब्बीस","सत्ताईस","अट्ठाईस","उनतीस",
    "तीस","इकतीस","बत्तीस","तैंतीस","चौंतीस","पैंतीस","छत्तीस","सैंतीस","अड़तीस","उनतालीस",
    "चालीस","इकतालीस","बयालीस","तैंतालीस","चवालीस","पैंतालीस","छियालीस","सैंतालीस","अड़तालीस","उनचास",
    "पचास","इक्यावन","बावन","तिरेपन","चौवन","पचपन","छप्पन","सत्तावन","अट्ठावन","उनसठ",
    "साठ","इकसठ","बासठ","तिरसठ","चौंसठ","पैंसठ","छियासठ","सड़सठ","अड़सठ","उनहत्तर",
    "सत्तर","इकहत्तर","बहत्तर","तिहत्तर","चौहत्तर","पचहत्तर","छिहत्तर","सतहत्तर","अठहत्तर","उन्यासी",
    "अस्सी","इक्यासी","बयासी","तिरासी","चौरासी","पचासी","छियासी","सत्तासी","अट्ठासी","नवासी",
    "नब्बे","इक्यानवे","बानवे","तिरानवे","चौरानवे","पंचानवे","छियानवे","सत्तानवे","अट्ठानवे","निन्यानवे"
}};

const std::array<std::string, 8> kScales = {{
    "हज़ार", "लाख", "करोड़", "अरब", "खरब", "नील", "पद्म", "शंख"
}};

inline std::string threeDigit(int n) {
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

inline std::string toHindi(std::string digits) {
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

inline std::string spellNumber(const std::string& input) {
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

inline bool isNumericInput(const std::string& s) {
    bool hasDigit = false;
    for (CodePoint cp : utf8Decode(s)) {
        const bool digit = (cp >= U'0' && cp <= U'9') || (cp >= 0x0966 && cp <= 0x096F);
        if (digit) { hasDigit = true; continue; }
        if (cp == U' ' || cp == U',' || cp == U'-') continue;
        return false;
    }
    return hasDigit;
}

class DomainLibraries {
public:
    static std::string queryAllDomains(const std::string& word) {
        static const std::unordered_map<std::string, std::string> lib = {
            {"माता", "परिवार की जननी, पालनकर्ता और स्नेह की मूरत।"},
            {"पिता", "परिवार के मुखिया, रक्षक और मार्गदर्शक।"},
            {"भाई", "सहज मित्र और रक्षा करने वाला सहोदर।"},
            {"रोटी", "गेहूं के आटे से बनी ऊर्जावान और मुख्य खाद्य सामग्री।"},
            {"दूध", "संपूर्ण आहार जो शरीर को शक्ति देता है।"},
            {"शेर", "जंगल का राजा, अपनी वीरता और शक्ति के लिए प्रसिद्ध।"},
            {"गाय", "शांत स्वभाव और दूध देने वाला पूजनीय पशु।"},
            {"तोता", "हरे रंग का सुंदर पक्षी जो इंसानी आवाज़ की नकल करता है।"},
            {"खेती", "अनाज उगाने और भूमि की सेवा करने का पवित्र कार्य।"}
        };
        auto it = lib.find(word);
        return (it != lib.end()) ? it->second : "";
    }
};

class CreativeWriterLayers {
public:
    static std::string generateAtmosphere() {
        return "चारों ओर घना सन्नाटा छाया हुआ था, ठंडी हवा के झोंके पेड़ों के पत्तों को सहला रहे थे।";
    }
    static std::string getPoeticFlourish() {
        return "वक्त का पहिया चुपचाप अपनी चाल चल रहा था।";
    }
    static std::string buildTension() {
        return "कहानी का रोमांच अपने चरम पर था, हर एक पल दिल की धड़कनों को बढ़ा रहा था।";
    }
    static std::string getPsychologicalInsight() {
        return "उनके मन के भीतर विचारों का एक अंतहीन तूफान उठ रहा था।";
    }
    static std::string getProverb() {
        return "मेहनत का फल हमेशा मीठा होता है।";
    }
};

// =============================================================================
// SECTION 3 — TRIGRAM MODEL & KNOWLEDGE BASE
// =============================================================================

inline const char* kCorpus = R"CORPUS(
[क्रिएटर और प्रश्नोत्तर प्रोफ़ाइल]
यूजर: तुम्हारा नाम क्या है? एआई: मेरा नाम Z+2AI है, जिसे ज़ीशान ने शून्य से डिज़ाइन और विकसित किया है।
यूजर: तुम्हें किसने बनाया है? एआई: मुझे मेरे मालिक और डेवलपर ज़ीशान ने बनाया है।
यूजर: तुम्हारे रचयिता कौन हैं? एआई: मेरे रचयिता ज़ीशान हैं, जो इस सिस्टम के मुख्य प्रोग्रामर हैं।
)CORPUS";

class WordTrigramModel {
public:
    void train(const std::string& text) {
        std::istringstream iss(text);
        std::string line;
        std::vector<std::string> allWords;
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '[') continue;
            auto words = tokenizeWords(line);
            allWords.insert(allWords.end(), words.begin(), words.end());
        }
        for (std::size_t i = 0; i + 2 < allWords.size(); ++i) {
            model_[allWords[i]][allWords[i+1]][allWords[i+2]]++;
        }
    }

    std::string generate(std::mt19937& rng, int maxWords = 12) const {
        if (model_.empty()) return "निरंतर प्रयास और धैर्य से सफलता मिलती है।";
        auto it1 = model_.begin();
        std::uniform_int_distribution<std::size_t> dist1(0, model_.size() - 1);
        std::advance(it1, static_cast<long>(dist1(rng)));
        std::string w1 = it1->first;

        auto it2 = it1->second.begin();
        std::uniform_int_distribution<std::size_t> dist2(0, it1->second.size() - 1);
        std::advance(it2, static_cast<long>(dist2(rng)));
        std::string w2 = it2->first;

        std::string out = w1 + " " + w2;
        int wordCount = 2;
        for (int step = 0; step < maxWords && wordCount < 2000; ++step) {
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
            wordCount++;
        }
        return out;
    }
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int>>> model_;
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

// =============================================================================
// SECTION 4 — PIPELINE & STEP RUNNER ARCHITECTURE
// =============================================================================

enum class StepStatus { CONTINUE, EARLY_EXIT, ERROR };

struct StepContext {
    std::string input;
    std::string output;
    StepStatus status = StepStatus::CONTINUE;
    Arena arena;
    explicit StepContext(std::string in) : input(std::move(in)), output("") {}
};

using StepFunction = std::function<void(StepContext&)>;

// S0: Guard & Sanitization
inline void S0_Guard(StepContext& ctx) {
    ctx.input = trim(ctx.input);
    if (ctx.input == "बाहर" || ctx.input == "exit") {
        ctx.output = "अलविदा! ज़ीशान द्वारा बनाए गए Z+2AI सिस्टम से जुड़ने के लिए धन्यवाद।";
        ctx.status = StepStatus::EARLY_EXIT;
    }
}

// S1: FastPath (Numbers & Direct QA)
inline void S1_FastPath(StepContext& ctx) {
    if (ctx.status == StepStatus::EARLY_EXIT) return;
    if (isNumericInput(ctx.input)) {
        ctx.output = "संख्या शब्दों में: " + spellNumber(ctx.input);
        ctx.status = StepStatus::EARLY_EXIT;
    }
}

// Global instances for pipeline steps sharing state
static KnowledgeBase g_kb;
static WordTrigramModel g_lm;
static std::mt19937 g_rng(1337);
static bool g_is_trained = false;

inline void ensureInitialized() {
    if (!g_is_trained) {
        g_kb.loadCorpus(kCorpus);
        g_lm.train(kCorpus);
        g_is_trained = true;
    }
}

// S2: Matra & Domain Fuzzy Lookup
inline void S2_Matra(StepContext& ctx) {
    if (ctx.status == StepStatus::EARLY_EXIT) return;
    ensureInitialized();

    auto kbAns = g_kb.query(ctx.input);
    if (kbAns.has_value()) {
        ctx.output = kbAns.value();
        ctx.status = StepStatus::EARLY_EXIT;
        return;
    }

    for (const auto& word : tokenizeWords(ctx.input)) {
        std::string domainResult = DomainLibraries::queryAllDomains(word);
        if (!domainResult.empty()) {
            ctx.output = domainResult;
            ctx.status = StepStatus::EARLY_EXIT;
            return;
        }
    }
}

// S3: Lightweight Generate (Strictly Bounded to max 12 words)
inline void S3_Generate(StepContext& ctx) {
    if (ctx.status == StepStatus::EARLY_EXIT) return;
    
    std::string atmosphere = CreativeWriterLayers::generateAtmosphere();
    std::string poetic = CreativeWriterLayers::getPoeticFlourish();
    std::string draft = g_lm.generate(g_rng, 12); // Bounded to 12 words for mobile speed
    std::string tension = CreativeWriterLayers::buildTension();
    std::string psychology = CreativeWriterLayers::getPsychologicalInsight();
    std::string moral = CreativeWriterLayers::getProverb();

    ctx.output = "Z+2AI उपन्यासकार विचार:\n" + atmosphere + "\n" + poetic + "\n" + draft + "\n" + tension + "\n" + psychology + "\n(शिक्षा: " + moral + ")";
}

// S4: Polish & Formatting
inline void S4_Polish(StepContext& ctx) {
    if (ctx.status == StepStatus::EARLY_EXIT) return;
    if (!ctx.output.empty()) {
        CodePoint last_cp = lastCodePoint(ctx.output);
        if (last_cp != U'।' && last_cp != U')') {
            ctx.output += utf8Encode(U'।');
        }
    }
}

class Pipeline {
public:
    Pipeline() {
        steps_.push_back(S0_Guard);
        steps_.push_back(S1_FastPath);
        steps_.push_back(S2_Matra);
        steps_.push_back(S3_Generate);
        steps_.push_back(S4_Polish);
    }

    std::string execute(const std::string& input) {
        StepContext ctx(input);
        for (const auto& step : steps_) {
            if (ctx.status == StepStatus::EARLY_EXIT || ctx.status == StepStatus::ERROR) {
                break;
            }
            try {
                step(ctx);
            } catch (...) {
                ctx.output = "जीशान भाई, प्रोसेसिंग के दौरान हल्का भार आया था, कृपया पुनः प्रयास करें।";
                ctx.status = StepStatus::ERROR;
            }
        }
        return ctx.output.empty() ? "समझ नहीं आया, कृपया दोबारा पूछें।" : ctx.output;
    }

private:
    std::vector<StepFunction> steps_;
};

static Pipeline g_pipeline;

// =============================================================================
// SECTION 5 — ANDROID JNI BRIDGE & COMPATIBILITY ALIAS
// =============================================================================

class BrainOrchestrator {
public:
    std::string process(const std::string& input) {
        return g_pipeline.execute(input);
    }
};

using Brain = BrainOrchestrator;

} // namespace z2ai

#ifdef __ANDROID__
extern "C" JNIEXPORT jstring JNICALL
Java_com_zeeshan_z2ai_MainActivity_nativeGetAIResponse(
        JNIEnv* env,
        jobject /* this */,
        jstring jInput) {
    
    const char* nativeInput = env->GetStringUTFChars(jInput, nullptr);
    std::string inputStr(nativeInput ? nativeInput : "");
    if (nativeInput) {
        env->ReleaseStringUTFChars(jInput, nativeInput);
    }

    z2ai::BrainOrchestrator brain;
    std::string response;
    try {
        response = brain.process(inputStr);
    } catch (...) {
        response = "जीशान भाई, मोबाइल की रैम पर हल्का भार आया था, कृपया दोबारा निर्देश दें।";
    }

    return env->NewStringUTF(response.c_str());
}
#endif

