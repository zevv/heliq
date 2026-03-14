import { useState, useRef, useEffect, useCallback } from "react";

const TAU = Math.PI * 2;

export default function WavefunctionViz() {
  const canvasRef = useRef(null);
  const animRef = useRef(null);
  const timeRef = useRef(0);
  const [playing, setPlaying] = useState(false);
  const [momentum, setMomentum] = useState(0);
  const [spread, setSpread] = useState(0.5);
  const [mass, setMass] = useState(1.0);
  const [viewAngle, setViewAngle] = useState(0.45);
  const [showEnvelope, setShowEnvelope] = useState(true);
  const dragRef = useRef({ dragging: false, lastX: 0 });

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    const W = canvas.width;
    const H = canvas.height;
    const t = timeRef.current;

    ctx.fillStyle = "#0a0a0f";
    ctx.fillRect(0, 0, W, H);

    const cx = W / 2;
    const cy = H / 2;
    const scaleX = W * 0.35;
    const scaleY = H * 0.28;

    const sigma = 0.3 + spread * 0.7;
    const p0 = momentum * 8;
    const m = mass;

    const N = 600;
    const xMin = -3.5;
    const xMax = 3.5;

    // time-dependent spread
    const sigmaT = Math.sqrt(sigma * sigma + (t * t) / (4 * m * m * sigma * sigma));

    // draw the x-axis
    ctx.strokeStyle = "rgba(255,255,255,0.12)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(cx + xMin * scaleX * 0.28, cy);
    ctx.lineTo(cx + xMax * scaleX * 0.28, cy);
    ctx.stroke();

    // axis ticks
    ctx.fillStyle = "rgba(255,255,255,0.2)";
    ctx.font = "11px monospace";
    ctx.textAlign = "center";
    for (let i = -3; i <= 3; i++) {
      const tx = cx + i * scaleX * 0.28;
      ctx.beginPath();
      ctx.moveTo(tx, cy - 3);
      ctx.lineTo(tx, cy + 3);
      ctx.stroke();
    }

    // perspective params
    const cosA = Math.cos(viewAngle);
    const sinA = Math.sin(viewAngle);

    function project(x, re, im) {
      const sx = x * 0.28;
      const px = sx - im * sinA * 0.6;
      const py = -re - im * cosA * 0.6;
      return [cx + px * scaleX, cy + py * scaleY];
    }

    // compute wave function points
    const points = [];
    for (let i = 0; i < N; i++) {
      const x = xMin + (xMax - xMin) * (i / (N - 1));

      // group velocity shift
      const xShifted = x - (p0 / m) * t;

      // gaussian envelope (spreading)
      const envSq = sigma / sigmaT;
      const env = Math.sqrt(envSq) * Math.exp(-(xShifted * xShifted) / (4 * sigmaT * sigmaT));

      // phase: momentum winding + quadratic chirp from spreading
      const chirp = (xShifted * xShifted * t) / (4 * m * sigmaT * sigmaT * sigma * sigma);
      const phase = p0 * x - (p0 * p0 / (2 * m)) * t + chirp;

      const re = env * Math.cos(phase);
      const im = env * Math.sin(phase);

      points.push({ x, re, im, env });
    }

    // draw envelope shadow on the "floor"
    if (showEnvelope) {
      ctx.beginPath();
      ctx.strokeStyle = "rgba(120,180,255,0.15)";
      ctx.lineWidth = 1.5;
      for (let i = 0; i < N; i++) {
        const [px, py] = project(points[i].x, 0, 0);
        const envH = points[i].env * scaleY;
        if (i === 0) ctx.moveTo(px, cy - envH);
        else ctx.lineTo(px, cy - envH);
      }
      ctx.stroke();

      ctx.beginPath();
      for (let i = 0; i < N; i++) {
        const [px, py] = project(points[i].x, 0, 0);
        const envH = points[i].env * scaleY;
        if (i === 0) ctx.moveTo(px, cy + envH);
        else ctx.lineTo(px, cy + envH);
      }
      ctx.stroke();
    }

    // draw the "shadow" projections
    // real part projection (on the "back wall")
    ctx.beginPath();
    ctx.strokeStyle = "rgba(255,120,80,0.2)";
    ctx.lineWidth = 1;
    for (let i = 0; i < N; i++) {
      const [px, py] = project(points[i].x, points[i].re, 0);
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // imaginary part projection (on the "floor")
    ctx.beginPath();
    ctx.strokeStyle = "rgba(80,200,120,0.2)";
    ctx.lineWidth = 1;
    for (let i = 0; i < N; i++) {
      const [px, py] = project(points[i].x, 0, points[i].im);
      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    // draw vectors along the curve (fewer, for clarity)
    const vecStep = Math.floor(N / 80);
    for (let i = 0; i < N; i += vecStep) {
      const { x, re, im, env } = points[i];
      if (env < 0.01) continue;

      const [bx, by] = project(x, 0, 0);
      const [tx, ty] = project(x, re, im);

      ctx.strokeStyle = `rgba(180,200,255,0.25)`;
      ctx.lineWidth = 0.8;
      ctx.beginPath();
      ctx.moveTo(bx, by);
      ctx.lineTo(tx, ty);
      ctx.stroke();

      ctx.fillStyle = "rgba(200,220,255,0.5)";
      ctx.beginPath();
      ctx.arc(tx, ty, 1.5, 0, TAU);
      ctx.fill();
    }

    // draw main helix curve
    ctx.beginPath();
    ctx.lineWidth = 2.5;
    for (let i = 0; i < N; i++) {
      const [px, py] = project(points[i].x, points[i].re, points[i].im);

      // color by phase
      const phase = Math.atan2(points[i].im, points[i].re);
      const hue = ((phase / TAU) * 360 + 360) % 360;
      const alpha = Math.min(1, points[i].env * 3);

      if (i > 0 && alpha > 0.01) {
        ctx.strokeStyle = `hsla(${hue}, 80%, 65%, ${alpha})`;
        ctx.beginPath();
        const [px0, py0] = project(points[i - 1].x, points[i - 1].re, points[i - 1].im);
        ctx.moveTo(px0, py0);
        ctx.lineTo(px, py);
        ctx.stroke();
      }
    }

    // draw probability density below
    const probY = H - 90;
    const probH = 60;

    ctx.fillStyle = "rgba(255,255,255,0.06)";
    ctx.fillRect(40, probY - probH - 10, W - 80, probH + 20);

    ctx.strokeStyle = "rgba(255,255,255,0.12)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(40, probY);
    ctx.lineTo(W - 40, probY);
    ctx.stroke();

    ctx.beginPath();
    ctx.strokeStyle = "rgba(120,180,255,0.8)";
    ctx.lineWidth = 2;
    let maxEnv = 0;
    for (let i = 0; i < N; i++) maxEnv = Math.max(maxEnv, points[i].env);

    for (let i = 0; i < N; i++) {
      const px = 40 + ((points[i].x - xMin) / (xMax - xMin)) * (W - 80);
      const probVal = (points[i].env * points[i].env) / (maxEnv * maxEnv + 0.001);
      const py2 = probY - probVal * probH;
      if (i === 0) ctx.moveTo(px, py2);
      else ctx.lineTo(px, py2);
    }
    ctx.stroke();

    // fill under probability
    ctx.lineTo(40 + ((points[N - 1].x - xMin) / (xMax - xMin)) * (W - 80), probY);
    ctx.lineTo(40, probY);
    ctx.closePath();
    ctx.fillStyle = "rgba(120,180,255,0.1)";
    ctx.fill();

    // labels
    ctx.fillStyle = "rgba(255,255,255,0.5)";
    ctx.font = "11px monospace";
    ctx.textAlign = "left";
    ctx.fillText("|ψ(x)|²  probability density", 50, probY - probH - 16);

    ctx.fillStyle = "rgba(255,120,80,0.5)";
    ctx.fillText("real part", W - 140, 30);
    ctx.fillStyle = "rgba(80,200,120,0.5)";
    ctx.fillText("imag part", W - 140, 46);

    ctx.fillStyle = "rgba(255,255,255,0.3)";
    ctx.font = "11px monospace";
    ctx.textAlign = "center";
    ctx.fillText(`t = ${t.toFixed(2)}`, cx, 26);
  }, [momentum, spread, mass, viewAngle, showEnvelope]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    canvas.getContext("2d").scale(dpr, dpr);
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    const ctx = canvas.getContext("2d");
    ctx.scale(dpr, dpr);
    draw();
  }, []);

  useEffect(() => {
    if (!playing) return;
    let last = performance.now();
    const step = (now) => {
      const dt = (now - last) / 1000;
      last = now;
      timeRef.current += dt * 0.8;
      draw();
      animRef.current = requestAnimationFrame(step);
    };
    animRef.current = requestAnimationFrame(step);
    return () => cancelAnimationFrame(animRef.current);
  }, [playing, draw]);

  useEffect(() => {
    if (!playing) draw();
  }, [momentum, spread, mass, viewAngle, showEnvelope, draw, playing]);

  const reset = () => {
    timeRef.current = 0;
    setPlaying(false);
    setTimeout(draw, 0);
  };

  const handleMouseDown = (e) => {
    dragRef.current = { dragging: true, lastX: e.clientX };
  };
  const handleMouseMove = (e) => {
    if (!dragRef.current.dragging) return;
    const dx = e.clientX - dragRef.current.lastX;
    dragRef.current.lastX = e.clientX;
    setViewAngle((a) => Math.max(-1.2, Math.min(1.2, a + dx * 0.005)));
  };
  const handleMouseUp = () => {
    dragRef.current.dragging = false;
  };

  const sliderStyle = {
    width: "100%",
    accentColor: "#6a9fff",
    cursor: "pointer",
    height: "4px",
  };

  const labelStyle = {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    color: "rgba(255,255,255,0.6)",
    fontSize: "12px",
    fontFamily: "monospace",
    marginBottom: "2px",
  };

  const controlGroup = {
    marginBottom: "12px",
  };

  return (
    <div
      style={{
        background: "#0a0a0f",
        minHeight: "100vh",
        color: "white",
        fontFamily: "'IBM Plex Mono', monospace",
        padding: "16px",
        display: "flex",
        flexDirection: "column",
        gap: "12px",
      }}
    >
      <div style={{ textAlign: "center", opacity: 0.4, fontSize: "11px", letterSpacing: "2px", textTransform: "uppercase" }}>
        quantum wave function · 1D free particle
      </div>

      <div style={{ textAlign: "center", opacity: 0.25, fontSize: "10px" }}>
        drag canvas to rotate view
      </div>

      <canvas
        ref={canvasRef}
        style={{
          width: "100%",
          height: "420px",
          borderRadius: "8px",
          border: "1px solid rgba(255,255,255,0.06)",
          cursor: "grab",
        }}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
      />

      <div style={{ display: "flex", gap: "8px", justifyContent: "center" }}>
        <button
          onClick={() => setPlaying(!playing)}
          style={{
            background: playing ? "rgba(255,100,80,0.2)" : "rgba(100,180,255,0.2)",
            border: `1px solid ${playing ? "rgba(255,100,80,0.4)" : "rgba(100,180,255,0.4)"}`,
            color: playing ? "#ff8866" : "#6ab4ff",
            padding: "8px 28px",
            borderRadius: "6px",
            cursor: "pointer",
            fontFamily: "monospace",
            fontSize: "13px",
            letterSpacing: "1px",
          }}
        >
          {playing ? "PAUSE" : "EVOLVE"}
        </button>
        <button
          onClick={reset}
          style={{
            background: "rgba(255,255,255,0.06)",
            border: "1px solid rgba(255,255,255,0.12)",
            color: "rgba(255,255,255,0.5)",
            padding: "8px 28px",
            borderRadius: "6px",
            cursor: "pointer",
            fontFamily: "monospace",
            fontSize: "13px",
            letterSpacing: "1px",
          }}
        >
          RESET
        </button>
        <button
          onClick={() => setShowEnvelope(!showEnvelope)}
          style={{
            background: showEnvelope ? "rgba(120,180,255,0.15)" : "rgba(255,255,255,0.06)",
            border: `1px solid ${showEnvelope ? "rgba(120,180,255,0.3)" : "rgba(255,255,255,0.12)"}`,
            color: showEnvelope ? "#6ab4ff" : "rgba(255,255,255,0.5)",
            padding: "8px 20px",
            borderRadius: "6px",
            cursor: "pointer",
            fontFamily: "monospace",
            fontSize: "13px",
            letterSpacing: "1px",
          }}
        >
          ENVELOPE
        </button>
      </div>

      <div
        style={{
          display: "grid",
          gridTemplateColumns: "1fr 1fr 1fr",
          gap: "16px",
          padding: "12px 16px",
          background: "rgba(255,255,255,0.02)",
          borderRadius: "8px",
          border: "1px solid rgba(255,255,255,0.06)",
        }}
      >
        <div style={controlGroup}>
          <div style={labelStyle}>
            <span>momentum</span>
            <span style={{ color: "rgba(255,255,255,0.3)" }}>{momentum.toFixed(2)}</span>
          </div>
          <input
            type="range"
            min="-1.5"
            max="1.5"
            step="0.01"
            value={momentum}
            onChange={(e) => setMomentum(parseFloat(e.target.value))}
            style={sliderStyle}
          />
          <div style={{ fontSize: "10px", opacity: 0.25, marginTop: "4px" }}>
            helix winding direction & tightness
          </div>
        </div>

        <div style={controlGroup}>
          <div style={labelStyle}>
            <span>position spread (σ)</span>
            <span style={{ color: "rgba(255,255,255,0.3)" }}>{spread.toFixed(2)}</span>
          </div>
          <input
            type="range"
            min="0.1"
            max="1.5"
            step="0.01"
            value={spread}
            onChange={(e) => setSpread(parseFloat(e.target.value))}
            style={sliderStyle}
          />
          <div style={{ fontSize: "10px", opacity: 0.25, marginTop: "4px" }}>
            width of the gaussian envelope
          </div>
        </div>

        <div style={controlGroup}>
          <div style={labelStyle}>
            <span>mass</span>
            <span style={{ color: "rgba(255,255,255,0.3)" }}>{mass.toFixed(2)}</span>
          </div>
          <input
            type="range"
            min="0.2"
            max="3.0"
            step="0.01"
            value={mass}
            onChange={(e) => setMass(parseFloat(e.target.value))}
            style={sliderStyle}
          />
          <div style={{ fontSize: "10px", opacity: 0.25, marginTop: "4px" }}>
            heavier = slower drift, slower spread
          </div>
        </div>
      </div>

      <div
        style={{
          fontSize: "11px",
          color: "rgba(255,255,255,0.3)",
          textAlign: "center",
          lineHeight: "1.6",
          padding: "4px",
        }}
      >
        the helix color shows phase · the little stems show the complex amplitude vectors ·
        probability density shown below
      </div>
    </div>
  );
}
