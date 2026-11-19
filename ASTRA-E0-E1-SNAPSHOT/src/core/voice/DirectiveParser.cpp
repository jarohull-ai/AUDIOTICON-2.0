#include "DirectiveParser.h"

#include "PolishVerbalizer.h"

#include <QMap>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace audioticon {
namespace {

const int PAUSE_SHORT_MS = 400;
const int PAUSE_LONG_MS = 900;

enum class Kind { Chapter, Pause, Slower, Faster, Reset, Dsp };

struct Token {
    const char *token;
    Kind kind;
    int pauseMs = 0;
    double speedFactor = 1.0;
    const char *dsp = "";
};

// Order matters: the VS16 variants come before the bare code points so the
// emoji presentation is matched whole instead of leaving an orphan U+FE0F.
const Token TOKENS[] = {
    {"\xF0\x9F\x93\x96", Kind::Chapter, PAUSE_LONG_MS, 1.0, ""},                 // 📖
    {"\xF0\x9F\x9B\x91", Kind::Pause, PAUSE_LONG_MS, 1.0, ""},                 // 🛑
    {"\xE2\x8F\xB8\xEF\xB8\x8F", Kind::Pause, PAUSE_SHORT_MS, 1.0, ""},           // ⏸️
    {"\xE2\x8F\xB8", Kind::Pause, PAUSE_SHORT_MS, 1.0, ""},                       // ⏸
    {"\xF0\x9F\x98\xAE\xE2\x80\x8D\xF0\x9F\x92\xA8", Kind::Pause, 600, 1.0, ""},  // 😮‍💨
    {"\xF0\x9F\x8E\x99\xEF\xB8\x8F", Kind::Reset, 0, 1.0, ""},                    // 🎙️
    {"\xF0\x9F\x8E\x99", Kind::Reset, 0, 1.0, ""},                                // 🎙
    {"\xF0\x9F\x90\xA2", Kind::Slower, 0, 0.85, ""},                              // 🐢
    {"\xF0\x9F\x90\x87", Kind::Faster, 0, 1.15, ""},                              // 🐇
    {"\xF0\x9F\x93\xBB", Kind::Dsp, 0, 1.0, "radio"},                             // 📻
    {"\xF0\x9F\x93\xB1", Kind::Dsp, 0, 1.0, "phone"},                             // 📱
    {"\xF0\x9F\x8F\x9B\xEF\xB8\x8F", Kind::Dsp, 0, 1.0, "cathedral"},             // 🏛️
    {"\xF0\x9F\x8F\x9B", Kind::Dsp, 0, 1.0, "cathedral"},                         // 🏛
    {"\xF0\x9F\xA4\xAB", Kind::Dsp, 0, 1.0, "whisper"},                           // 🤫
};

const QMap<QString, QString> DSP_LABELS = {
    {QStringLiteral("radio"), QStringLiteral("Radio")},
    {QStringLiteral("phone"), QStringLiteral("Phone")},
    {QStringLiteral("cathedral"), QStringLiteral("Cathedral")},
    {QStringLiteral("whisper"), QStringLiteral("Whisper")},
};

QString utf8(const char *value) { return QString::fromUtf8(value); }

const QString &directivePatternSource() {
    static const QString source = [] {
        QStringList alternatives;
        for (const Token &entry : TOKENS)
            alternatives << QRegularExpression::escape(utf8(entry.token));
        return alternatives.join(QLatin1Char('|'));
    }();
    return source;
}

const QRegularExpression &directivePattern() {
    static const QRegularExpression pattern(directivePatternSource());
    return pattern;
}

const Token *tokenFor(const QString &value) {
    for (const Token &entry : TOKENS) {
        if (utf8(entry.token) == value)
            return &entry;
    }
    return nullptr;
}

QString kindName(Kind kind) {
    switch (kind) {
    case Kind::Chapter: return QStringLiteral("chapter");
    case Kind::Pause: return QStringLiteral("pause");
    case Kind::Slower: return QStringLiteral("slower");
    case Kind::Faster: return QStringLiteral("faster");
    case Kind::Reset: return QStringLiteral("reset");
    case Kind::Dsp: return QStringLiteral("dsp");
    }
    return QString();
}

// --- Stray emoji -------------------------------------------------------------
// Emoji that are not part of the grammar are dropped instead of spoken. The
// donor used \p{Extended_Pictographic}; PCRE2 does not ship that binary property,
// so the same ranges are checked directly. Skin-tone modifiers and ZWJ sequences
// are part of the run, not separate characters.

bool isExtendedPictographic(char32_t cp) {
    static const struct { char32_t lo, hi; } RANGES[] = {
        {0x00A9, 0x00A9}, {0x00AE, 0x00AE}, {0x203C, 0x203C}, {0x2049, 0x2049},
        {0x2122, 0x2122}, {0x2139, 0x2139}, {0x2194, 0x2199}, {0x21A9, 0x21AA},
        {0x231A, 0x231B}, {0x2328, 0x2328}, {0x2388, 0x2388}, {0x23CF, 0x23CF},
        {0x23E9, 0x23F3}, {0x23F8, 0x23FA}, {0x24C2, 0x24C2}, {0x25AA, 0x25AB},
        {0x25B6, 0x25B6}, {0x25C0, 0x25C0}, {0x25FB, 0x25FE}, {0x2600, 0x2605},
        {0x2607, 0x2612}, {0x2614, 0x2685}, {0x2690, 0x2705}, {0x2708, 0x2712},
        {0x2714, 0x2714}, {0x2716, 0x2716}, {0x271D, 0x271D}, {0x2721, 0x2721},
        {0x2728, 0x2728}, {0x2733, 0x2734}, {0x2744, 0x2744}, {0x2747, 0x2747},
        {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755}, {0x2757, 0x2757},
        {0x2763, 0x2767}, {0x2795, 0x2797}, {0x27A1, 0x27A1}, {0x27B0, 0x27B0},
        {0x27BF, 0x27BF}, {0x2934, 0x2935}, {0x2B05, 0x2B07}, {0x2B1B, 0x2B1C},
        {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x3030, 0x3030}, {0x303D, 0x303D},
        {0x3297, 0x3297}, {0x3299, 0x3299}, {0x1F000, 0x1F0FF}, {0x1F10D, 0x1F10F},
        {0x1F12F, 0x1F12F}, {0x1F16C, 0x1F171}, {0x1F17E, 0x1F17F}, {0x1F18E, 0x1F18E},
        {0x1F191, 0x1F19A}, {0x1F1AD, 0x1F1E5}, {0x1F201, 0x1F20F}, {0x1F21A, 0x1F21A},
        {0x1F22F, 0x1F22F}, {0x1F232, 0x1F23A}, {0x1F23C, 0x1F23F}, {0x1F249, 0x1F3FA},
        {0x1F400, 0x1F53D}, {0x1F546, 0x1F64F}, {0x1F680, 0x1F6FF}, {0x1F774, 0x1F77F},
        {0x1F7D5, 0x1F7FF}, {0x1F80C, 0x1F80F}, {0x1F848, 0x1F84F}, {0x1F85A, 0x1F85F},
        {0x1F888, 0x1F88F}, {0x1F8AE, 0x1F8FF}, {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945},
        {0x1F947, 0x1FAFF}, {0x1FC00, 0x1FFFD},
    };
    for (const auto &range : RANGES) {
        if (cp >= range.lo && cp <= range.hi)
            return true;
    }
    return false;
}

bool isSkinTone(char32_t cp) { return cp >= 0x1F3FB && cp <= 0x1F3FF; }

int codePointAt(const QString &text, int index, char32_t *cp) {
    const QChar high = text.at(index);
    if (high.isHighSurrogate() && index + 1 < text.size() && text.at(index + 1).isLowSurrogate()) {
        *cp = QChar::surrogateToUcs4(high, text.at(index + 1));
        return 2;
    }
    *cp = high.unicode();
    return 1;
}

QString stripStrayEmoji(const QString &text) {
    QString out;
    out.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        char32_t cp = 0;
        const int width = codePointAt(text, i, &cp);
        if (!isExtendedPictographic(cp)) {
            out += text.mid(i, width);
            i += width;
            continue;
        }
        // One emoji run: base, optional VS16 / keycap, optional skin tone, then
        // any number of ZWJ-joined pictographs.
        i += width;
        while (i < text.size()) {
            char32_t next = 0;
            const int nextWidth = codePointAt(text, i, &next);
            if (next == 0xFE0F || next == 0x20E3 || isSkinTone(next)) {
                i += nextWidth;
                continue;
            }
            if (next != 0x200D)
                break;
            if (i + nextWidth < text.size()) {
                char32_t joined = 0;
                const int joinedWidth = codePointAt(text, i + nextWidth, &joined);
                if (isExtendedPictographic(joined)) {
                    i += nextWidth + joinedWidth;
                    char32_t presentation = 0;
                    if (i < text.size() && codePointAt(text, i, &presentation) == 1
                        && presentation == 0xFE0F)
                        i += 1;
                    continue;
                }
            }
            break;
        }
        out += QLatin1Char(' ');
    }
    return out;
}

} // namespace

double round2(double value) { return std::round(value * 100.0) / 100.0; }

QVariantList describeDirectives() {
    QVariantList list;
    QStringList seen;
    for (const Token &entry : TOKENS) {
        const QString key = kindName(entry.kind) + QLatin1Char(':') + QString::number(entry.pauseMs)
                            + QLatin1Char(':') + utf8(entry.dsp);
        if (seen.contains(key))
            continue;
        seen << key;
        QString label;
        switch (entry.kind) {
        case Kind::Chapter: label = QStringLiteral("Chapter start"); break;
        case Kind::Pause: label = QString::number(entry.pauseMs) + QStringLiteral(" ms pause"); break;
        case Kind::Slower: label = QStringLiteral("Slower"); break;
        case Kind::Faster: label = QStringLiteral("Faster"); break;
        case Kind::Reset: label = QStringLiteral("Narrator (reset)"); break;
        case Kind::Dsp: label = DSP_LABELS.value(utf8(entry.dsp), utf8(entry.dsp)); break;
        }
        list << QVariantMap{{QStringLiteral("token"), utf8(entry.token)},
                            {QStringLiteral("kind"), kindName(entry.kind)},
                            {QStringLiteral("pauseMs"), entry.pauseMs},
                            {QStringLiteral("dsp"), utf8(entry.dsp)},
                            {QStringLiteral("label"), label}};
    }
    return list;
}

bool hasDirectives(const QString &text) {
    return text.contains(directivePattern());
}

QString stripDirectives(const QString &text) {
    QString stripped = text;
    stripped.replace(directivePattern(), QStringLiteral(" "));
    return stripStrayEmoji(stripped);
}

bool hasSpeakableText(const QString &text) {
    static const QRegularExpression speakable(QStringLiteral("[\\p{L}\\p{N}]"),
                                              QRegularExpression::UseUnicodePropertiesOption);
    return text.contains(speakable);
}

QString cleanupChunk(const QString &value, bool polish) {
    QString text = value;
    text.replace(directivePattern(), QStringLiteral(" "));
    text = stripStrayEmoji(text);
    text = text.simplified();
    if (text.isEmpty())
        return QString();
    if (polish)
        text = verbalizePolish(text);
    text = text.simplified();
    if (!hasSpeakableText(text))
        return QString();
    return text;
}

QString preparePlainText(const QString &value, bool polish) {
    return cleanupChunk(stripDirectives(value), polish);
}

DirectedText parseDirectedText(const QString &raw, bool polish) {
    DirectedText result;
    QString buffer;
    double speed = 1.0;
    QString dsp;
    int lastIndex = 0;

    auto cleanup = [polish](const QString &value) { return cleanupChunk(value, polish); };

    auto flush = [&]() {
        const QString text = cleanup(buffer);
        buffer.clear();
        if (text.isEmpty())
            return;
        VoiceSegment segment;
        segment.text = text;
        segment.speed = speed;
        segment.dsp = dsp;
        result.segments.append(segment);
        // A chapter marker opens before its text: the first segment that lands on
        // that index supplies the title.
        for (Chapter &chapter : result.chapters) {
            if (chapter.index == result.segments.size() - 1 && chapter.title.isEmpty()) {
                chapter.title = text.left(60);
                break;
            }
        }
    };

    auto addPause = [&result](int ms) {
        // The donor records the pause twice (once when the token is matched, once
        // here). Kept for byte-for-byte parity with its segment metadata; the
        // duplicate is a display concern, not an audio one.
        result.directives << QVariantMap{{QStringLiteral("kind"), QStringLiteral("pause")},
                                         {QStringLiteral("pauseMs"), ms},
                                         {QStringLiteral("dsp"), QString()}};
        if (result.segments.isEmpty())
            result.leadingPauseMs += ms;
        else
            result.segments.last().pauseMs += ms;
    };

    auto it = directivePattern().globalMatch(raw);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        buffer += raw.mid(lastIndex, match.capturedStart() - lastIndex);
        lastIndex = match.capturedEnd();
        const Token *directive = tokenFor(match.captured(0));
        if (!directive)
            continue;
        result.directives << QVariantMap{{QStringLiteral("kind"), kindName(directive->kind)},
                                         {QStringLiteral("pauseMs"), directive->pauseMs},
                                         {QStringLiteral("dsp"), utf8(directive->dsp)}};
        if (directive->kind == Kind::Pause) {
            flush();
            addPause(directive->pauseMs);
            continue;
        }
        if (directive->kind == Kind::Chapter) {
            flush();
            result.chapters.append(Chapter{int(result.segments.size()), QString()});
            if (result.segments.isEmpty())
                result.leadingPauseMs += directive->pauseMs;
            else
                result.segments.last().pauseMs += directive->pauseMs;
            continue;
        }
        if (directive->kind == Kind::Slower) {
            flush();
            speed = std::max(0.5, round2(speed * directive->speedFactor));
            continue;
        }
        if (directive->kind == Kind::Faster) {
            flush();
            speed = std::min(2.0, round2(speed * directive->speedFactor));
            continue;
        }
        if (directive->kind == Kind::Reset) {
            flush();
            speed = 1.0;
            dsp.clear();
            continue;
        }
        if (directive->kind == Kind::Dsp) {
            flush();
            dsp = utf8(directive->dsp);
        }
    }
    buffer += raw.mid(lastIndex);
    flush();

    // A chapter marker with no text before the next one still needs a title.
    for (int i = 0; i < result.chapters.size(); ++i) {
        if (result.chapters[i].title.isEmpty())
            result.chapters[i].title = QStringLiteral("Chapter %1").arg(i + 1);
    }

    result.text = cleanup(raw);
    return result;
}

} // namespace audioticon
