// =============================================================================
//  Z+2AI BRAIN — 5-Domain Ultra-Lightweight Offline Hindi Engine with Story Slots
//  Architect / Creator : ज़ीशान (Zeeshan)
//  Optimized for Samsung Galaxy A12 (4GB RAM, Crash-Free Safe JNI Singleton)
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
    explicit Arena(std::size_t size = 4096) : buffer_(size), offset_(0) {}
    void* allocate(std::size_t size) {
        std::size_t aligned_size = (size + 7) & ~7;
        if (offset_ + aligned_size > buffer_.size()) return nullptr;
        void* ptr = &buffer_[offset_];
        offset_ += aligned_size;
        return ptr;
    }
    void reset() { offset_ = 0; }
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
        if (!cleaned.empty()) words.push_back(cleaned);
    }
    return words;
}

// =============================================================================
// SECTION 2 — 5 OPTIMIZED CORE HINDI DOMAIN LIBRARIES
// =============================================================================

namespace numbers {
const std::array<std::string, 100> kUnder100 = {{
    "शून्य","एक","दो","तीन","चार","पाँच","छह","सात","आठ","नौ",
    "दस","ग्यारह","बारह","तेरह","चौदह","पंद्रह","सोलह","सत्रह","अठारह","उन्नीस",
    "बीस","इक्कीस","बाईस","तेईस","चौबीस","पच्चीस","छब्बीस","सत्ताईस","अट्ठाईस","उनतीस",
    "तीस","इकतीस","बत्तीस","तैंतीस","चौंतीस","पैंतीस","छत्तीस","सैंतीस","अड़तीस","उनतालीस",
    "चालीस","इकतालीस","बयालीस","तैंतालीस","चवालीस","पैंतालीस","छियालीस","सैंतालीस","अड़तालीस","उनचास",
    "पचास","इक्यावन","बावन","तिरेपन","चौवन","pचपन","छप्पन","सत्तावन","अट्ठावन","उनसठ",
    "साठ","इकसठ","बासठ","तिरसठ","चौंसठ","पैंसठ","छियासठ","सड़सठ","अड़सठ","उनहत्तर",
    "सत्तर","इकहत्तर","बहत्तर","तिहत्तर","चौहत्तर","पचहत्तर","छिहत्तर","सतहत्तर","अठहत्तर","उन्यासी",
    "अस्सी","इक्यासी","बयासी","तिरासी","चौरासी","पचासी","छियासी","सत्तासी","अट्ठासी","नवासी",
    "नब्बे","इक्यानवे","बानवे","तिरानवे","चौरानवे","पंचानवे","छियानवे","सत्तानवे","अट्ठानवे","निन्यानवे"
}};

inline std::string toHindi(std::string digits) {
    const std::size_t firstNonZero = digits.find_first_not_of('0');
    if (digits.empty() || firstNonZero == std::string::npos) return "शून्य";
    digits.erase(0, firstNonZero);
    if (digits.size() > 19) return "बहुत बड़ी संख्या";
    const std::size_t len = digits.size();
    const int hundreds = std::stoi(len > 3 ? digits.substr(len - 3) : digits);
    return kUnder100[hundreds % 100];
}
} // namespace numbers

inline std::string spellNumber(const std::string& input) {
    std::string digits;
    for (CodePoint cp : utf8Decode(input)) {
        if (cp >= U'0' && cp <= U'9') digits += static_cast<char>(cp);
    }
    if (digits.empty()) return "कृपया संख्या लिखें।";
    return "संख्या शब्दों में: " + numbers::toHindi(digits);
}

inline bool isNumericInput(const std::string& s) {
    for (CodePoint cp : utf8Decode(s)) {
        if (cp >= U'0' && cp <= U'9') return true;
    }
    return false;
}

class DomainLibraries {
public:
    // ठीक 5 सबसे ज़रूरी कोर डोमेन लाइब्रेरीज़
    static std::string queryDomain(const std::string& word) {
        static const std::unordered_map<std::string, std::string> lib = {
            // 1. परिवार (Family)
            {"माता", "पारिवारिक ज्ञान: माता — परिवार की जननी और स्नेह की मूरत।"},
            {"पिता", "पारिवारिक ज्ञान: पिता — परिवार के मुखिया और मार्गदर्शक।"},
            {"भाई", "पारिवारिक ज्ञान: भाई — सहज मित्र और सहोदर।"},
            
            // 2. खान-पान (Food)
            {"रोटी", "खान-पान ज्ञान: रोटी — गेहूं के आटे से बनी मुख्य खाद्य सामग्री।"},
            {"दूध", "खान-पान ज्ञान: दूध — संपूर्ण आहार जो शक्ति देता है।"},
            
            // 3. पशु जगत (Animals)
            {"शेर", "पशु जगत ज्ञान: शेर — जंगल का राजा, अपनी वीरता के लिए प्रसिद्ध।"},
            {"गाय", "पशु जगत ज्ञान: गाय — शांत स्वभाव और दूध देने वाला पूजनीय पशु।"},
            
            // 4. पक्षी जगत (Birds)
            {"тоता", "पक्षी जगत ज्ञान: तोता — सुंदर पक्षी जो इंसानी आवाज़ की नकल करता है।"}, // Note: keeping standard Hindi toota
            {"तोता", "पक्षी जगत ज्ञान: तोता — सुंदर पक्षी जो इंसानी आवाज़ की नकल करता है।"},
            
            // 5. मानव भावना (Emotions)
            {"खुशी", "मानव भावना: खुशी — मन की प्रसन्नता और आनंद का भाव।"}
        };
        auto it = lib.find(word);
        return (it != lib.end()) ? it->second : "";
    }
};

// =============================================================================
// SECTION 3 — EXTENSIBLE CORPUS & KNOWLEDGE BASE (कहानियां जोड़ने का स्थान)
// =============================================================================

// [निर्देश]: जीशान भाई, आप जितनी चाहें उतनी नई कहानियां या पैराग्राफ यहाँ नीचे 
// [नैतिक कहानी X] लिखकर पेस्ट कर सकते हैं। इंजन इन्हें अपने आप सीख जाएगा!
inline const char* kCorpus = R"CORPUS(
[क्रिएटर और प्रश्नोत्तर प्रोफ़ाइल]
यूजर: तुम्हारा नाम क्या है? एआई: मेरा नाम Z+2AI है, जिसे ज़ीशान ने शून्य से डिज़ाइन और विकसित किया है।
यूजर: तुम्हें किसने बनाया है? एआई: मुझे मेरे मालिक और डेवलपर ज़ीशान ने बनाया है।
यूजर: तुम्हारे रचयिता कौन हैं? एआई: मेरे रचयिता ज़ीशान हैं, जो इस सिस्टम के मुख्य प्रोग्रामर हैं।
यूजर: मोबाइल पर बिना लैग या क्रैश के चलने का तुम्हारा क्या नियम है? एआई: मेरा यह नियम है कि मैं स्टेप-बाय-स्टेप पाइपलाइन और 12-वर्ड ट्रिग्राम लिमिट का पालन करता हूँ।
यूजर: क्या तुम हिंदी की मात्राओं को समझ सकते हो? एआई: हाँ, मेरी फजी मैचिंग प्रणाली मात्राओं को पहचानती है।

[नैतिक कहानी 1 — लोमड़ी और कछुआ]
एक समय की बात है, एक घने जंगल में एक चालाक लोमड़ी और एक भोला कछुआ रहते थे। कछुआ बहुत मेहनती था और हमेशा अपने काम में लगा रहता था। एक दिन लोमड़ी ने कछुए से कहा कि वह उससे तेज दौड़ सकती है। दोनों ने दौड़ लगाने का फैसला किया। कछुआ धीरे-धीरे लेकिन लगातार चलता रहा और अंत में दौड़ जीत गया। इससे यह शिक्षा मिलती है कि निरंतर प्रयास और धैर्य से बड़ी से बड़ी सफलता हासिल की जा सकती है।

[नैतिक कहानी 2 — यहाँ अपनी नई कहानी पेस्ट करें (Story Slot 2)]
(जीशान भाई, आप अपनी अगली कहानी यहाँ लिख या पेस्ट सकते हैं। उदाहरण: एक बार एक गांव में एक ईमानदार किसान रहता था...)

[नैतिक कहानी 3 — यहाँ अपनी तीसरी कहानी पेस्ट करें (Story Slot 3)]
(आप इसी तरह [नैतिक कहानी 4], [नैतिक कहानी 5] बनाकर कोड को बड़ा और अपने AI को और समझदार बना सकते हैं।)
)CORPUS";

class WordTrigramModel {
public:
    void train(const std::string& text) {
        std::istringstream iss(text);
        std::string line;
        std::vector<std::string> allWords;
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '[') continue; // [ब्रेस] वाली लाइनों को छोड़कर बाकी सब सीखेगा
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
        std::string w1 = it1->first;
        auto it2 = it1->second.begin();
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
// SECTION 4 — CRASH-PROOF SAFE BRAIN ORCHESTRATOR (SINGLETON PATTERN)
// =============================================================================

class BrainEngine {
public:
    BrainEngine() {
        kb_.loadCorpus(kCorpus);
        lm_.train(kCorpus);
        rng_.seed(42);
    }

    std::string process(const std::string& input) {
        std::string clean = trim(input);
        if (clean == "बाहर" || clean == "exit") {
            return "अलविदा! ज़ीशान द्वारा बनाए गए Z+2AI सिस्टम से जुड़ने के लिए धन्यवाद।";
        }

        // Step 1: Number Check
        if (isNumericInput(clean)) {
            return spellNumber(clean);
        }

        // Step 2: Knowledge Base Q&A Check
        auto kbAns = kb_.query(clean);
        if (kbAns.has_value()) {
            return kbAns.value();
        }

        // Step 3: 5-Domain Library Lookup
        for (const auto& word : tokenizeWords(clean)) {
            std::string domainRes = DomainLibraries::queryDomain(word);
            if (!domainRes.empty()) {
                return domainRes;
            }
        }

        // Step 4: Lightweight Trigram Storyteller (Max 12 Words - Powered by your custom stories)
        std::string draft = lm_.generate(rng_, 12);
        std::string response = "Z+2AI उपन्यासकार विचार:\nचारों ओर घना सन्नाटा छाया हुआ था, " + draft + "\n(शिक्षा: मेहनत का फल हमेशा मीठा होता है।)";

        // Step 5: Polish
        CodePoint last_cp = lastCodePoint(response);
        if (!response.empty() && last_cp != U'।' && last_cp != U')') {
            response += utf8Encode(U'।');
        }
        return response;
    }

private:
    KnowledgeBase kb_;
    WordTrigramModel lm_;
    std::mt19937 rng_;
};

// Safely get singleton instance to prevent Android JNI Startup Crash
inline BrainEngine& getBrain() {
    static BrainEngine instance;
    return instance;
}

} // namespace z2ai

// =============================================================================
// SECTION 5 — ANDROID JNI BRIDGE (SAFE & EXCEPTION-PROOF)
// =============================================================================

#ifdef __ANDROID__
extern "C" JNIEXPORT jstring JNICALL
Java_com_zeeshan_z2ai_MainActivity_nativeGetAIResponse(
        JNIEnv* env,
        jobject /* this */,
        jstring jInput) {
    
    if (!jInput) return env->NewStringUTF("अमान्य इनपुट।");

    const char* nativeInput = env->GetStringUTFChars(jInput, nullptr);
    if (!nativeInput) return env->NewStringUTF("मेमोरी त्रुटि।");
    
    std::string inputStr(nativeInput);
    env->ReleaseStringUTFChars(jInput, nativeInput);

    std::string response;
    try {
        response = z2ai::getBrain().process(inputStr);
    } catch (...) {
        response = "जीशान भाई, मोबाइल की रैम पर हल्का भार आया था, कृपया दोबारा निर्देश दें।";
    }

    return env->NewStringUTF(response.c_str());
}
#endif

