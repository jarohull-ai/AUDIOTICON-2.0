#include "PolishVerbalizer.h"

#include <QMap>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <functional>

namespace audioticon {
namespace {

// --- Word tables -------------------------------------------------------------
// Kept identical to the donor module: changing a form changes the audio, so any
// edit has to go through the golden set in tests/data/tts-golden.json.

const QStringList ONES = {QStringLiteral("zero"), QStringLiteral("jeden"), QStringLiteral("dwa"),
                          QStringLiteral("trzy"), QStringLiteral("cztery"), QStringLiteral("pięć"),
                          QStringLiteral("sześć"), QStringLiteral("siedem"), QStringLiteral("osiem"),
                          QStringLiteral("dziewięć")};
const QStringList TEENS = {QStringLiteral("dziesięć"), QStringLiteral("jedenaście"),
                           QStringLiteral("dwanaście"), QStringLiteral("trzynaście"),
                           QStringLiteral("czternaście"), QStringLiteral("piętnaście"),
                           QStringLiteral("szesnaście"), QStringLiteral("siedemnaście"),
                           QStringLiteral("osiemnaście"), QStringLiteral("dziewiętnaście")};
const QStringList TENS = {QString(), QString(), QStringLiteral("dwadzieścia"),
                          QStringLiteral("trzydzieści"), QStringLiteral("czterdzieści"),
                          QStringLiteral("pięćdziesiąt"), QStringLiteral("sześćdziesiąt"),
                          QStringLiteral("siedemdziesiąt"), QStringLiteral("osiemdziesiąt"),
                          QStringLiteral("dziewięćdziesiąt")};
const QStringList HUNDREDS = {QString(), QStringLiteral("sto"), QStringLiteral("dwieście"),
                              QStringLiteral("trzysta"), QStringLiteral("czterysta"),
                              QStringLiteral("pięćset"), QStringLiteral("sześćset"),
                              QStringLiteral("siedemset"), QStringLiteral("osiemset"),
                              QStringLiteral("dziewięćset")};

struct Scale {
    QString one, few, many;
};
const Scale SCHALE[] = {{QString(), QString(), QString()},
                        {QStringLiteral("tysiąc"), QStringLiteral("tysiące"), QStringLiteral("tysięcy")},
                        {QStringLiteral("milion"), QStringLiteral("miliony"), QStringLiteral("milionów")},
                        {QStringLiteral("miliard"), QStringLiteral("miliardy"), QStringLiteral("miliardów")}};

const QStringList GENITIVE_DAY = {
    QString(), QStringLiteral("pierwszego"), QStringLiteral("drugiego"), QStringLiteral("trzeciego"),
    QStringLiteral("czwartego"), QStringLiteral("piątego"), QStringLiteral("szóstego"),
    QStringLiteral("siódmego"), QStringLiteral("ósmego"), QStringLiteral("dziewiątego"),
    QStringLiteral("dziesiątego"), QStringLiteral("jedenastego"), QStringLiteral("dwunastego"),
    QStringLiteral("trzynastego"), QStringLiteral("czternastego"), QStringLiteral("piętnastego"),
    QStringLiteral("szesnastego"), QStringLiteral("siedemnastego"), QStringLiteral("osiemnastego"),
    QStringLiteral("dziewiętnastego"), QStringLiteral("dwudziestego"),
    QStringLiteral("dwudziestego pierwszego"), QStringLiteral("dwudziestego drugiego"),
    QStringLiteral("dwudziestego trzeciego"), QStringLiteral("dwudziestego czwartego"),
    QStringLiteral("dwudziestego piątego"), QStringLiteral("dwudziestego szóstego"),
    QStringLiteral("dwudziestego siódmego"), QStringLiteral("dwudziestego ósmego"),
    QStringLiteral("dwudziestego dziewiątego"), QStringLiteral("trzydziestego"),
    QStringLiteral("trzydziestego pierwszego")};

const QStringList MONTHS_GENITIVE = {
    QStringLiteral("stycznia"), QStringLiteral("lutego"), QStringLiteral("marca"),
    QStringLiteral("kwietnia"), QStringLiteral("maja"), QStringLiteral("czerwca"),
    QStringLiteral("lipca"), QStringLiteral("sierpnia"), QStringLiteral("września"),
    QStringLiteral("października"), QStringLiteral("listopada"), QStringLiteral("grudnia")};

const QStringList GENITIVE_ONES = {QString(), QStringLiteral("pierwszego"), QStringLiteral("drugiego"),
                                   QStringLiteral("trzeciego"), QStringLiteral("czwartego"),
                                   QStringLiteral("piątego"), QStringLiteral("szóstego"),
                                   QStringLiteral("siódmego"), QStringLiteral("ósmego"),
                                   QStringLiteral("dziewiątego")};
const QStringList GENITIVE_TEENS = {
    QStringLiteral("dziesiątego"), QStringLiteral("jedenastego"), QStringLiteral("dwunastego"),
    QStringLiteral("trzynastego"), QStringLiteral("czternastego"), QStringLiteral("piętnastego"),
    QStringLiteral("szesnastego"), QStringLiteral("siedemnastego"), QStringLiteral("osiemnastego"),
    QStringLiteral("dziewiętnastego")};
const QStringList GENITIVE_TENS = {QString(), QString(), QStringLiteral("dwudziestego"),
                                   QStringLiteral("trzydziestego"), QStringLiteral("czterdziestego"),
                                   QStringLiteral("pięćdziesiątego"), QStringLiteral("sześćdziesiątego"),
                                   QStringLiteral("siedemdziesiątego"), QStringLiteral("osiemdziesiątego"),
                                   QStringLiteral("dziewięćdziesiątego")};
const QStringList GENITIVE_HUNDREDS = {
    QString(), QStringLiteral("setnego"), QStringLiteral("dwusetnego"), QStringLiteral("trzysetnego"),
    QStringLiteral("czterysetnego"), QStringLiteral("pięćsetnego"), QStringLiteral("sześćsetnego"),
    QStringLiteral("siedemsetnego"), QStringLiteral("osiemsetnego"), QStringLiteral("dziewięćsetnego")};

const QMap<int, QString> HOUR_FEMININE = {
    {1, QStringLiteral("pierwsza")}, {2, QStringLiteral("druga")}, {3, QStringLiteral("trzecia")},
    {4, QStringLiteral("czwarta")}, {5, QStringLiteral("piąta")}, {6, QStringLiteral("szósta")},
    {7, QStringLiteral("siódma")}, {8, QStringLiteral("ósma")}, {9, QStringLiteral("dziewiąta")},
    {10, QStringLiteral("dziesiąta")}, {11, QStringLiteral("jedenasta")}, {12, QStringLiteral("dwunasta")},
    {13, QStringLiteral("trzynasta")}, {14, QStringLiteral("czternasta")}, {15, QStringLiteral("piętnasta")},
    {16, QStringLiteral("szesnasta")}, {17, QStringLiteral("siedemnasta")}, {18, QStringLiteral("osiemnasta")},
    {19, QStringLiteral("dziewiętnasta")}, {20, QStringLiteral("dwudziesta")},
    {21, QStringLiteral("dwudziesta pierwsza")}, {22, QStringLiteral("dwudziesta druga")},
    {23, QStringLiteral("dwudziesta trzecia")}, {24, QStringLiteral("dwudziesta czwarta")}};

const QMap<int, QString> HOUR_LOCATIVE = {
    {1, QStringLiteral("pierwszej")}, {2, QStringLiteral("drugiej")}, {3, QStringLiteral("trzeciej")},
    {4, QStringLiteral("czwartej")}, {5, QStringLiteral("piątej")}, {6, QStringLiteral("szóstej")},
    {7, QStringLiteral("siódmej")}, {8, QStringLiteral("ósmej")}, {9, QStringLiteral("dziewiątej")},
    {10, QStringLiteral("dziesiątej")}, {11, QStringLiteral("jedenastej")}, {12, QStringLiteral("dwunastej")},
    {13, QStringLiteral("trzynastej")}, {14, QStringLiteral("czternastej")}, {15, QStringLiteral("piętnastej")},
    {16, QStringLiteral("szesnastej")}, {17, QStringLiteral("siedemnastej")}, {18, QStringLiteral("osiemnastej")},
    {19, QStringLiteral("dziewiętnastej")}, {20, QStringLiteral("dwudziestej")},
    {21, QStringLiteral("dwudziestej pierwszej")}, {22, QStringLiteral("dwudziestej drugiej")},
    {23, QStringLiteral("dwudziestej trzeciej")}, {24, QStringLiteral("dwudziestej czwartej")}};

// --- Numbers -----------------------------------------------------------------

QString cardinalGroup(int value) {
    QStringList words;
    const int hundreds = value / 100;
    const int rest = value % 100;
    if (hundreds)
        words << HUNDREDS.value(hundreds);
    if (rest >= 20) {
        words << TENS.value(rest / 10);
        if (rest % 10)
            words << ONES.value(rest % 10);
    } else if (rest >= 10) {
        words << TEENS.value(rest - 10);
    } else if (rest > 0) {
        words << ONES.value(rest);
    }
    words.removeAll(QString());
    return words.join(QLatin1Char(' '));
}

QString scaleWord(long long count, int scale) {
    if (scale < 1 || scale > 3)
        return QString();
    const Scale &plural = SCHALE[scale];
    if (plural.one.isEmpty())
        return QString();
    const int last = int(count % 10);
    const int lastTwo = int(count % 100);
    if (count == 1)
        return plural.one;
    if (lastTwo >= 12 && lastTwo <= 14)
        return plural.many;
    if (last >= 2 && last <= 4)
        return plural.few;
    return plural.many;
}

QString cardinal(long long value) {
    long long number = std::llabs(value);
    if (number == 0)
        return ONES.value(0);
    QList<int> groups;
    while (number > 0) {
        groups.append(int(number % 1000));
        number /= 1000;
    }
    QStringList words;
    for (int index = groups.size() - 1; index >= 0; --index) {
        const int group = groups.at(index);
        if (!group)
            continue;
        if (index == 0) {
            words << cardinalGroup(group);
            continue;
        }
        if (group == 1 && index == 1) {
            words << QStringLiteral("tysiąc");
            continue;
        }
        words << (cardinalGroup(group) + QLatin1Char(' ') + scaleWord(group, index)).trimmed();
    }
    words.removeAll(QString());
    return words.join(QLatin1Char(' '));
}

QString cardinalFeminine(long long value) {
    QString text = cardinal(value);
    const long long number = std::llabs(value);
    const int last = int(number % 10);
    const int lastTwo = int(number % 100);
    if (lastTwo == 1 && text.endsWith(QStringLiteral("jeden")))
        text.chop(5), text += QStringLiteral("jedna");
    else if (last == 2 && !(lastTwo >= 12 && lastTwo <= 14) && text.endsWith(QStringLiteral("dwa")))
        text.chop(3), text += QStringLiteral("dwie");
    return text;
}

QString genitiveGroup(int value) {
    QStringList words;
    const int hundreds = value / 100;
    const int rest = value % 100;
    if (hundreds)
        words << GENITIVE_HUNDREDS.value(hundreds);
    if (rest >= 20) {
        words << GENITIVE_TENS.value(rest / 10);
        if (rest % 10)
            words << GENITIVE_ONES.value(rest % 10);
    } else if (rest >= 10) {
        words << GENITIVE_TEENS.value(rest - 10);
    } else if (rest > 0) {
        words << GENITIVE_ONES.value(rest);
    }
    words.removeAll(QString());
    return words.join(QLatin1Char(' '));
}

QString genitiveNumber(long long value) {
    long long number = std::llabs(value);
    if (number == 0)
        return ONES.value(0);
    QList<int> groups;
    while (number > 0) {
        groups.append(int(number % 1000));
        number /= 1000;
    }
    QStringList parts;
    for (int index = groups.size() - 1; index >= 0; --index) {
        const int group = groups.at(index);
        if (!group)
            continue;
        if (index == 0) {
            parts << genitiveGroup(group);
            continue;
        }
        if (index == 1 && group == 1) {
            parts << QStringLiteral("tysiąc");
            continue;
        }
        parts << (cardinalGroup(group) + QLatin1Char(' ') + scaleWord(group, index)).trimmed();
    }
    parts.removeAll(QString());
    return parts.join(QLatin1Char(' '));
}

QString pluralForm(long long count, const QString &one, const QString &few, const QString &many) {
    const int last = int(count % 10);
    const int lastTwo = int(count % 100);
    if (count == 1)
        return one;
    if (lastTwo >= 12 && lastTwo <= 14)
        return many;
    if (last >= 2 && last <= 4)
        return few;
    return many;
}

QString hourWord(int hour) {
    return HOUR_FEMININE.value(hour, cardinal(hour));
}

// "1234,56" -> "jeden tysiąc dwieście trzydzieści cztery przecinek pięćdziesiąt sześć".
QString decimalWords(const QString &raw) {
    QString cleaned;
    cleaned.reserve(raw.size());
    for (const QChar ch : raw) {
        if (ch.isSpace() || ch == QChar(0x00A0) || ch == QChar(0x202F))
            continue;
        cleaned.append(ch);
    }
    const QStringList parts = cleaned.split(QLatin1Char(','));
    const QString whole = cardinal(parts.value(0).toLongLong());
    if (parts.size() < 2)
        return whole;
    QString fractionPart = parts.value(1);
    while (fractionPart.endsWith(QLatin1Char('0')))
        fractionPart.chop(1);
    const long long fraction = fractionPart.isEmpty() ? 0 : fractionPart.toLongLong();
    return whole + QStringLiteral(" przecinek ") + cardinal(fraction);
}

// --- Replacement helper ------------------------------------------------------
// QString has no replace-with-callback, and every step here runs over the result
// of the previous one (exactly like the JS chain), so match iteration is local.
QString replaceAll(const QString &input, const QRegularExpression &re,
                   const std::function<QString(const QRegularExpressionMatch &)> &fn) {
    QString out;
    out.reserve(input.size());
    int last = 0;
    auto it = re.globalMatch(input);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += input.mid(last, match.capturedStart() - last);
        out += fn(match);
        last = match.capturedEnd();
    }
    out += input.mid(last);
    return out;
}

QString collapse(const QString &value) { return value.simplified(); }

QRegularExpression re(const QString &pattern, QRegularExpression::PatternOptions options = {}) {
    return QRegularExpression(pattern, options);
}

} // namespace

QString verbalizePolish(const QString &input) {
    QString text = input;
    if (text.trimmed().isEmpty())
        return QString();

    // Dates: 24.02.2024 / 24-02-2024 -> genitive day + month + genitive year.
    text = replaceAll(
        text, re(QStringLiteral("\\b(\\d{1,2})[.\\-/](\\d{1,2})[.\\-/](\\d{4})\\b")),
        [](const QRegularExpressionMatch &m) {
            const int day = m.captured(1).toInt();
            const int month = m.captured(2).toInt();
            if (day < 1 || day > 31 || month < 1 || month > 12)
                return m.captured(0);
            const QString dayText = GENITIVE_DAY.value(day, cardinal(day));
            const QString monthText = MONTHS_GENITIVE.value(month - 1);
            return dayText + QLatin1Char(' ') + monthText + QLatin1Char(' ')
                   + genitiveNumber(m.captured(3).toLongLong()) + QStringLiteral(" roku");
        });

    // Hours: 15:47 -> piętnasta czterdzieści siedem; after "o" -> locative.
    text = replaceAll(
        text, re(QStringLiteral("(\\b(?:o|około|przed|po|do|od)\\s+)?\\b([01]?\\d|2[0-3]):([0-5]\\d)\\b")),
        [](const QRegularExpressionMatch &m) {
            const QString prefix = m.captured(1);
            const int hour = m.captured(2).toInt();
            const int minute = m.captured(3).toInt();
            const QString hourText = prefix.isEmpty() ? hourWord(hour)
                                                      : HOUR_LOCATIVE.value(hour, hourWord(hour));
            const QString minutes = minute == 0 ? QString()
                                                : QLatin1Char(' ') + cardinal(minute);
            return prefix + hourText + minutes;
        });

    // Counted nouns: the number decides the form, so "3 szt." is not "trzy sztuk".
    text = replaceAll(
        text,
        re(QStringLiteral("(\\d{1,3}(?:[ \\x{00A0}\\x{202F}]\\d{3})*|\\d+)\\s*(szt|os)\\."),
           QRegularExpression::CaseInsensitiveOption),
        [](const QRegularExpressionMatch &m) {
            QString digits = m.captured(1);
            for (const QChar ch : QString(digits)) {
                Q_UNUSED(ch);
            }
            digits.remove(QRegularExpression(QStringLiteral("[\\x{00A0}\\x{202F}\\s]")));
            const long long count = digits.toLongLong();
            const QString noun = m.captured(2).toLower() == QStringLiteral("os")
                                     ? pluralForm(count, QStringLiteral("osoba"), QStringLiteral("osoby"),
                                                  QStringLiteral("osób"))
                                     : pluralForm(count, QStringLiteral("sztuka"), QStringLiteral("sztuki"),
                                                  QStringLiteral("sztuk"));
            return cardinalFeminine(count) + QLatin1Char(' ') + noun;
        });

    // Amounts: 1234,56 zł -> ... złotych ... groszy.
    text = replaceAll(
        text,
        re(QStringLiteral("(\\d{1,3}(?:[ \\x{00A0}\\x{202F}]\\d{3})*|\\d+)(?:,(\\d{1,2}))?\\s*(zł|PLN)(?![\\wąćęłńóśźż])"),
           QRegularExpression::CaseInsensitiveOption),
        [](const QRegularExpressionMatch &m) {
            QString digits = m.captured(1);
            digits.remove(QRegularExpression(QStringLiteral("[\\x{00A0}\\x{202F}\\s]")));
            const long long amount = digits.toLongLong();
            QString out = cardinal(amount) + QLatin1Char(' ')
                          + pluralForm(amount, QStringLiteral("złoty"), QStringLiteral("złote"),
                                       QStringLiteral("złotych"));
            const QString fraction = m.captured(2);
            if (fraction.isEmpty())
                return out;
            QString padded = fraction;
            while (padded.size() < 2)
                padded.append(QLatin1Char('0'));
            const long long grosze = padded.toLongLong();
            out += QLatin1Char(' ') + cardinal(grosze) + QLatin1Char(' ')
                   + pluralForm(grosze, QStringLiteral("grosz"), QStringLiteral("grosze"),
                                QStringLiteral("groszy"));
            return out;
        });

    // Percentages.
    text = replaceAll(text, re(QStringLiteral("(\\d+(?:,\\d+)?)\\s*%")),
                      [](const QRegularExpressionMatch &m) {
                          return decimalWords(m.captured(1)) + QStringLiteral(" procent");
                      });

    // Abbreviations before numbers, so "ok. 15" does not turn into "około" mid-token.
    struct Abbreviation {
        const char *pattern;
        const char *replacement;
    };
    static const Abbreviation ABBREVIATIONS[] = {
        {"\\bm\\.in\\.", "między innymi"},      {"\\bnp\\.", "na przykład"},
        {"\\bitd\\.", "i tak dalej"},           {"\\bitp\\.", "i tym podobne"},
        {"\\btzn\\.", "to znaczy"},             {"\\btj\\.", "to jest"},
        {"\\bprof\\.", "profesor"},             {"\\bdr hab\\.", "doktor habilitowany"},
        {"\\bdr\\.", "doktor"},                 {"\\bmgr\\.", "magister"},
        {"\\bgodz\\.", "godzina"},              {"\\bul\\.", "ulica"},
        {"\\bnr\\.?", "numer"},                 {"\\bpkt\\.?", "punkt"},
        {"\\btys\\.", "tysięcy"},               {"\\bmln\\.?", "milionów"},
        {"\\bmld\\.?", "miliardów"},            {"\\bkm\\b", "kilometrów"},
        {"\\bkg\\b", "kilogramów"},             {"\\bcm\\b", "centymetrów"},
        {"\\bmm\\b", "milimetrów"},             {"\\br\\.\\s*(?=\\d)", "roku "},
        {"\\br\\.(?=\\s|$)", "roku"},           {"\\bart\\.", "artykuł"},
        {"\\bstr\\.", "strona"},                {"\\bwzw\\.", "w związku z"},
        {"\\bwg\\b", "według"},
    };
    for (const Abbreviation &entry : ABBREVIATIONS) {
        const QString replacement = QString::fromUtf8(entry.replacement);
        text.replace(re(QString::fromUtf8(entry.pattern), QRegularExpression::CaseInsensitiveOption),
                     replacement);
    }

    // Decimals and thousands-separated numbers.
    text = replaceAll(
        text, re(QStringLiteral("\\b\\d{1,3}(?:[ \\x{00A0}\\x{202F}\\x{2009}]\\d{3})+(?:,\\d+)?\\b")),
        [](const QRegularExpressionMatch &m) { return decimalWords(m.captured(0)); });
    text = replaceAll(text, re(QStringLiteral("\\b\\d+,\\d+\\b")),
                      [](const QRegularExpressionMatch &m) { return decimalWords(m.captured(0)); });
    text = replaceAll(text, re(QStringLiteral("\\b\\d+\\b")),
                      [](const QRegularExpressionMatch &m) { return cardinal(m.captured(0).toLongLong()); });

    // Ranges and a few symbols that a voice engine would otherwise mispronounce.
    text.replace(re(QStringLiteral("\\s*&\\s*")), QStringLiteral(" i "));
    text = replaceAll(text, re(QStringLiteral("\\s+([,.!?;:])")),
                      [](const QRegularExpressionMatch &m) { return m.captured(1); });
    return collapse(text);
}

} // namespace audioticon
