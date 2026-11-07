// =============================================================================
//  Z+2AI BRAIN — 5-Domain Ultra-Lightweight Offline Hindi Engine with 10 Stories
//  Architect / Creator : ज़ीशान (Zeeshan)
//  GitHub Raw Link : https://github.com/zeeshanhusain717-collab/AI-Brain/raw/refs/heads/main/brain.cpp
//  Optimized for Samsung Galaxy A12 (4GB RAM, 100% Crash-Free Safe JNI Singleton)
// =============================================================================

#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace z2ai {

using CodePoint = char32_t;

// ---------------------------------------------------------------------------
// SECTION 1 — सुरक्षित UTF-8 / UTF-16 हेल्पर
// ---------------------------------------------------------------------------
inline std::vector<CodePoint> utf8Decode(const std::string& s) {
    std::vector<CodePoint> out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len;
        CodePoint cp;
        if (c < 0x80)              { len = 1; cp = c; }
        else if ((c >> 5) == 0x6)  { len = 2; cp = c & 0x1F; }
        else if ((c >> 4) == 0xE)  { len = 3; cp = c & 0x0F; }
        else if ((c >> 3) == 0x1E) { len = 4; cp = c & 0x07; }
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

inline void appendUtf8(std::string& out, CodePoint cp) {
    if (cp == 0) return;  // NUL नहीं चाहिए
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
}

inline std::string encodeUtf8(const std::vector<CodePoint>& cps) {
    std::string out;
    out.reserve(cps.size() * 3);
    for (CodePoint cp : cps) appendUtf8(out, cp);
    return out;
}

// UTF-16 (Java String) -> UTF-8 (सर्रोगेट पेयर सहित)
inline std::string utf16ToUtf8(const std::u16string& u) {
    std::vector<CodePoint> cps;
    cps.reserve(u.size());
    for (std::size_t i = 0; i < u.size(); ++i) {
        char32_t c = u[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < u.size() &&
            u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00);
            ++i;
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            c = 0xFFFD;
        }
        cps.push_back(c);
    }
    return encodeUtf8(cps);
}

// UTF-8 -> UTF-16 (JNI NewString के लिए)
inline std::u16string utf8ToUtf16(const std::string& s) {
    std::u16string out;
    for (CodePoint cp : utf8Decode(s)) {
        if (cp == 0) continue;
        if (cp < 0x10000) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// SECTION 2 — ट्रिम, नॉर्मलाइज़, टोकनाइज़
// ---------------------------------------------------------------------------
inline bool isEdgePunct(CodePoint c) {
    return c == U' ' || c == U'\t' || c == U'\r' || c == U'\n' ||
           c == U'।' || c == U'?' || c == U'!' || c == U',' || c == U'.';
}

inline std::string trimText(const std::string& s) {
    auto cps = utf8Decode(s);
    std::size_t b = 0, e = cps.size();
    while (b < e && isEdgePunct(cps[b])) ++b;
    while (e > b && isEdgePunct(cps[e - 1])) --e;
    return encodeUtf8(std::vector<CodePoint>(cps.begin() + b, cps.begin() + e));
}

inline std::string normalizeKey(const std::string& s) {
    std::vector<CodePoint> out;
    for (CodePoint c : utf8Decode(s)) {
        if (c == U'।' || c == U'?' || c == U'!' || c == U',' || c == U'.') continue;
        if (c >= U'A' && c <= U'Z') c = c - U'A' + U'a';
        out.push_back(c);
    }
    std::size_t b = 0, e = out.size();
    while (b < e && out[b] == U' ') ++b;
    while (e > b && out[e - 1] == U' ') --e;
    return encodeUtf8(std::vector<CodePoint>(out.begin() + b, out.begin() + e));
}

inline std::vector<std::string> tokenizeWords(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string w;
    while (iss >> w) {
        std::string t = trimText(w);
        if (!t.empty()) words.push_back(t);
    }
    return words;
}

// ---------------------------------------------------------------------------
// SECTION 3 — संख्या शब्दों में
// ---------------------------------------------------------------------------
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

inline std::string spellInt(long long n) {
    if (n < 100) return kUnder100[static_cast<std::size_t>(n)];
    std::string out;
    if (n >= 10000000) { out += spellInt(n / 10000000) + " करोड़ "; n %= 10000000; }
    if (n >= 100000)   { out += spellInt(n / 100000) + " लाख ";     n %= 100000; }
    if (n >= 1000)     { out += spellInt(n / 1000) + " हज़ार ";      n %= 1000; }
    if (n >= 100)      { out += kUnder100[n / 100] + " सौ ";          n %= 100; }
    if (n > 0)         { out += kUnder100[n]; }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}
} // namespace numbers

inline bool hasDigit(const std::string& s) {
    for (CodePoint c : utf8Decode(s)) {
        if ((c >= U'0' && c <= U'9') || (c >= U'\u0966' && c <= U'\u096F')) return true;
    }
    return false;
}

inline std::string spellNumber(const std::string& input) {
    std::string digits;
    for (CodePoint c : utf8Decode(input)) {
        if (c >= U'0' && c <= U'9') digits.push_back(static_cast<char>(c));
        else if (c >= U'\u0966' && c <= U'\u096F') digits.push_back(static_cast<char>('0' + (c - U'\u0966')));
    }
    const std::size_t nz = digits.find_first_not_of('0');
    if (digits.empty()) return "कृपया संख्या लिखें।";
    if (nz == std::string::npos) return "संख्या शब्दों में: शून्य";
    digits = digits.substr(nz);
    if (digits.size() > 11) return "यह संख्या बहुत बड़ी है।";
    return "संख्या शब्दों में: " + numbers::spellInt(std::stoll(digits));
}

// ---------------------------------------------------------------------------
// SECTION 4 — 5 डोमेन लाइब्रेरी
// ---------------------------------------------------------------------------
inline std::string domainLookup(const std::string& word) {
    static const std::unordered_map<std::string, std::string> lib = {
        {"माता", "पारिवारिक ज्ञान: माता — परिवार की जननी और स्नेह की मूरत।"},
        {"पिता", "पारिवारिक ज्ञान: पिता — परिवार के मुखिया और मार्गदर्शक।"},
        {"भाई",  "पारिवारिक ज्ञान: भाई — सहज मित्र और सहोदर।"},
        {"रोटी", "खान-पान ज्ञान: रोटी — गेहूं के आटे से बनी मुख्य खाद्य सामग्री।"},
        {"दूध",  "खान-पान ज्ञान: दूध — संपूर्ण आहार जो शक्ति देता है।"},
        {"शेर",  "पशु जगत ज्ञान: शेर — जंगल का राजा, अपनी वीरता के लिए प्रसिद्ध।"},
        {"गाय",  "पशु जगत ज्ञान: गाय — शांत स्वभाव और दूध देने वाला पूजनीय पशु।"},
        {"तोता", "पक्षी जगत ज्ञान: तोता — सुंदर पक्षी जो इंसानी आवाज़ की नकल करता है।"},
        {"खुशी", "मानव भावना: खुशी — मन की प्रसन्नता और आनंद का भाव।"}
    };
    auto it = lib.find(normalizeKey(word));
    return it != lib.end() ? it->second : std::string();
}

// ---------------------------------------------------------------------------
// SECTION 5 — कॉर्पस (10 कहानियाँ + प्रश्नोत्तर)
// ---------------------------------------------------------------------------
inline const char* kCorpus = R"CORPUS(
[क्रिएटर और प्रश्नोत्तर प्रोफ़ाइल]
यूजर: तुम्हारा नाम क्या है? एआई: मेरा नाम Z+2AI है, जिसे ज़ीशान ने शून्य से डिज़ाइन और विकसित किया है।
यूजर: आपका नाम क्या है? एआई: मेरा नाम Z+2AI है, जिसे ज़ीशान ने शून्य से डिज़ाइन और विकसित किया है।
यूजर: तुम्हें किसने बनाया है? एआई: मुझे मेरे मालिक और डेवलपर ज़ीशान ने बनाया है।
यूजर: आपके रचयिता कौन हैं? एआई: मेरे रचयिता ज़ीशान हैं, जो इस सिस्टम के मुख्य प्रोग्रामर हैं।
यूजर: मोबाइल पर बिना लैग या क्रैश के चलने का तुम्हारा क्या नियम है? एआई: मेरा यह नियम है कि मैं स्टेप-बाय-स्टेप पाइपलाइन और 12-वर्ड ट्रिग्राम लिमिट का पालन करता हूँ।
यूजर: क्या तुम हिंदी की मात्राओं को समझ सकते हो? एआई: हाँ, मेरी फजी मैचिंग प्रणाली मात्राओं को पहचानती है।

[नैतिक कहानी 1 — लोमड़ी और कछुआ]
एक समय की बात है, एक घने जंगल में एक चालाक लोमड़ी और एक भोला कछुआ रहते थे। कछुआ बहुत मेहनती था और हमेशा अपने काम में लगा रहता था। एक दिन लोमड़ी ने कछुए से कहा कि वह उससे तेज दौड़ सकती है। दोनों ने दौड़ लगाने का फैसला किया। कछुआ धीरे-धीरे लेकिन लगातार चलता रहा और अंत में दौड़ जीत गया। इससे यह शिक्षा मिलती है कि निरंतर प्रयास और धैर्य से बड़ी से बड़ी सफलता हासिल की जा सकती है।

[नैतिक कहानी 2 — ईमानदार लकड़हारा]
एक गाँव में एक ईमानदार लकड़हारा रहता था। वह रोज सुबह उठकर जंगल जाता और सूखी लकड़ियाँ काटकर अपना गुजर-बसर करता था। एक दिन उसकी कुल्हाड़ी नदी के गहरे पानी में गिर गई। वह बहुत दुखी होकर नदी किनारे रोने लगा। तब नदी के देवता ने प्रकट होकर उसे सोने और चांदी की कुल्हाड़ियाँ दीं, लेकिन उसने अपनी पुरानी लोहे की कुल्हाड़ी ही चुनी। उसकी ईमानदारी देखकर देवता प्रसन्न हुए और उसे सभी कुल्हाड़ियाँ इनाम में दे दीं।

[नैतिक कहानी 3 — प्यासा कौवा]
चिलचिलाती धूप में एक बार एक कौवा बहुत प्यासा था। वह पानी की तलाश में यहाँ-वहाँ उड़ रहा था, लेकिन उसे कहीं पानी नहीं मिला। अंत में, उसने एक बगीचे में पानी का एक घड़ा देखा। घड़े में पानी बहुत नीचे था, जहाँ उसकी चोंच नहीं पहुँच सकती थी। कौवे ने एक चतुराई सोची। उसने पास पड़े छोटे-छोटे कंकड़ उठाकर एक-एक करके घड़े में डालने शुरू किए। पानी ऊपर आ गया और कौवे ने पानी पीकर अपनी प्यास बुझाई।

[नैतिक कहानी 4 — बहादुर रोबोट टर्बो]
भविष्य के एक शांत शहर में टर्बो नाम का एक छोटा सा रोबोट रहता था। उसका काम शहर की पुरानी गलियों में रोशनी फैलाना और लोगों की मदद करना था। एक रात शहर का मुख्य पावर जेनरेटर खराब हो गया और चारों तरफ अंधेरा छा गया। टर्बो ने बिना डरे अपने इंटरनल बैटरी से खुद को जोड़ा और पूरे शहर को रोशन कर दिया। सबने उसकी बहादुरी की तारीफ की और उसे अपना सच्चा दोस्त माना।

[नैतिक कहानी 5 — दयालु हाथी और हिरण]
एक विशाल जंगल में एक दयालु हाथी रहता था। वह रोज नदी के किनारे पानी पीने जाता और रास्ते में मिलने वाले छोटे जीवों की मदद करता था। एक दिन एक शिकारी ने जाल बिछाया, जिसमें एक हिरण फंस गया। हाथी ने अपनी सूंड और ताकत से जाल को तोड़कर हिरण की जान बचाई। संकट के समय एक-दूसरे की मदद करना ही सच्ची मानवता है, यही इस घटना से सबको सिखने को मिला।

[नैतिक कहानी 6 — मेहनती किसान सोहन]
एक उपजाऊ गाँव में सोहन नाम का एक मेहनती किसान रहता था। वह सुबह होने से पहले ही अपने खेतों में पहुँच जाता था और दिन-रात कड़ी मेहनत करता था। लोग उससे कहते थे कि इतनी मेहनत मत करो, लेकिन सोहन का मानना था कि खेत हमारी माता है और इसकी सेवा करने से ही अन्न उपजता है। उस वर्ष उसकी मेहनत रंग लाई और उसके खेत में सबसे अच्छी फसल हुई, जिससे पूरा गाँव खुश हो गया।

[नैतिक कहानी 7 — जादुई बीज और माली]
एक सुंदर घाटी में एक बूढ़ा माली रहता था। उसने एक दिन बच्चों को एक जादुई बीज दिया और कहा कि इसे प्यार से सींचो। बच्चों ने हर रोज उसे पानी दिया और उसकी देखभाल की। कुछ ही हफ्तों में वह बीज एक विशाल और सुंदर फूल वाले पेड़ में बदल गया, जिसने पूरे गाँव को महका दिया। प्यार और देखभाल से हर मुश्किल काम आसान हो जाता है और प्रकृति हमें हमेशा सुंदर फल देती है।

[नैतिक कहानी 8 — समझदार उल्लू]
एक प्राचीन बरगद के पेड़ पर एक समझदार उल्लू रहता था। जंगल के सभी जानवर अपनी परेशानियाँ लेकर उसके पास आते थे। एक बार बंदर और खरगोश के बीच खाने के बँटवारे को लेकर झगड़ा हो गया। उल्लू ने दोनों की बात ध्यान से सुनी और न्याय करते हुए दोनों को बराबर हिस्सा दिया। उसने समझाया कि मिलजुलकर रहने से ही जीवन में सुख और शांति बनी रहती है।

[नैतिक कहानी 9 — वफादार कुत्ता शेरू]
एक छोटे से कस्बे में शेरू नाम का एक वफादार कुत्ता रहता था। वह रात भर पूरे मोहल्ले की रखवाली करता था। एक ठंड की रात जब एक बच्चा घर का रास्ता भटक गया और रोने लगा, तब शेरू ने उसके पास जाकर अपनी गरमी दी और जोर-जोर से भौंककर उसके घरवालों को बुलाया। शेरू की समझदारी और वफादारी के कारण उस बच्चे की जान बच गई, और सबने उसे प्यार से खाना खिलाया।

[नैतिक कहानी 10 — राहुल और घोड़ा बादल]
एक छोटे से बच्चे राहुल और उसके पालतू घोड़े बादल के बीच गहरी दोस्ती थी। राहुल रोज स्कूल से आने के बाद बादल के साथ खेलता था। एक बार जब राहुल तेज बुखार से पीड़ित था और अकेला घर में था, तब बादल ने दरवाजे पर पैर पटक-पटककर पड़ोसी को बुला लिया। पड़ोसी ने समय पर आकर डॉक्टर को दिखाया और राहुल ठीक हो गया। इस घटना ने साबित कर दिया कि सच्चा प्यार और दोस्ती किसी भी बंधन से परे होती है।
)CORPUS";

// ---------------------------------------------------------------------------
// SECTION 6 — प्रश्नोत्तर
// ---------------------------------------------------------------------------
class KnowledgeBase {
public:
    void load(const std::string& corpus) {
        const std::string userTag = "यूजर:";
        const std::string aiTag   = "एआई:";
        std::istringstream st(corpus);
        std::string line;
        while (std::getline(st, line)) {
            line = trimText(line);
            if (line.empty() || line[0] == '[') continue;
            const auto u = line.find(userTag);
            const auto a = line.find(aiTag);
            if (u == std::string::npos || a == std::string::npos || a < u) continue;

            const std::size_t qStart = u + userTag.size();
            const std::string q   = trimText(line.substr(qStart, a - qStart));
            const std::string ans = trimText(line.substr(a + aiTag.size()));
            const std::string key = normalizeKey(q);
            if (key.empty() || ans.empty()) continue;
            qa_.emplace(key, ans);
        }
    }

    std::optional<std::string> find(const std::string& input) const {
        const std::string key = normalizeKey(input);
        if (key.empty()) return std::nullopt;

        auto it = qa_.find(key);
        if (it != qa_.end()) return it->second;

        if (utf8Decode(key).size() < 6) return std::nullopt;
        for (const auto& kv : qa_) {
            if (kv.first.find(key) != std::string::npos) return kv.second;
        }
        return std::nullopt;
    }

private:
    std::unordered_map<std::string, std::string> qa_;
};

// ---------------------------------------------------------------------------
// SECTION 7 — ट्रिग्राम कहानीकार
// ---------------------------------------------------------------------------
class TrigramModel {
public:
    void train(const std::string& text) {
        std::istringstream iss(text);
        std::string line;
        std::vector<std::string> all;
        while (std::getline(iss, line)) {
            line = trimText(line);
            if (line.empty() || line[0] == '[') continue;
            auto ws = tokenizeWords(line);
            all.insert(all.end(), ws.begin(), ws.end());
        }
        for (std::size_t i = 0; i + 2 < all.size(); ++i) {
            model_[all[i]][all[i + 1]][all[i + 2]]++;
        }
    }

    std::string generate(std::mt19937& rng, int maxWords) const {
        static const std::string fallback = "निरंतर प्रयास और धैर्य से सफलता मिलती है।";
        if (model_.empty()) return fallback;

        const auto& first = *model_.begin();
        if (first.second.empty()) return fallback;
        const auto& second = *first.second.begin();

        std::string w1 = first.first;
        std::string w2 = second.first;
        std::string out = w1 + " " + w2;

        for (int step = 0; step < maxWords; ++step) {
            auto o1 = model_.find(w1);
            if (o1 == model_.end()) break;
            auto o2 = o1->second.find(w2);
            if (o2 == o1->second.end() || o2->second.empty()) break;

            std::vector<std::string> keys;
            std::vector<double> weights;
            for (const auto& kv : o2->second) {
                keys.push_back(kv.first);
                weights.push_back(static_cast<double>(kv.second));
            }
            std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
            const std::string w3 = keys[dist(rng)];
            out += " " + w3;
            w1 = w2;
            w2 = w3;
        }
        return out;
    }

private:
    std::unordered_map<std::string,
        std::unordered_map<std::string,
            std::unordered_map<std::string, int>>> model_;
};

// ---------------------------------------------------------------------------
// SECTION 8 — ब्रेन इंजन (सिंगलटन + थ्रेड-सेफ़)
// ---------------------------------------------------------------------------
class Brain {
public:
    Brain() {
        kb_.load(kCorpus);
        lm_.train(kCorpus);
        rng_.seed(42);
    }

    std::string process(const std::string& raw) {
        std::lock_guard<std::mutex> lock(mu_);

        const std::string in = trimText(raw);
        if (in.empty()) return "कुछ लिखिए, मैं सुन रहा हूँ।";
        if (in == "बाहर" || in == "exit") {
            return "अलविदा! ज़ीशान द्वारा बनाए गए Z+2AI से बात करने के लिए धन्यवाद।";
        }
        if (hasDigit(in)) return spellNumber(in);

        if (auto ans = kb_.find(in)) return *ans;

        for (const auto& w : tokenizeWords(in)) {
            const std::string d = domainLookup(w);
            if (!d.empty()) return d;
        }

        std::string draft = lm_.generate(rng_, 12);
        std::string response =
            "Z+2AI उपन्यासकार विचार:\nचारों ओर घना सन्नाटा छाया हुआ था, " +
            draft + "\n(शिक्षा: निरंतर प्रयास और धैर्य से सफलता मिलती है।)";

        const auto cps = utf8Decode(response);
        if (!cps.empty() && cps.back() != U'।' && cps.back() != U')') {
            appendUtf8(response, U'।');
        }
        return response;
    }

private:
    KnowledgeBase kb_;
    TrigramModel lm_;
    std::mt19937 rng_;
    std::mutex mu_;
};

inline Brain& brain() {
    static Brain instance;
    return instance;
}

} // namespace z2ai

// ---------------------------------------------------------------------------
// SECTION 9 — ANDROID JNI
// ---------------------------------------------------------------------------
#ifdef __ANDROID__
extern "C" JNIEXPORT jstring JNICALL
Java_com_zeeshan_z2ai_MainActivity_nativeGetAIResponse(JNIEnv* env, jobject, jstring jInput) {
    std::string input;
    if (jInput != nullptr) {
        const jsize n = env->GetStringLength(jInput);
        const jchar* chars = env->GetStringChars(jInput, nullptr);
        if (chars != nullptr) {
            std::u16string u(reinterpret_cast<const char16_t*>(chars), static_cast<std::size_t>(n));
            env->ReleaseStringChars(jInput, chars);
            input = z2ai::utf16ToUtf8(u);
        }
    }

    std::string response;
    try {
        response = z2ai::brain().process(input);
    } catch (...) {
        response = "मेमोरी में हल्की समस्या आई। कृपया दोबारा लिखें।";
    }

    const std::u16string out = z2ai::utf8ToUtf16(response);
    return env->NewString(reinterpret_cast<const jchar*>(out.data()),
                          static_cast<jsize>(out.size()));
}
#endif

