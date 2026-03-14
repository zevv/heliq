import { useState, useRef, useEffect, useCallback } from "react";

const N = 80;
const L = 20;
const dx = L / N;
const dk = (2 * Math.PI) / L;
const DT = 0.007;
const STEPS_PER_FRAME = 6;
const ABSORB_WIDTH = 10;

function fft1d(re, im, n, inverse) {
  let j = 0;
  for (let i = 0; i < n; i++) {
    if (i < j) {
      let t = re[i]; re[i] = re[j]; re[j] = t;
      t = im[i]; im[i] = im[j]; im[j] = t;
    }
    let m = n >> 1;
    while (m >= 1 && j >= m) { j -= m; m >>= 1; }
    j += m;
  }
  for (let step = 1; step < n; step <<= 1) {
    const ang = (inverse ? 1 : -1) * Math.PI / step;
    const wR = Math.cos(ang), wI = Math.sin(ang);
    for (let grp = 0; grp < n; grp += step << 1) {
      let cR = 1, cI = 0;
      for (let p = 0; p < step; p++) {
        const a = grp + p, b = a + step;
        const tR = cR * re[b] - cI * im[b];
        const tI = cR * im[b] + cI * re[b];
        re[b] = re[a] - tR; im[b] = im[a] - tI;
        re[a] += tR; im[a] += tI;
        const nR = cR * wR - cI * wI;
        cI = cR * wI + cI * wR; cR = nR;
      }
    }
  }
  if (inverse) {
    for (let i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
  }
}

function fft2d(re, im, inverse) {
  const tR = new Float64Array(N);
  const tI = new Float64Array(N);
  for (let r = 0; r < N; r++) {
    for (let c = 0; c < N; c++) { tR[c] = re[r * N + c]; tI[c] = im[r * N + c]; }
    fft1d(tR, tI, N, inverse);
    for (let c = 0; c < N; c++) { re[r * N + c] = tR[c]; im[r * N + c] = tI[c]; }
  }
  for (let c = 0; c < N; c++) {
    for (let r = 0; r < N; r++) { tR[r] = re[r * N + c]; tI[r] = im[r * N + c]; }
    fft1d(tR, tI, N, inverse);
    for (let r = 0; r < N; r++) { re[r * N + c] = tR[r]; im[r * N + c] = tI[r]; }
  }
}

function kVal(i) { return i < N / 2 ? i * dk : (i - N) * dk; }

function mulArrays(pRe, pIm, oRe, oIm) {
  for (let k = 0; k < N * N; k++) {
    const r = pRe[k] * oRe[k] - pIm[k] * oIm[k];
    const i = pRe[k] * oIm[k] + pIm[k] * oRe[k];
    pRe[k] = r;
    pIm[k] = i;
  }
}

// precompute 1D absorbing profile
function makeAbsorb1D() {
  const a = new Float64Array(N);
  for (let i = 0; i < N; i++) a[i] = 1.0;
  const str = 0.03;
  for (let i = 0; i < ABSORB_WIDTH; i++) {
    const f = 1.0 - str * Math.pow((ABSORB_WIDTH - i) / ABSORB_WIDTH, 2);
    a[i] = f;
    a[N - 1 - i] = f;
  }
  return a;
}

function makeAbsorb2D(a1d) {
  const a = new Float64Array(N * N);
  for (let i = 0; i < N; i++) {
    for (let j = 0; j < N; j++) {
      a[i * N + j] = a1d[i] * a1d[j];
    }
  }
  return a;
}

function makeOperators(V0) {
  const potRe = new Float64Array(N * N);
  const potIm = new Float64Array(N * N);
  const kinRe = new Float64Array(N * N);
  const kinIm = new Float64Array(N * N);
  const w = 0.6;
  for (let i = 0; i < N; i++) {
    const x1 = -L / 2 + i * dx;
    for (let j = 0; j < N; j++) {
      const x2 = -L / 2 + j * dx;
      const d = x1 - x2;
      const V = V0 * Math.exp(-(d * d) / (2 * w * w));
      const ph = -V * DT / 2;
      potRe[i * N + j] = Math.cos(ph);
      potIm[i * N + j] = Math.sin(ph);
    }
  }
  for (let i = 0; i < N; i++) {
    const k1 = kVal(i);
    for (let j = 0; j < N; j++) {
      const k2 = kVal(j);
      const ph = -(k1 * k1 + k2 * k2) / 2 * DT;
      kinRe[i * N + j] = Math.cos(ph);
      kinIm[i * N + j] = Math.sin(ph);
    }
  }
  return { potRe, potIm, kinRe, kinIm };
}

function createInitialState() {
  const psiRe = new Float64Array(N * N);
  const psiIm = new Float64Array(N * N);
  const sigma = 0.9;
  const x01 = -3.5, x02 = 3.5, p0 = 2.2;
  let norm = 0;
  for (let i = 0; i < N; i++) {
    const x1 = -L / 2 + i * dx;
    for (let j = 0; j < N; j++) {
      const x2 = -L / 2 + j * dx;
      const env = Math.exp(-((x1 - x01) * (x1 - x01)) / (4 * sigma * sigma))
                * Math.exp(-((x2 - x02) * (x2 - x02)) / (4 * sigma * sigma));
      const phase = p0 * x1 - p0 * x2;
      psiRe[i * N + j] = env * Math.cos(phase);
      psiIm[i * N + j] = env * Math.sin(phase);
      norm += env * env;
    }
  }
  norm = Math.sqrt(norm * dx * dx);
  for (let k = 0; k < N * N; k++) {
    psiRe[k] /= norm;
    psiIm[k] /= norm;
  }
  return { psiRe, psiIm };
}

const absorb1d = makeAbsorb1D();
const absorb2d = makeAbsorb2D(absorb1d);

export default function EntanglementViz() {
  const canvasRef = useRef(null);
  const stateRef = useRef(null);
  const opsRef = useRef(null);
  const animRef = useRef(null);
  const [playing, setPlaying] = useState(false);
  const [interaction, setInteraction] = useState(20);
  const [timeDisplay, setTimeDisplay] = useState(0);
  const [entMeasure, setEntMeasure] = useState(0);
  const [normDisplay, setNormDisplay] = useState(1);
  const frameRef = useRef(0);

  useEffect(() => {
    const init = createInitialState();
    stateRef.current = { ...init, t: 0 };
    opsRef.current = makeOperators(20);
  }, []);

  useEffect(() => {
    opsRef.current = makeOperators(interaction);
  }, [interaction]);

  const evolveStep = useCallback(() => {
    const s = stateRef.current;
    const ops = opsRef.current;
    if (!s || !ops) return;
    mulArrays(s.psiRe, s.psiIm, ops.potRe, ops.potIm);
    fft2d(s.psiRe, s.psiIm, false);
    mulArrays(s.psiRe, s.psiIm, ops.kinRe, ops.kinIm);
    fft2d(s.psiRe, s.psiIm, true);
    mulArrays(s.psiRe, s.psiIm, ops.potRe, ops.potIm);
    // absorbing boundaries
    for (let k = 0; k < N * N; k++) {
      s.psiRe[k] *= absorb2d[k];
      s.psiIm[k] *= absorb2d[k];
    }
    s.t += DT;
  }, []);

  const computeEnt = useCallback(() => {
    const s = stateRef.current;
    if (!s) return { ent: 0, norm: 1 };
    const m1 = new Float64Array(N);
    const m2 = new Float64Array(N);
    let total = 0;
    for (let i = 0; i < N; i++) {
      for (let j = 0; j < N; j++) {
        const idx = i * N + j;
        const p = s.psiRe[idx] * s.psiRe[idx] + s.psiIm[idx] * s.psiIm[idx];
        m1[i] += p;
        m2[j] += p;
        total += p;
      }
    }
    let diff = 0;
    if (total > 1e-20) {
      for (let i = 0; i < N; i++) {
        for (let j = 0; j < N; j++) {
          const product = (m1[i] * m2[j]) / (total + 1e-30);
          const actual = s.psiRe[i * N + j] * s.psiRe[i * N + j] + s.psiIm[i * N + j] * s.psiIm[i * N + j];
          const d = actual - product;
          diff += d * d;
        }
      }
    }
    const ent = Math.min(1, Math.sqrt(diff / (total * total + 1e-30)) * N * 30);
    return { ent, norm: total * dx * dx };
  }, []);

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    const s = stateRef.current;
    if (!canvas || !s) return;
    const ctx = canvas.getContext("2d");
    const W = 580;
    const H = 530;
    const mapSize = 380;

    ctx.fillStyle = "#0a0a0f";
    ctx.fillRect(0, 0, W, H);

    const prob = new Float64Array(N * N);
    const m1 = new Float64Array(N);
    const m2 = new Float64Array(N);
    let maxP = 0;
    for (let i = 0; i < N; i++) {
      for (let j = 0; j < N; j++) {
        const idx = i * N + j;
        const p = s.psiRe[idx] * s.psiRe[idx] + s.psiIm[idx] * s.psiIm[idx];
        prob[idx] = p;
        if (p > maxP) maxP = p;
        m1[i] += p;
        m2[j] += p;
      }
    }

    // heatmap
    const imgData = ctx.createImageData(mapSize, mapSize);
    for (let py = 0; py < mapSize; py++) {
      const j = N - 1 - Math.floor((py / mapSize) * N);
      for (let px = 0; px < mapSize; px++) {
        const i = Math.floor((px / mapSize) * N);
        const v = Math.pow(prob[i * N + j] / (maxP + 1e-30), 0.35);
        const idx4 = (py * mapSize + px) * 4;
        imgData.data[idx4] = Math.floor(Math.min(255, v * 50 + v * v * 230));
        imgData.data[idx4 + 1] = Math.floor(Math.min(255, v * v * 150 + v * v * v * 110));
        imgData.data[idx4 + 2] = Math.floor(Math.min(255, v * 90 + v * v * 190));
        imgData.data[idx4 + 3] = 255;
      }
    }
    ctx.putImageData(imgData, 0, 0);

    // absorbing boundary indicator
    const abFrac = ABSORB_WIDTH / N;
    ctx.fillStyle = "rgba(255,50,50,0.04)";
    ctx.fillRect(0, 0, mapSize * abFrac, mapSize);
    ctx.fillRect(mapSize * (1 - abFrac), 0, mapSize * abFrac, mapSize);
    ctx.fillRect(0, 0, mapSize, mapSize * abFrac);
    ctx.fillRect(0, mapSize * (1 - abFrac), mapSize, mapSize * abFrac);

    // diagonal
    ctx.strokeStyle = "rgba(255,255,255,0.08)";
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(0, mapSize);
    ctx.lineTo(mapSize, 0);
    ctx.stroke();
    ctx.setLineDash([]);

    ctx.fillStyle = "rgba(255,255,255,0.3)";
    ctx.font = "11px monospace";
    ctx.textAlign = "center";
    ctx.fillText("particle 1 position \u2192", mapSize / 2, mapSize - 4);
    ctx.save();
    ctx.translate(13, mapSize / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText("particle 2 position \u2192", 0, 0);
    ctx.restore();

    // marginal 1
    const mY = mapSize + 10;
    const mH = 50;
    ctx.fillStyle = "rgba(255,255,255,0.03)";
    ctx.fillRect(0, mY, mapSize, mH);
    let mm1 = 0;
    for (let i = 0; i < N; i++) if (m1[i] > mm1) mm1 = m1[i];
    ctx.beginPath();
    ctx.strokeStyle = "rgba(120,180,255,0.7)";
    ctx.lineWidth = 2;
    for (let i = 0; i < N; i++) {
      const x = (i / N) * mapSize;
      const y = mY + mH - 4 - (m1[i] / (mm1 + 1e-30)) * (mH - 14);
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.fillStyle = "rgba(255,255,255,0.2)";
    ctx.font = "9px monospace";
    ctx.textAlign = "center";
    ctx.fillText("particle 1 alone", mapSize / 2, mY + 10);

    // marginal 2
    const m2Y = mY + mH + 6;
    ctx.fillStyle = "rgba(255,255,255,0.03)";
    ctx.fillRect(0, m2Y, mapSize, mH);
    let mm2 = 0;
    for (let j = 0; j < N; j++) if (m2[j] > mm2) mm2 = m2[j];
    ctx.beginPath();
    ctx.strokeStyle = "rgba(255,160,100,0.7)";
    ctx.lineWidth = 2;
    for (let j = 0; j < N; j++) {
      const x = (j / N) * mapSize;
      const y = m2Y + mH - 4 - (m2[j] / (mm2 + 1e-30)) * (mH - 14);
      if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.fillStyle = "rgba(255,255,255,0.2)";
    ctx.font = "9px monospace";
    ctx.textAlign = "center";
    ctx.fillText("particle 2 alone", mapSize / 2, m2Y + 10);

    // side panel
    const px = mapSize + 16;
    ctx.textAlign = "left";

    ctx.fillStyle = "rgba(255,255,255,0.25)";
    ctx.font = "10px monospace";
    ctx.fillText("TIME", px, 20);
    ctx.fillStyle = "rgba(255,255,255,0.5)";
    ctx.font = "14px monospace";
    ctx.fillText("t = " + s.t.toFixed(2), px, 40);

    ctx.fillStyle = "rgba(255,255,255,0.25)";
    ctx.font = "10px monospace";
    ctx.fillText("ENTANGLEMENT", px, 75);
    const barW = 170;
    ctx.fillStyle = "rgba(255,255,255,0.06)";
    ctx.fillRect(px, 85, barW, 10);
    const eColor = entMeasure < 0.3 ? "#50c878" : entMeasure < 0.6 ? "#dcc83c" : "#ff6450";
    ctx.fillStyle = eColor;
    ctx.fillRect(px, 85, Math.max(2, entMeasure * barW), 10);
    ctx.fillStyle = eColor;
    ctx.font = "10px monospace";
    const eLabel = entMeasure < 0.05 ? "factorable" :
                   entMeasure < 0.3 ? "slightly entangled" :
                   entMeasure < 0.6 ? "entangled" : "strongly entangled";
    ctx.fillText(eLabel, px, 110);

    ctx.fillStyle = "rgba(255,255,255,0.25)";
    ctx.font = "10px monospace";
    ctx.fillText("NORM", px, 140);
    ctx.fillStyle = "rgba(255,255,255,0.4)";
    ctx.fillText((normDisplay * 100).toFixed(1) + "%", px, 157);
    ctx.fillStyle = "rgba(255,255,255,0.15)";
    ctx.font = "9px monospace";
    ctx.fillText("(drops as amplitude", px, 172);
    ctx.fillText(" exits through edges)", px, 184);

    ctx.fillStyle = "rgba(255,255,255,0.13)";
    ctx.font = "9px monospace";
    const lines = [
      "",
      "round blob = independent",
      "diagonal = entangled",
      "",
      "red tint at edges:",
      "absorbing boundary",
      "(prevents reflections)",
      "",
      "bottom plots: each",
      "particle seen alone",
      "",
      "dashed: x\u2081 = \u2212x\u2082"
    ];
    lines.forEach((line, idx) => {
      ctx.fillText(line, px, 200 + idx * 14);
    });
  }, [entMeasure, normDisplay]);

  useEffect(() => {
    if (!playing) setTimeout(() => draw(), 50);
  }, [entMeasure, normDisplay, draw, playing]);

  useEffect(() => {
    if (!playing) return;
    const step = () => {
      for (let i = 0; i < STEPS_PER_FRAME; i++) evolveStep();
      frameRef.current++;
      const s = stateRef.current;
      if (s) setTimeDisplay(s.t);
      if (frameRef.current % 8 === 0) {
        const { ent, norm } = computeEnt();
        setEntMeasure(ent);
        setNormDisplay(norm);
      }
      draw();
      animRef.current = requestAnimationFrame(step);
    };
    animRef.current = requestAnimationFrame(step);
    return () => { if (animRef.current) cancelAnimationFrame(animRef.current); };
  }, [playing, evolveStep, draw, computeEnt]);

  const reset = () => {
    setPlaying(false);
    const init = createInitialState();
    stateRef.current = { ...init, t: 0 };
    opsRef.current = makeOperators(interaction);
    setTimeDisplay(0);
    setEntMeasure(0);
    setNormDisplay(1);
    frameRef.current = 0;
    setTimeout(() => draw(), 50);
  };

  return (
    <div style={{
      background: "#0a0a0f", minHeight: "100vh", color: "white",
      fontFamily: "monospace", padding: "16px",
      display: "flex", flexDirection: "column", alignItems: "center", gap: "10px"
    }}>
      <div style={{ opacity: 0.4, fontSize: "11px", letterSpacing: "2px", textTransform: "uppercase" }}>
        two-particle entanglement · real schrödinger evolution
      </div>

      <canvas ref={canvasRef} width={580} height={530}
        style={{ width: "580px", height: "530px", borderRadius: "6px" }}
      />

      <div style={{ display: "flex", gap: "12px", alignItems: "center", flexWrap: "wrap", justifyContent: "center" }}>
        <button onClick={() => setPlaying(!playing)} style={{
          background: playing ? "rgba(255,100,80,0.15)" : "rgba(100,180,255,0.15)",
          border: "1px solid " + (playing ? "rgba(255,100,80,0.3)" : "rgba(100,180,255,0.3)"),
          color: playing ? "#ff8866" : "#6ab4ff", padding: "8px 24px",
          borderRadius: "6px", cursor: "pointer", fontFamily: "monospace", fontSize: "12px"
        }}>
          {playing ? "PAUSE" : "EVOLVE"}
        </button>
        <button onClick={reset} style={{
          background: "rgba(255,255,255,0.04)",
          border: "1px solid rgba(255,255,255,0.1)",
          color: "rgba(255,255,255,0.4)", padding: "8px 24px",
          borderRadius: "6px", cursor: "pointer", fontFamily: "monospace", fontSize: "12px"
        }}>
          RESET
        </button>
        <div style={{ display: "flex", flexDirection: "column", gap: "2px", marginLeft: "8px" }}>
          <div style={{ fontSize: "10px", opacity: 0.3 }}>
            interaction: {interaction === 0 ? "off" : interaction < 10 ? "weak" : interaction < 25 ? "medium" : "strong"}
          </div>
          <input type="range" min="0" max="40" step="1" value={interaction}
            onChange={e => setInteraction(Number(e.target.value))}
            style={{ width: "140px", accentColor: "#6a9fff" }}
          />
        </div>
      </div>
    </div>
  );
}
