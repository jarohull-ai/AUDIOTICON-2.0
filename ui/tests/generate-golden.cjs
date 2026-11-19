#!/usr/bin/env node
"use strict";

// Generates tests/data/tts-golden.json from the donor module.
//
// The C++ port of the parser and the Polish verbalizer must agree with Portable
// Local Studio exactly — a different word or a dropped pause is an audible
// regression. This script is the only place that knows about the donor's path;
// the ported code and the tests never do.
//
//   DONOR_DIR=~/Portable-Local-Studio node tests/generate-golden.cjs

const fs = require("fs");
const path = require("path");
const os = require("os");

const donorDir = process.env.DONOR_DIR || path.join(os.homedir(), "Portable-Local-Studio");
const modulePath = path.join(donorDir, "scripts", "server", "tts-directives.cjs");
if (!fs.existsSync(modulePath)) {
  console.error(`Donor module not found: ${modulePath}\nSet DONOR_DIR to the Portable-Local-Studio checkout.`);
  process.exit(1);
}

const d = require(modulePath);

const INPUTS = [
  ["plain", "Dzień dobry."],
  ["date", "Dzień dobry, 24.02.2024."],
  ["dashes", "Tekst ⏸️ — ⏸️ dalej"],
  ["pause-no-vs16", "Tekst ⏸ dalej"],
  ["dots", "..."],
  ["emoji-only", "🙂"],
  ["turtle-chapter", "🐢 Dzień dobry 📖 dalej"],
  ["tempo", "🐇 szybciej 🐢 🐢 wolniej 🎙️ reset"],
  ["dsp-plans", "📻 radio 📱 telefon 🏛️ sala 🏛 katedra 🤫 szept"],
  ["breath", "😮‍💨 oddech"],
  ["chapters", "Rozdział 📖 pierwszy 📖 drugi"],
  ["chapter-only", "📖"],
  ["amounts", "1 234,56 zł"],
  ["hour", "15:47"],
  ["hour-locative", "o 15:47"],
  ["hour-around", "około 7:05"],
  ["pieces-3", "3 szt."],
  ["pieces-22", "22 szt."],
  ["people", "5 os."],
  ["percent", "12%"],
  ["percent-decimal", "1,5%"],
  ["abbrev-np", "np. to jest test"],
  ["abbrev-title", "m.in. prof. dr hab. Kowalski"],
  ["street", "ul. Marszałkowska 1"],
  ["year", "w 2024 r. było 100 zł"],
  ["millions", "2 000 000 osób"],
  ["zero", "0"],
  ["million", "1000000"],
  ["twenty-one", "21"],
  ["hundred-one", "101"],
  ["thousand", "1000"],
  ["two-thousand", "2000"],
  ["sentence", "Dzień dobry! Jak się masz?"],
  ["emoji-inside", "emoji 😀 w środku"],
  ["emoji-run", "😀😀😀"],
  ["pause-only", "⏸️"],
  ["turtle-only", "🐢"],
  ["ampersand", "A & B"],
  ["mixed-money", "Cena 1 299,99 zł i 7 szt."],
  ["hour-exact", "Zadzwonił o 12:00."],
  ["cells", "12 kg, 40 km, 3 cm i 5 mm"],
  ["dr", "dr Nowak i mgr Kowalski"],
  ["speed-reset", "🐢🐢 tekst 🎙️"],
  ["braces", "wartość 0,5 oraz 2,25"],
];

const cases = INPUTS.map(([name, input]) => {
  const parsed = d.parseDirectedText(input);
  return {
    name,
    input,
    verbalized: d.verbalizePolish(input),
    speakable: d.hasSpeakableText(input),
    plain: d.preparePlainText(input),
    text: parsed.text,
    leadingPauseMs: parsed.leadingPauseMs,
    segments: parsed.segments.map((s) => ({
      text: s.text,
      speed: s.speed,
      dsp: s.dsp || "",
      pauseMs: s.pauseMs,
    })),
    chapters: parsed.chapters,
    directives: parsed.directives.map((x) => ({
      kind: x.kind,
      pauseMs: x.pauseMs || 0,
      dsp: x.dsp || "",
    })),
  };
});

const palette = d.describeDirectives().map((entry) => ({
  token: entry.token,
  kind: entry.kind,
  pauseMs: entry.pauseMs || 0,
  dsp: entry.dsp || "",
  label: entry.label,
}));

const out = { generatedFrom: "Portable-Local-Studio/scripts/server/tts-directives.cjs", cases, palette };
const target = path.join(__dirname, "data", "tts-golden.json");
fs.mkdirSync(path.dirname(target), { recursive: true });
fs.writeFileSync(target, JSON.stringify(out, null, 2) + "\n");
console.log(`wrote ${cases.length} cases + ${palette.length} palette entries to ${target}`);
