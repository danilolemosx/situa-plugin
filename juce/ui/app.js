// Situa: janela do plugin. Cada controle é ligado a um parâmetro pelo JUCE (juce.js).
// Aberta num navegador comum (sem o plugin), funciona como demonstração.
// Script comum (não módulo): o WebKit do macOS não carrega módulos pelo endereço do plugin.

// ---------------------------------------------------------------------------------------------
// parâmetros: as mesmas faixas de PluginProcessor.cpp (log = faixaLog)

const PARAMS = {
  lowCut:   { de: 20,   ate: 1000,  log: true, padrao: 20,    m: "device" },
  highCut:  { de: 1000, ate: 20000, log: true, padrao: 20000, m: "device" },
  drive:    { de: 0,    ate: 100,   padrao: 0,   m: "device" },
  muffle:   { de: 0,    ate: 100,   padrao: 60,  m: "barrier" },
  leak:     { de: 0,    ate: 100,   padrao: 20,  m: "barrier" },
  size:     { de: 1.5,  ate: 80,    log: true, padrao: 6,   m: "space" },
  decay:    { de: 0.05, ate: 8,     log: true, padrao: 0.6, m: "space" },
  distance: { de: 0.3,  ate: 50,    log: true, padrao: 2,   m: "space" },
  mix:      { de: 0,    ate: 100,   padrao: 100 },
  output:   { de: -24,  ate: 12,    padrao: 0, bipolar: true },
  // definidos pelos presets (sem knob)
  lf:       { de: 0.5,  ate: 2,     padrao: 1 },
  hf:       { de: 0.2,  ate: 1,     padrao: 0.6 },
  peakFreq: { de: 100,  ate: 6000,  log: true, padrao: 1000 },
  peakGain: { de: -12,  ate: 12,    padrao: 0 },
  peakQ:    { de: 0.3,  ate: 4,     padrao: 1 },
};
const CHAVES = ["device", "space", "barrier", "airPlus", "mono", "bypass", "comp", "cabin"];
const CODECS = ["OFF", "LANDLINE", "2G", "3G", "HD", "VOIP"];

const hz = (f) => (f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 0 : 1) + "k" : Math.round(f));
const FORMATO = {
  lowCut:   (v) => (v <= 21 ? "OFF" : hz(v) + " Hz"),
  highCut:  (v) => (v >= 19800 ? "OFF" : hz(v) + " Hz"),
  drive:    (v) => Math.round(v) + "%",
  muffle:   (v) => Math.round(v) + "%",
  leak:     (v) => Math.round(v) + "%",
  size:     (v) => (v >= 60 ? "OPEN" : (v < 10 ? v.toFixed(1) : Math.round(v)) + " m"),
  decay:    (v) => v.toFixed(v < 1 ? 2 : 1) + " s",
  distance: (v) => (v < 10 ? v.toFixed(1) : Math.round(v)) + " m",
  mix:      (v) => Math.round(v) + "%",
  output:   (v) => (v > 0.05 ? "+" : v < -0.05 ? "−" : "") + Math.abs(v).toFixed(1) + " dB",
};
const paraN = (p, v) => Math.min(1, Math.max(0, p.log ? Math.log(v / p.de) / Math.log(p.ate / p.de) : (v - p.de) / (p.ate - p.de)));
const deN = (p, n) => (p.log ? p.de * Math.pow(p.ate / p.de, n) : p.de + n * (p.ate - p.de));

// ---------------------------------------------------------------------------------------------
// presets: os mesmos do protótipo (CLAUDE AVID/SITUA/PROTOTIPO/scripts/situa.py)

const PRESETS = [
  ["SPACE", [
    ["Small Room", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 4, "decay": 0.35, "distance": 1.5, "lf": 1.0, "hf": 0.7, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Living Room", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 6, "decay": 0.5, "distance": 2.5, "lf": 1.1, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Large Hall", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 30, "decay": 2.0, "distance": 8, "lf": 1.3, "hf": 0.5, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Church", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 45, "decay": 4.5, "distance": 10, "lf": 1.5, "hf": 0.45, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Bathroom", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 2.8, "decay": 1.0, "distance": 1.2, "lf": 1.0, "hf": 0.9, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Stairwell", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 10, "decay": 2.2, "distance": 4, "lf": 1.0, "hf": 0.7, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Car Interior", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": true, "size": 1.8, "decay": 0.08, "distance": 0.7, "lf": 1.2, "hf": 0.5, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Warehouse", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 50, "decay": 3.0, "distance": 8, "lf": 1.4, "hf": 0.5, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}]
  ]],
  ["DISTANCE", [
    ["Across the Room", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 8, "decay": 0.6, "distance": 5, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Far Away · Indoor", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 20, "decay": 1.2, "distance": 15, "lf": 1.1, "hf": 0.55, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": -3, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Far Away · Street", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 80, "decay": 0.6, "distance": 30, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": -4, "device": false, "space": true, "barrier": false, "mono": false}],
    ["Shouting Distance", {"lowCut": 20, "highCut": 20000, "drive": 8, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 80, "decay": 0.8, "distance": 40, "lf": 1.0, "hf": 0.6, "airPlus": true, "muffle": 60, "leak": 20, "mix": 100, "output": -6, "device": true, "space": true, "barrier": false, "mono": false}]
  ]],
  ["DEVICE", [
    ["Landline Phone", {"lowCut": 300, "highCut": 3400, "drive": 20, "codec": 1, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": true, "cabin": false, "size": 6, "decay": 0.6, "distance": 2, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": false, "barrier": false, "mono": true}],
    ["Cell Phone", {"lowCut": 300, "highCut": 3400, "drive": 10, "codec": 3, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": true, "cabin": false, "size": 6, "decay": 0.6, "distance": 2, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": false, "barrier": false, "mono": true}],
    ["Cell Phone · HD", {"lowCut": 80, "highCut": 7000, "drive": 5, "codec": 4, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": true, "cabin": false, "size": 6, "decay": 0.6, "distance": 2, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": false, "barrier": false, "mono": true}],
    ["Video Call", {"lowCut": 100, "highCut": 7500, "drive": 0, "codec": 5, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": true, "cabin": false, "size": 4, "decay": 0.4, "distance": 0.8, "lf": 1.0, "hf": 0.7, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": true, "barrier": false, "mono": true}],
    ["Speakerphone", {"lowCut": 350, "highCut": 3400, "drive": 30, "codec": 3, "peakFreq": 900, "peakGain": 4, "peakQ": 1.2, "comp": true, "cabin": false, "size": 4, "decay": 0.4, "distance": 1.5, "lf": 1.0, "hf": 0.7, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": true, "barrier": false, "mono": true}],
    ["Walkie-Talkie", {"lowCut": 400, "highCut": 3000, "drive": 60, "codec": 0, "peakFreq": 1500, "peakGain": 5, "peakQ": 1.0, "comp": true, "cabin": false, "size": 6, "decay": 0.6, "distance": 2, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": false, "barrier": false, "mono": true}],
    ["AM Radio", {"lowCut": 150, "highCut": 4500, "drive": 20, "codec": 0, "peakFreq": 1000, "peakGain": 2, "peakQ": 0.8, "comp": true, "cabin": false, "size": 6, "decay": 0.6, "distance": 2, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": false, "barrier": false, "mono": false}],
    ["TV in Next Room", {"lowCut": 120, "highCut": 8000, "drive": 10, "codec": 0, "peakFreq": 200, "peakGain": 3, "peakQ": 1.0, "comp": false, "cabin": false, "size": 5, "decay": 0.5, "distance": 2.5, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 45, "leak": 25, "mix": 100, "output": 0, "device": true, "space": true, "barrier": true, "mono": false}],
    ["PA Speaker", {"lowCut": 300, "highCut": 5000, "drive": 25, "codec": 0, "peakFreq": 2500, "peakGain": 4, "peakQ": 1.0, "comp": true, "cabin": false, "size": 50, "decay": 2.5, "distance": 15, "lf": 1.3, "hf": 0.5, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": true, "barrier": false, "mono": false}],
    ["Megaphone", {"lowCut": 500, "highCut": 4000, "drive": 60, "codec": 0, "peakFreq": 1800, "peakGain": 6, "peakQ": 1.2, "comp": true, "cabin": false, "size": 80, "decay": 0.5, "distance": 8, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 60, "leak": 20, "mix": 100, "output": 0, "device": true, "space": true, "barrier": false, "mono": false}]
  ]],
  ["BARRIER", [
    ["Through Wall", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 5, "decay": 0.5, "distance": 2, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 70, "leak": 5, "mix": 100, "output": 0, "device": false, "space": true, "barrier": true, "mono": false}],
    ["Through Door", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 5, "decay": 0.5, "distance": 2.5, "lf": 1.0, "hf": 0.6, "airPlus": false, "muffle": 45, "leak": 35, "mix": 100, "output": 0, "device": false, "space": true, "barrier": true, "mono": false}],
    ["Next Apartment", {"lowCut": 60, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 6, "decay": 0.55, "distance": 2.5, "lf": 1.1, "hf": 0.6, "airPlus": false, "muffle": 90, "leak": 0, "mix": 100, "output": 0, "device": true, "space": true, "barrier": true, "mono": false}],
    ["Underwater", {"lowCut": 20, "highCut": 20000, "drive": 0, "codec": 0, "peakFreq": 1000, "peakGain": 0, "peakQ": 1, "comp": false, "cabin": false, "size": 3, "decay": 1.5, "distance": 1, "lf": 1.0, "hf": 0.3, "airPlus": false, "muffle": 95, "leak": 0, "mix": 100, "output": 6, "device": false, "space": true, "barrier": true, "mono": false}]
  ]]
];
// a ordem do parâmetro "preset" (PluginProcessor.cpp): 0 = INIT, depois a lista acima
const NOMES = ["INIT"].concat(PRESETS.flatMap(([, itens]) => itens.map(([n]) => n)));
const VALORES = Object.fromEntries(PRESETS.flatMap(([, itens]) => itens));

// ---------------------------------------------------------------------------------------------
// estado dos parâmetros: o do JUCE, ou um de demonstração

function ouvintes() {
  const l = new Set();
  return { addListener: (f) => l.add(f), callListeners: () => l.forEach((f) => f()) };
}
function sliderDemo(nome) {
  const s = {
    scaledValue: PARAMS[nome].padrao, valueChangedEvent: ouvintes(),
    getScaledValue: () => s.scaledValue,
    sliderDragStarted: () => {}, sliderDragEnded: () => {},
  };
  return s;
}
function chaveDemo() {
  const c = { v: false, valueChangedEvent: ouvintes(), getValue: () => c.v, setValue: (v) => { c.v = v; c.valueChangedEvent.callListeners(); } };
  return c;
}
function comboDemo(n) {
  const c = {
    value: 0, properties: { choices: new Array(n).fill("") }, valueChangedEvent: ouvintes(),
    getChoiceIndex: () => Math.round(c.value * (n - 1)),
    setChoiceIndex: (i) => { c.value = i / (n - 1); c.valueChangedEvent.callListeners(); },
  };
  return c;
}

const juce = window.__JUCE__ && window.Juce ? window.Juce : null;
const sliders = {}, chaves = {};
for (const nome of Object.keys(PARAMS)) sliders[nome] = juce ? juce.getSliderState(nome) : sliderDemo(nome);
for (const nome of CHAVES) chaves[nome] = juce ? juce.getToggleState(nome) : chaveDemo();
const combos = {
  codec: juce ? juce.getComboBoxState("codec") : comboDemo(CODECS.length),
  preset: juce ? juce.getComboBoxState("preset") : comboDemo(NOMES.length),
};
const nativo = (nome) => (juce ? juce.getNativeFunction(nome) : null);

// os valores vão e vêm em unidades reais (Hz, m, s…): o plugin converte com a faixa logarítmica
// dele (o setNormalisedValue do juce.js só conhece faixas lineares com skew)
const valor = (nome) => sliders[nome].getScaledValue();
function enviar(nome, v) {
  const s = sliders[nome], p = PARAMS[nome];
  v = Math.min(p.ate, Math.max(p.de, v));
  s.scaledValue = v;
  if (juce) window.__JUCE__.backend.emitEvent(s.identifier, { eventType: "valueChanged", value: v });
  else s.valueChangedEvent.callListeners();
}
const ligado = (nome) => !!chaves[nome].getValue();
const indice = (combo, n) => Math.min(n - 1, Math.max(0, Math.round(combo.value * (n - 1))));
function definir(nome, v) {
  const s = sliders[nome];
  s.sliderDragStarted();
  enviar(nome, v);
  s.sliderDragEnded();
}
function escolher(combo, i, n) {
  if (!juce || combo.properties.choices.length === n) combo.setChoiceIndex(i);
  else {
    // as opções ainda não chegaram do plugin: manda o valor normalizado direto
    combo.value = i / (n - 1);
    window.__JUCE__.backend.emitEvent(combo.identifier, { eventType: "valueChanged", value: combo.value });
    combo.valueChangedEvent.callListeners();
  }
}

// retrato do estado atual (o que os desenhos usam)
function ler() {
  const e = {};
  for (const k of Object.keys(PARAMS)) e[k] = valor(k);
  for (const k of CHAVES) e[k] = ligado(k);
  e.codec = indice(combos.codec, CODECS.length);
  e.preset = indice(combos.preset, NOMES.length);
  return e;
}

function carregar(nome) {
  const v = VALORES[nome];
  if (!v) return;
  for (const k of Object.keys(PARAMS)) definir(k, k in v ? v[k] : PARAMS[k].padrao);
  for (const k of CHAVES) if (k !== "bypass") chaves[k].setValue(!!v[k]);
  escolher(combos.codec, v.codec || 0, CODECS.length);
  escolher(combos.preset, NOMES.indexOf(nome), NOMES.length);
}

// o preset foi mexido depois de escolhido?
function editado(e) {
  const v = VALORES[NOMES[e.preset]];
  if (!v) return false;
  for (const k of Object.keys(PARAMS)) {
    const p = PARAMS[k], alvo = k in v ? v[k] : p.padrao;
    if (Math.abs(paraN(p, e[k]) - paraN(p, alvo)) > 0.004) return true;
  }
  for (const k of CHAVES) if (k !== "bypass" && e[k] !== !!v[k]) return true;
  return e.codec !== (v.codec || 0);
}

// ---------------------------------------------------------------------------------------------
// lista de presets (com busca)

const lista = document.getElementById("lista");
for (const [cat, itens] of PRESETS) {
  const h = document.createElement("div"); h.className = "cat"; h.textContent = cat; lista.append(h);
  for (const [nome] of itens) {
    const b = document.createElement("button");
    b.className = "preset"; b.textContent = nome; b.dataset.nome = nome; b.dataset.cat = cat;
    b.onclick = () => carregar(nome);
    lista.append(b);
  }
}
document.getElementById("busca").addEventListener("input", (ev) => {
  const q = ev.target.value.trim().toLowerCase();
  for (const b of lista.querySelectorAll(".preset"))
    b.style.display = !q || b.dataset.nome.toLowerCase().includes(q) || b.dataset.cat.toLowerCase().includes(q) ? "" : "none";
  for (const h of lista.querySelectorAll(".cat")) {
    let el = h.nextElementSibling, algum = false;
    while (el && el.classList.contains("preset")) { if (el.style.display !== "none") algum = true; el = el.nextElementSibling; }
    h.style.display = algum ? "" : "none";
  }
});

// ---------------------------------------------------------------------------------------------
// botões giratórios: 270° de curso, arrastar para cima/baixo (shift = fino), ⌥+clique = padrão

function ponto(r, graus) {
  const a = ((graus - 90) * Math.PI) / 180;
  return [50 + r * Math.cos(a), 50 + r * Math.sin(a)];
}
function arco(r, de, ate) {
  const [x1, y1] = ponto(r, de), [x2, y2] = ponto(r, ate);
  return `M${x1.toFixed(2)} ${y1.toFixed(2)} A${r} ${r} 0 ${ate - de > 180 ? 1 : 0} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
}
function desenharKnob(el, e) {
  const nome = el.dataset.param, p = PARAMS[nome];
  const v = paraN(p, e[nome]);
  const ang = -135 + 270 * v;
  const [px, py] = ponto(19, ang);
  const deAng = p.bipolar ? -135 + 270 * paraN(p, 0) : -135;
  const [a0, a1] = ang >= deAng ? [deAng, ang] : [ang, deAng];
  el.querySelector("svg").innerHTML =
    `<path d="${arco(40, -135, 135)}" fill="none" stroke="#3a3835" stroke-width="6" stroke-linecap="round"/>` +
    (a1 - a0 > 0.5 ? `<path d="${arco(40, a0, a1)}" fill="none" stroke="${el.dataset.cor}" stroke-width="6" stroke-linecap="round"/>` : "") +
    `<circle cx="50" cy="50" r="28" fill="#34322f" stroke="#4a4742" stroke-width="1.5"/>` +
    `<line x1="50" y1="50" x2="${px.toFixed(2)}" y2="${py.toFixed(2)}" stroke="#efe5cf" stroke-width="4" stroke-linecap="round"/>`;
  el.querySelector(".v").textContent = FORMATO[nome](e[nome]);
  el.classList.toggle("inativo", !!p.m && !e[p.m]);
}
const knobs = [...document.querySelectorAll(".knob")];
for (const el of knobs) {
  const nome = el.dataset.param, p = PARAMS[nome], svg = el.querySelector("svg");
  svg.setAttribute("viewBox", "0 0 100 100");
  svg.setAttribute("width", 46); svg.setAttribute("height", 46);
  svg.addEventListener("pointerdown", (ev) => {
    if (p.m && !ligado(p.m)) chaves[p.m].setValue(true);   // mexer no knob liga a etapa
    if (ev.altKey) { definir(nome, p.padrao); return; }
    svg.setPointerCapture(ev.pointerId);
    const s = sliders[nome], y0 = ev.clientY, n0 = paraN(p, valor(nome));
    s.sliderDragStarted();
    const mover = (e2) => enviar(nome, deN(p, Math.min(1, Math.max(0, n0 + ((y0 - e2.clientY) / 160) * (e2.shiftKey ? 0.2 : 1)))));
    const soltar = () => {
      svg.removeEventListener("pointermove", mover);
      svg.removeEventListener("pointerup", soltar);
      svg.removeEventListener("pointercancel", soltar);
      s.sliderDragEnded();
    };
    svg.addEventListener("pointermove", mover);
    svg.addEventListener("pointerup", soltar);
    svg.addEventListener("pointercancel", soltar);
  });
}

// teclas
for (const b of document.querySelectorAll("#cadeia .tecla"))
  b.onclick = () => chaves[b.dataset.m].setValue(!ligado(b.dataset.m));
for (const id of ["mono", "bypass", "airPlus"])
  document.getElementById(id).onclick = () => chaves[id].setValue(!ligado(id));
// CODEC: cada clique passa para o próximo tipo (e liga o DEVICE)
document.getElementById("codec").onclick = () => {
  const i = (indice(combos.codec, CODECS.length) + 1) % CODECS.length;
  escolher(combos.codec, i, CODECS.length);
  if (i > 0 && !ligado("device")) chaves.device.setValue(true);
};

// ---------------------------------------------------------------------------------------------
// resposta de frequência aproximada da cadeia (só para o desenho; a conta de verdade está em Motor.cpp)

// absorção do ar, ISO 9613-1 (20 °C, 50% UR), em dB/m
function arIso(f) {
  const T = 293.15, h = 1.1645, fO = 24 + 4.04e4 * h * (0.02 + h) / (0.391 + h), fN = 9 + 280 * h;
  return 8.686 * f * f * (1.84e-11 + 0.01275 * Math.exp(-2239.1 / T) / (fO + f * f / fO) + 0.1068 * Math.exp(-3352 / T) / (fN + f * f / fN));
}
// parede pela lei da massa: 6 dB/oitava (leve) até 12 dB/oitava (dupla), canto de 1,5 kHz a 100 Hz
const cantoParede = (m) => 1500 * Math.pow(100 / 1500, m);

function resposta(s, f) {
  let db = 0;
  if (s.device) {
    if (s.lowCut > 21) db -= 10 * Math.log10(1 + Math.pow(s.lowCut / f, 8));
    if (s.highCut < 19800) db -= 10 * Math.log10(1 + Math.pow(f / s.highCut, 8));
    if (Math.abs(s.peakGain) > 0.05) db += s.peakGain * Math.exp(-Math.pow(Math.log2(f / s.peakFreq) * s.peakQ * 1.4, 2));
    if (s.codec === 1) db -= 10 * Math.log10(1 + Math.pow(f / 3600, 16));
    if (s.codec === 2 || s.codec === 3) db -= 10 * Math.log10(1 + Math.pow(f / 4000, 16));
    if (s.codec >= 4) db -= 10 * Math.log10(1 + Math.pow(f / 7500, 16));
  }
  if (s.space) {
    const d = s.distance;
    db -= arIso(f) * d * (s.airPlus ? 4 : 1);
    db -= Math.min(1, d / 6) * 4 * (150 * 150 / (f * f + 150 * 150));
    if (s.cabin) db += 4 * (120 * 120 / (f * f + 120 * 120));
    if (s.airPlus) db -= 6 * (f * f / (f * f + 3000 * 3000));
  }
  if (s.barrier) {
    const m = s.muffle / 100, fc = cantoParede(m), ordem = 1 + m;
    const parede = Math.pow(10, -ordem * 10 * Math.log10(1 + Math.pow(f / fc, 2)) / 20);
    const fresta = Math.pow(10, (-40 + 28 * s.leak / 100 + 4 + 10 * m) / 20) / Math.sqrt(1 + Math.pow(1000 / f, 2));
    db += 20 * Math.log10(parede + fresta);
  }
  return db + s.output;
}

function desenharCurva(s) {
  const svg = document.getElementById("curva"), W = 300, H = 150, top = 12, bot = -42;
  const xF = (f) => (Math.log(f / 20) / Math.log(1000)) * W;
  const yD = (d) => ((top - Math.max(bot, Math.min(top, d))) / (top - bot)) * H;
  let g = "";
  for (const f of [100, 1000, 10000]) g += `<line x1="${xF(f)}" y1="0" x2="${xF(f)}" y2="${H}" stroke="#2f2d2a"/>`;
  for (const d of [0, -12, -24, -36]) g += `<line x1="0" y1="${yD(d)}" x2="${W}" y2="${yD(d)}" stroke="${d === 0 ? "#3d3a36" : "#2f2d2a"}"/>`;
  g += `<rect x="${xF(100)}" y="0" width="${xF(8000) - xF(100)}" height="${H}" fill="#efe5cf" opacity=".025"/>`;
  const pts = [];
  for (let i = 0; i <= 120; i++) { const f = 20 * Math.pow(1000, i / 120); pts.push([xF(f), yD(s.bypass ? 0 : resposta(s, f))]); }
  const linha = pts.map((p, i) => (i ? "L" : "M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join("");
  g += `<path d="${linha}L${W} ${H}L0 ${H}Z" fill="#d2382f" opacity=".12"/>`;
  g += `<path d="${linha}" fill="none" stroke="${s.bypass ? "#6b665e" : "#d2382f"}" stroke-width="2" vector-effect="non-scaling-stroke"/>`;
  svg.innerHTML = g;
  let b = "FULL RANGE";
  if (s.device && (s.lowCut > 21 || s.highCut < 19800))
    b = (s.lowCut > 21 ? hz(s.lowCut) : "20") + "–" + (s.highCut < 19800 ? hz(s.highCut) : "20k") + " Hz";
  else if (s.barrier) b = "WALL ABOVE " + hz(cantoParede(s.muffle / 100)) + " Hz";
  document.getElementById("banda").textContent = b;
}

// ---------------------------------------------------------------------------------------------
// cena vista de cima: sala, voz, ouvinte, primeiras reflexões e parede

function desenharCena(s) {
  const svg = document.getElementById("cena");
  const W = 300, H = 150, cx = W / 2;
  const aberto = s.space && s.size >= 60;
  const lado = s.space ? Math.min(s.size, 40) : 6;
  const larg = 240, alt = s.barrier ? 100 : 128;
  const sala = { x: cx - larg / 2, y: s.barrier ? 6 : (H - alt) / 2, w: larg, h: alt };
  const ouv = s.barrier ? [cx, H - 14] : [cx, sala.y + alt - 16];
  const dist = s.space ? s.distance : 2;
  const px = Math.min(alt - 34, (dist / lado) * (alt - 34) + 12);
  const voz = s.barrier ? [cx + Math.min(60, px * 0.25), sala.y + alt - 10 - px] : [cx + Math.min(60, px * 0.25), ouv[1] - px];
  let g = "";
  if (!aberto)
    g += `<rect x="${sala.x}" y="${sala.y}" width="${sala.w}" height="${sala.h}" rx="3" fill="#262523" stroke="${s.space ? "#8fd9a8" : "#47443f"}" stroke-width="1.5" opacity="${s.space ? 1 : .6}"/>`;
  else
    g += `<line x1="${sala.x}" y1="${sala.y + alt}" x2="${sala.x + larg}" y2="${sala.y + alt}" stroke="#8fd9a8" stroke-dasharray="3 4"/>`;
  if (s.space) {
    const n = Math.max(1, Math.round(Math.log2(s.decay * 10) * 1.6));
    for (let i = 1; i <= n; i++)
      g += `<circle cx="${voz[0]}" cy="${voz[1]}" r="${10 + i * 11}" fill="none" stroke="#8fd9a8" opacity="${(0.32 * (1 - i / (n + 1))).toFixed(2)}"/>`;
    if (!aberto) {
      for (const [eixo, pos] of [["x", sala.x], ["x", sala.x + larg], ["y", sala.y]]) {
        const img = eixo === "x" ? [2 * pos - voz[0], voz[1]] : [voz[0], 2 * pos - voz[1]];
        const t = eixo === "x" ? (pos - ouv[0]) / (img[0] - ouv[0]) : (pos - ouv[1]) / (img[1] - ouv[1]);
        const r = [ouv[0] + (img[0] - ouv[0]) * t, ouv[1] + (img[1] - ouv[1]) * t];
        g += `<path d="M${voz[0]} ${voz[1]}L${r[0]} ${r[1]}L${ouv[0]} ${ouv[1]}" fill="none" stroke="#c9902e" stroke-width="1" opacity=".55" stroke-dasharray="2 3"/>`;
      }
    }
  }
  g += `<line x1="${voz[0]}" y1="${voz[1]}" x2="${ouv[0]}" y2="${ouv[1]}" stroke="#efe5cf" stroke-width="1.5" opacity="${s.barrier ? .3 : .8}"/>`;
  if (s.barrier) {
    const esp = 2 + (s.muffle / 100) * 7, my = sala.y + alt + esp / 2 - 1;
    g += `<rect x="${sala.x}" y="${my - esp / 2}" width="${larg}" height="${esp}" fill="#c9902e" opacity=".85"/>`;
    if (s.leak > 5) g += `<rect x="${cx + 30}" y="${my - esp / 2 - 1}" width="${4 + s.leak / 6}" height="${esp + 2}" fill="#222120"/>`;
  }
  if (s.device) g += `<rect x="${voz[0] - 8}" y="${voz[1] - 6}" width="16" height="12" rx="2" fill="#d2382f"/><circle cx="${voz[0]}" cy="${voz[1]}" r="3" fill="#1d1c1b"/>`;
  else g += `<circle cx="${voz[0]}" cy="${voz[1]}" r="6" fill="#d2382f"/>`;
  g += `<text x="${voz[0] + 12}" y="${voz[1] + 3}" fill="#cfc6b4" font-size="8" font-weight="800" letter-spacing="1">VOICE</text>`;
  g += `<circle cx="${ouv[0]}" cy="${ouv[1]}" r="6" fill="none" stroke="#efe5cf" stroke-width="2"/><circle cx="${ouv[0]}" cy="${ouv[1]}" r="2" fill="#efe5cf"/>`;
  g += `<text x="${ouv[0] + 11}" y="${ouv[1] + 3}" fill="#8d8577" font-size="8" font-weight="800" letter-spacing="1">LISTENER</text>`;
  if (s.space) g += `<text x="${ouv[0] - 12}" y="${ouv[1] - Math.min(24, px / 3)}" fill="#efe5cf" font-size="8.5" font-weight="800" text-anchor="end">${FORMATO.distance(s.distance)}</text>`;
  svg.innerHTML = g;

  document.getElementById("lugar").textContent = !s.space ? "DRY" : aberto ? "OUTDOOR" : FORMATO.size(s.size) + " ROOM";
  // leituras (as mesmas contas de drAlvo() em Motor.cpp)
  if (s.space) {
    const L = Math.min(s.size, 60), Al = L < 12 ? Math.min(0.45 * L, 2.7) : 0.45 * L;
    const dr = aberto ? 8 - 10 * Math.log10(s.distance / 2)
      : Math.min(15, Math.max(-15, 20 * Math.log10(0.057 * Math.sqrt((L * 0.75 * L * Al) / s.decay) / s.distance) + 10));
    const hc = aberto ? 1.6 : Math.min(1.5, Al / 2);
    const pd = (Math.sqrt(s.distance * s.distance + 4 * hc * hc) - s.distance) / 343 * 1000;
    document.getElementById("nDr").textContent = (dr >= 0 ? "+" : "−") + Math.abs(dr).toFixed(1) + " dB";
    document.getElementById("nRt").textContent = FORMATO.decay(s.decay);
    document.getElementById("nPd").textContent = pd.toFixed(1) + " ms";
  } else {
    for (const id of ["nRt", "nPd"]) document.getElementById(id).textContent = "—";
    document.getElementById("nDr").textContent = "DRY";
  }
  // nível na fala: o motor iguala o volume do DEVICE; a distância e a parede baixam
  let nivel = s.output;
  if (s.space) nivel -= Math.min(8, 3 * Math.log2(Math.max(s.distance, 1)));
  if (s.barrier) nivel -= 4 + 10 * s.muffle / 100;
  document.getElementById("nNivel").textContent = s.bypass ? "BYPASS" : FORMATO.output(nivel);
}

// ---------------------------------------------------------------------------------------------

let taxa = 48000;
function atualizar() {
  const e = ler();
  knobs.forEach((el) => desenharKnob(el, e));
  for (const b of document.querySelectorAll("#cadeia .tecla")) b.classList.toggle("ligada", e[b.dataset.m]);
  for (const id of ["mono", "bypass"]) document.getElementById(id).classList.toggle("ligada", e[id]);
  const cod = document.getElementById("codec");
  cod.textContent = e.codec === 0 ? "CODEC" : "CODEC · " + CODECS[e.codec];
  cod.classList.toggle("ligada", e.codec !== 0);
  document.getElementById("airPlus").classList.toggle("ligada", e.airPlus);
  const nome = NOMES[e.preset];
  for (const b of document.querySelectorAll(".preset")) b.classList.toggle("ativo", b.dataset.nome === nome);
  const mexido = editado(e);
  const selo = document.getElementById("selo");
  selo.textContent = nome.toUpperCase() + (mexido ? " · EDITED" : "");
  selo.classList.toggle("aviso", mexido);
  const cauda = e.space ? " · tail " + FORMATO.decay(e.decay * 1.3 + 0.3) : "";
  const atraso = e.device && e.codec >= 2 && Math.abs(taxa - 48000) < 1 ? " · codec delay 26.5 ms (compensated)" : "";
  document.getElementById("status").textContent = (taxa / 1000).toFixed(1).replace(".0", "") + " kHz" + (e.mono ? " · mono" : "") + cauda + atraso;
  desenharCurva(e);
  desenharCena(e);
}

// redesenha uma vez por quadro quando qualquer parâmetro muda
let pedido = false;
const agendar = () => { if (!pedido) { pedido = true; requestAnimationFrame(() => { pedido = false; atualizar(); }); } };
for (const s of Object.values(sliders)) s.valueChangedEvent.addListener(agendar);
for (const c of Object.values(chaves)) c.valueChangedEvent.addListener(agendar);
for (const c of Object.values(combos)) c.valueChangedEvent.addListener(agendar);
atualizar();
if (nativo("taxa")) nativo("taxa")().then((t) => { if (t > 0) { taxa = t; agendar(); } });

// conferência dos testes: carrega um preset pedido e depois relata o que a janela desenhou
if (nativo("testePreset")) nativo("testePreset")().then((nome) => { if (nome) setTimeout(() => carregar(nome), 800); });
setTimeout(() => {
  const f = nativo("janelaPronta");
  if (!f) return;
  const resumo = {
    juce: !!juce,
    erros: window.__situaErros,
    selo: document.getElementById("selo").textContent,
    knobs: knobs.map((k) => k.dataset.param + "=" + k.querySelector(".v").textContent),
    presets: document.querySelectorAll(".preset").length,
    fonte: document.fonts ? document.fonts.check("12px Archivo") : null,
  };
  f(JSON.stringify(resumo));
}, 2500);
