#pragma once

#include <QString>
#include <QVariantList>
#include <QVector>

namespace audioticon {

// The script grammar: a handful of emoji are stage directions (pauses, tempo,
// acoustic plans, chapters), everything else is spoken. Ported from the donor
// module so the audio matches what Portable Local Studio renders.
struct VoiceSegment {
    QString text;
    double speed = 1.0;
    QString dsp;       // radio | phone | cathedral | whisper, empty = none
    int pauseMs = 0;   // pause after this segment
};

struct Chapter {
    int index = 0;     // segment index the chapter starts at
    QString title;
};

struct DirectedText {
    QVector<VoiceSegment> segments;
    QVector<Chapter> chapters;
    int leadingPauseMs = 0;
    QString text;              // whole text, directives stripped and verbalized
    QVariantList directives;   // [{kind, pauseMs, dsp}], in script order
};

// The palette, deduplicated: {token, kind, pauseMs, dsp, label}.
QVariantList describeDirectives();

bool hasDirectives(const QString &text);
QString stripDirectives(const QString &text);

// A chunk with no letters or digits ("...", "—", "🙂") has no phonemes; sending
// it to the engine produced an empty WAV and a hard failure, so it is dropped.
bool hasSpeakableText(const QString &text);

QString cleanupChunk(const QString &value, bool polish = true);
QString preparePlainText(const QString &value, bool polish = true);

DirectedText parseDirectedText(const QString &raw, bool polish = true);

double round2(double value);

} // namespace audioticon
