#pragma once

#include <QString>

namespace audioticon {

// Turns the things a lektor meets — numbers, decimals, thousands, dates, hours,
// amounts, percentages and a short list of abbreviations — into words.
//
// This is a straight port of `verbalizePolish()` from the donor module
// (Portable-Local-Studio, scripts/server/tts-directives.cjs). Cardinal forms are
// nominative, dates and years use the genitive ("dwudziestego czwartego").
QString verbalizePolish(const QString &input);

} // namespace audioticon
