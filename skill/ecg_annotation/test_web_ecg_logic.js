"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const htmlPath = path.join(__dirname, "web", "ecg_rpeak_label_trainer.html");
const source = fs.readFileSync(htmlPath, "utf8");

function extractFunction(name) {
  const marker = `function ${name}(`;
  const start = source.indexOf(marker);
  assert(start >= 0, `Missing function ${name}`);

  const bodyStart = source.indexOf("{", start);
  let depth = 0;
  for (let i = bodyStart; i < source.length; i++) {
    if (source[i] === "{") depth++;
    if (source[i] === "}") depth--;
    if (depth === 0) return source.slice(start, i + 1);
  }
  throw new Error(`Unterminated function ${name}`);
}

const sandbox = { ECG_QRS_MWI_LEN: 54 };
vm.createContext(sandbox);
vm.runInContext(
  [
    extractFunction("median3"),
    extractFunction("runEmbeddedFilter"),
    extractFunction("runSqi"),
    "this.runEmbeddedFilter = runEmbeddedFilter;",
    "this.runSqi = runSqi;"
  ].join("\n"),
  sandbox
);

function makeParams(overrides = {}) {
  return {
    clipPct: 5,
    meanDiffMax: 0.10,
    spikeDiff: 0.20,
    spikePct: 20,
    sqiUseOtr: false,
    hardClipRun: 8,
    constantRun: 72,
    headroomPct: 25,
    meanAdcMin: 32,
    meanAdcMax: 223,
    ...overrides
  };
}

function makeCleanData(n = 720) {
  const raw = Array.from({ length: n }, (_, i) => 0.10 * Math.sin(i * 0.10));
  return {
    fs: 360,
    raw,
    rawAdc: raw.map(x => Math.round(128 + 128 * x)),
    lod: new Array(n).fill(0),
    otr: new Array(n).fill(0)
  };
}

{
  const data = makeCleanData(4000);
  for (let i = 1400; i < 2200; i++) {
    data.rawAdc[i] = 228;
    data.raw[i] = (228 - 128) / 128;
  }
  const result = sandbox.runSqi(data, makeParams());
  assert.strictEqual(result.states[1470], 'GOOD');
  for (let i = 1471; i < 2200; i++) assert.strictEqual(result.states[i], 'BAD');
  assert.strictEqual(result.states[3999], 'GOOD');
}

{
  const out = sandbox.runEmbeddedFilter([0, 1, 1]);
  assert.strictEqual(out.filt[0], 0, "First display sample must pass through");
  assert(Math.abs(out.filt[1]) < 1e-12, "Median-of-3 must reject the isolated step");
  assert(Math.abs(out.filt[2] - 0.2) < 1e-12, "EMA alpha must match board value 0.2");
}

{
  const data = makeCleanData();
  const result = sandbox.runSqi(data, makeParams());
  assert.strictEqual(result.states[719], "GOOD", "Clean first window should become GOOD");
  assert.strictEqual(result.warnings[719], 0, "Centered clean data should have full headroom");
}

{
  const data = makeCleanData();
  data.otr.fill(1);
  const ignored = sandbox.runSqi(data, makeParams({ sqiUseOtr: false }));
  const enabled = sandbox.runSqi(data, makeParams({ sqiUseOtr: true }));
  assert.strictEqual(ignored.states[719], "GOOD", "Disabled OTR must not gate SQI");
  assert.strictEqual(enabled.states[719], "BAD", "Enabled OTR must gate SQI");
  assert(enabled.reasons[719] & 0x02, "Enabled OTR must set the OTR reason bit");
}

{
  const data = makeCleanData();
  for (let i = 0; i < 8; i++) {
    data.rawAdc[i] = 255;
    data.raw[i] = 127 / 128;
  }
  const result = sandbox.runSqi(data, makeParams());
  assert.strictEqual(result.states[7], "BAD", "Eight clipped samples must gate immediately");
  assert(result.reasons[7] & 0x04, "Hard clipping must set the clipping reason bit");
}

{
  const data = makeCleanData();
  data.rawAdc = data.rawAdc.map(x => 230 + Math.round((x - 128) * 0.2));
  data.raw = data.rawAdc.map(x => (x - 128) / 128);
  const result = sandbox.runSqi(data, makeParams());
  assert.strictEqual(result.states[719], "GOOD", "Low headroom alone must not gate processing");
  assert(result.warnings[719] & 0x01, "Low headroom must raise a separate warning");
}

console.log("ECG web logic tests passed");
