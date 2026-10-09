// Animated figures for the EposLib documentation. No dependencies.
//
//   <div class="epos-anim" data-anim="control-sim"></div>
//   <div class="epos-anim" data-anim="bus"></div>
//   <div class="epos-anim" data-anim="cascade"></div>
//   <div class="epos-anim" data-anim="state-machine"></div>
//
// The control simulator integrates a real (if simple) model: an inertia with
// viscous friction, a current loop with a current limit and a voltage limit
// (back-EMF), and the same PI / PID laws the EPOS4 uses - so what it shows
// is what the equations on the Control loops page predict.

(function () {
  "use strict";

  const RED = "#df1b25";
  const INK = "#1c1c1c";
  const GREY = "#9a948c";
  const RULE = "#d9d4cc";
  const PAPER = "#ffffff";
  const BLUE = "#1f6feb";
  const GREEN = "#1a8f4c";
  const AMBER = "#d99a00";
  const FONT = '"Archivo", "Helvetica Neue", Arial, sans-serif';
  const MONO = '"IBM Plex Mono", monospace';

  const reduceMotion = window.matchMedia &&
    window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  // --- helpers -------------------------------------------------------------

  function makeCanvas(host, height) {
    const canvas = document.createElement("canvas");
    canvas.style.width = "100%";
    canvas.style.height = height + "px";
    canvas.style.display = "block";
    host.appendChild(canvas);
    const ctx = canvas.getContext("2d");
    function resize() {
      const dpr = window.devicePixelRatio || 1;
      const w = host.clientWidth;
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(height * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      return w;
    }
    let width = resize();
    window.addEventListener("resize", () => { width = resize(); });
    return { canvas, ctx, get width() { return width; }, height };
  }

  function button(parent, label, onClick, active) {
    const b = document.createElement("button");
    b.type = "button";
    b.textContent = label;
    b.className = "epos-anim__btn" + (active ? " is-active" : "");
    b.addEventListener("click", onClick);
    parent.appendChild(b);
    return b;
  }

  function loop(step) {
    let last = performance.now();
    let running = !reduceMotion;
    function frame(now) {
      const dt = Math.max(0, Math.min(0.05, (now - last) / 1000));
      last = now;
      if (running) step(dt);
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
    return {
      get running() { return running; },
      set running(v) { running = v; last = performance.now(); },
    };
  }

  function roundRect(ctx, x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
  }

  function box(ctx, x, y, w, h, title, sub, fill, stroke) {
    roundRect(ctx, x, y, w, h, 4);
    ctx.fillStyle = fill || "#faf8f5";
    ctx.fill();
    ctx.lineWidth = 1.5;
    ctx.strokeStyle = stroke || INK;
    ctx.stroke();
    ctx.fillStyle = INK;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.font = "600 13px " + FONT;
    ctx.fillText(title, x + w / 2, y + h / 2 - (sub ? 7 : 0));
    if (sub) {
      ctx.font = "11px " + FONT;
      ctx.fillStyle = GREY;
      ctx.fillText(sub, x + w / 2, y + h / 2 + 9);
    }
  }

  // --- 1. control simulator -------------------------------------------------

  const MODES = {
    ppm: {
      label: "Profile Position",
      unit: "turns", top: "position [turns]", info:
        "PPM: one command - 15 turns at 1200 rpm, 3000 rpm/s. The drive generates the trapezoid; its PID position loop follows it.",
    },
    csv: {
      label: "Cyclic Velocity",
      unit: "rpm", top: "velocity [rpm]", info:
        "CSV: your program stages a velocity every 10 ms; the drive's PI velocity loop follows it and brakes on the way down.",
    },
    cst: {
      label: "Cyclic Torque",
      unit: "rpm", top: "velocity [rpm]", info:
        "CST: your program stages 0.02 N m. Nobody controls velocity: it rises until the back-EMF uses up the voltage (no-load speed), and coasts when the torque returns to zero.",
    },
    csp: {
      label: "Cyclic Position",
      unit: "deg", top: "position [deg at output]", info:
        "CSP: your program stages a sine every 10 ms; the drive interpolates and follows it with a small lag.",
    },
  };

  function controlSim(host) {
    host.classList.add("epos-anim--sim");
    const bar = document.createElement("div");
    bar.className = "epos-anim__bar";
    host.appendChild(bar);
    const info = document.createElement("p");
    info.className = "epos-anim__info";
    const stage = makeCanvas(host, 330);
    host.appendChild(info);
    const readout = document.createElement("div");
    readout.className = "epos-anim__readout";
    host.appendChild(readout);

    // Motor: the team's test motor, Kt = 0.105 N m/A, at 24 V.
    const P = { J: 2.5e-5, b: 4e-5, Kt: 0.105, Kn: 91.1, R: 0.6, U: 24, Imax: 9.28 };
    let mode = "csv";
    let s;
    const buttons = {};

    function reset() {
      s = { t: 0, th: 0, w: 0, i: 0, integ: 0, prevE: 0, hist: [], cmd: 0, sp: 0, spv: 0, ref: 0 };
    }

    for (const key of Object.keys(MODES)) {
      buttons[key] = button(bar, MODES[key].label, () => {
        mode = key;
        Object.values(buttons).forEach((b) => b.classList.remove("is-active"));
        buttons[key].classList.add("is-active");
        reset();
      }, key === mode);
    }
    const play = button(bar, reduceMotion ? "▶ Play" : "❚❚ Pause", () => {
      anim.running = !anim.running;
      play.textContent = anim.running ? "❚❚ Pause" : "▶ Play";
    });
    reset();

    const T = 6.0;          // seconds shown and per run
    const dt = 1 / 2500;    // the drive's velocity/position loop rate

    function trapezoid(t, t0, vmax, acc, hold) {
      const tu = vmax / acc;
      const u = t - t0;
      if (u < 0) return 0;
      if (u < tu) return acc * u;
      if (u < tu + hold) return vmax;
      return Math.max(0, vmax - acc * (u - tu - hold));
    }

    function stepOnce() {
      const t = s.t;
      const omegaMax = 0.9 * P.U * P.Kn * 2 * Math.PI / 60;   // rad/s at full voltage
      let iCmd = 0;
      let ref = 0;
      let actual = 0;

      if (mode === "ppm") {
        // trapezoid generated by the drive: 0 -> 15 turns, 1200 rpm, 3000 rpm/s.
        // Short moves never reach the profile velocity: then it is a triangle.
        const dist = 15 * 2 * Math.PI;
        const acc = 3000 * 2 * Math.PI / 60;
        const vmax = Math.min(1200 * 2 * Math.PI / 60, Math.sqrt(acc * dist));
        const t0 = 0.4;
        const ta = vmax / acc;
        const xa = 0.5 * acc * ta * ta;
        const tc = Math.max(0, (dist - 2 * xa) / vmax);
        const u = t - t0;
        let xd = 0;
        let vd = 0;
        if (u > 0 && u < ta) { xd = 0.5 * acc * u * u; vd = acc * u; }
        else if (u >= ta && u < ta + tc) { xd = xa + vmax * (u - ta); vd = vmax; }
        else if (u >= ta + tc && u < 2 * ta + tc) {
          const r = u - ta - tc;
          xd = xa + vmax * tc + vmax * r - 0.5 * acc * r * r;
          vd = vmax - acc * r;
        } else if (u >= 2 * ta + tc) { xd = dist; vd = 0; }
        const e = xd - s.th;
        s.integ += e * dt;
        const de = vd - s.w;
        iCmd = 8 * e + 2 * s.integ + 0.25 * de + 0.0002 * vd;
        ref = xd / (2 * Math.PI);
        actual = s.th / (2 * Math.PI);
      } else if (mode === "csv") {
        // velocity trapezoid staged every 10 ms (zero-order hold, interpolated)
        const vref = trapezoid(Math.floor(t * 100) / 100, 0.4, 1500, 2500, 2.0) * 2 * Math.PI / 60;
        const e = vref - s.w;
        s.integ += e * dt;
        iCmd = 0.05 * e + 1.2 * s.integ;
        ref = vref * 60 / (2 * Math.PI);
        actual = s.w * 60 / (2 * Math.PI);
      } else if (mode === "cst") {
        const tq = (t > 0.4 && t < 3.4) ? 0.02 : 0;   // N m, at the motor shaft
        iCmd = tq / P.Kt;
        ref = NaN;
        actual = s.w * 60 / (2 * Math.PI);
        s.cmd = tq;
      } else {
        // csp: 30 degree sine at the output of a 1:20 gearbox, staged every
        // 10 ms. Like the drive, interpolate linearly from the previous staged
        // point to the newest one over the interpolation period.
        const p = (tt) => 20 * (30 * Math.PI / 180) * Math.sin(2 * Math.PI * 0.4 * Math.max(0, tt - 0.2));
        const ts = Math.floor(t * 100) / 100;
        const p0 = p(ts - 0.01);
        const p1 = p(ts);
        const thd = p0 + (p1 - p0) * (t - ts) / 0.01;
        const vd = (p1 - p0) / 0.01;
        const e = thd - s.th;
        s.integ += e * dt;
        iCmd = 6 * e + 3 * s.integ + 0.12 * (vd - s.w) + 0.0003 * vd;
        ref = thd / 20 * 180 / Math.PI;
        actual = s.th / 20 * 180 / Math.PI;
      }

      // Current limit, and the voltage limit: the back-EMF leaves less room
      // the faster the motor turns.
      let i = Math.max(-P.Imax, Math.min(P.Imax, iCmd));
      const headroom = Math.max(0, 1 - Math.abs(s.w) / omegaMax);
      const iVolt = headroom * 0.9 * P.U / P.R;
      i = Math.max(-iVolt, Math.min(iVolt, i));
      if (Math.sign(iCmd) !== Math.sign(s.w)) i = Math.max(-P.Imax, Math.min(P.Imax, iCmd));
      s.i += (i - s.i) * 0.25;   // the 25 kHz current loop, seen from 2.5 kHz

      const torque = P.Kt * s.i;
      const wdot = (torque - P.b * s.w) / P.J;
      s.w += wdot * dt;
      s.th += s.w * dt;
      s.t += dt;
      return { ref, actual, torque };
    }

    function draw(sample) {
      const ctx = stage.ctx;
      const W = stage.width;
      const H = stage.height;
      ctx.clearRect(0, 0, W, H);
      ctx.fillStyle = PAPER;
      ctx.fillRect(0, 0, W, H);

      const dial = Math.min(130, W * 0.22);
      const left = 54;
      const right = W - dial - 30;
      const plotW = right - left;
      const top1 = 16, h1 = 170, top2 = 222, h2 = 80;

      // axes
      const hist = s.hist;
      let maxAbs = 1;
      for (const p of hist) {
        if (!isNaN(p.ref)) maxAbs = Math.max(maxAbs, Math.abs(p.ref));
        maxAbs = Math.max(maxAbs, Math.abs(p.actual));
      }
      maxAbs *= 1.15;
      const signed = mode === "csp";
      const y1 = (v) => signed ? top1 + h1 / 2 - v / maxAbs * h1 / 2 : top1 + h1 - v / maxAbs * h1;
      let maxT = 0.01;
      for (const p of hist) maxT = Math.max(maxT, Math.abs(p.torque));
      maxT *= 1.2;
      const y2 = (v) => top2 + h2 / 2 - v / maxT * h2 / 2;
      const x = (t) => left + t / T * plotW;

      ctx.strokeStyle = RULE;
      ctx.lineWidth = 1;
      for (let k = 0; k <= 6; k++) {
        ctx.beginPath(); ctx.moveTo(x(k), top1); ctx.lineTo(x(k), top2 + h2); ctx.stroke();
      }
      ctx.strokeStyle = INK;
      ctx.beginPath(); ctx.moveTo(left, top1); ctx.lineTo(left, top1 + h1);
      ctx.lineTo(right, top1 + h1); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(left, top2); ctx.lineTo(left, top2 + h2); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(left, y2(0)); ctx.lineTo(right, y2(0)); ctx.stroke();
      if (signed) { ctx.beginPath(); ctx.moveTo(left, y1(0)); ctx.lineTo(right, y1(0)); ctx.stroke(); }

      ctx.fillStyle = GREY;
      ctx.font = "11px " + FONT;
      ctx.textAlign = "left";
      ctx.textBaseline = "top";
      ctx.fillText(MODES[mode].top, left + 4, top1);
      ctx.fillText("torque [N m]", left + 4, top2);
      ctx.textAlign = "right";
      ctx.fillText(maxAbs.toFixed(maxAbs < 10 ? 1 : 0), left - 4, signed ? top1 : top1);
      ctx.fillText(maxT.toFixed(2), left - 4, top2);
      ctx.textAlign = "center";
      for (let k = 0; k <= 6; k += 1) ctx.fillText(k + " s", x(k), top2 + h2 + 6);

      function curve(key, y, color, dash, width) {
        ctx.beginPath();
        let started = false;
        for (const p of hist) {
          const v = p[key];
          if (isNaN(v)) { started = false; continue; }
          if (!started) { ctx.moveTo(x(p.t), y(v)); started = true; } else ctx.lineTo(x(p.t), y(v));
        }
        ctx.setLineDash(dash || []);
        ctx.strokeStyle = color;
        ctx.lineWidth = width || 2;
        ctx.stroke();
        ctx.setLineDash([]);
      }
      curve("ref", y1, GREY, [5, 4], 1.5);
      curve("actual", y1, RED, null, 2.2);
      curve("torque", y2, INK, null, 1.5);

      // legend
      ctx.font = "11px " + FONT;
      ctx.textAlign = "left";
      ctx.fillStyle = RED; ctx.fillRect(right - 150, top1 + 4, 14, 3);
      ctx.fillStyle = INK; ctx.fillText("actual", right - 132, top1);
      if (mode !== "cst") {
        ctx.strokeStyle = GREY; ctx.setLineDash([5, 4]);
        ctx.beginPath(); ctx.moveTo(right - 85, top1 + 5); ctx.lineTo(right - 71, top1 + 5); ctx.stroke();
        ctx.setLineDash([]);
        ctx.fillText("target", right - 67, top1);
      }

      // the shaft
      const cx = W - dial / 2 - 12;
      const cy = top1 + 20 + dial / 2;
      const r = dial / 2 - 6;
      ctx.beginPath(); ctx.arc(cx, cy, r, 0, 2 * Math.PI);
      ctx.fillStyle = "#faf8f5"; ctx.fill();
      ctx.lineWidth = 2; ctx.strokeStyle = INK; ctx.stroke();
      const shown = mode === "csp" ? s.th / 20 : s.th;
      for (let k = 0; k < 6; k++) {
        const a = shown + k * Math.PI / 3;
        ctx.beginPath();
        ctx.moveTo(cx + Math.cos(a) * r * 0.25, cy + Math.sin(a) * r * 0.25);
        ctx.lineTo(cx + Math.cos(a) * r * 0.85, cy + Math.sin(a) * r * 0.85);
        ctx.strokeStyle = k === 0 ? RED : GREY;
        ctx.lineWidth = k === 0 ? 4 : 1.5;
        ctx.stroke();
      }
      ctx.beginPath(); ctx.arc(cx, cy, 6, 0, 2 * Math.PI); ctx.fillStyle = INK; ctx.fill();
      ctx.fillStyle = INK; ctx.textAlign = "center"; ctx.font = "600 12px " + FONT;
      ctx.fillText(mode === "csp" ? "output shaft (1:20)" : "motor shaft", cx, cy + r + 18);
      ctx.font = "12px " + MONO;
      const rpm = s.w * 60 / (2 * Math.PI);
      ctx.fillText((Math.abs(rpm) < 0.5 ? 0 : rpm).toFixed(0) + " rpm", cx, cy + r + 36);
      ctx.fillText((Math.abs(s.i) < 0.005 ? 0 : s.i).toFixed(2) + " A", cx, cy + r + 54);

      if (sample) {
        const unit = MODES[mode].unit;
        const fix = (v, n) => (Math.abs(v) < Math.pow(10, -n) / 2 ? 0 : v).toFixed(n);
        const n = unit === "rpm" ? 0 : 2;
        const tgt = mode === "cst" ? fix(s.cmd, 3) + " N m" : fix(sample.ref, n) + " " + unit;
        readout.textContent = `t = ${s.t.toFixed(2)} s   target ${tgt}   actual ${fix(sample.actual, n)} ${unit}   torque ${fix(sample.torque, 3)} N m`;
      }
    }

    let acc = 0;
    let lastSample = null;
    const anim = loop((dtFrame) => {
      acc += dtFrame;
      const steps = Math.floor(acc / dt);
      acc -= steps * dt;
      for (let k = 0; k < steps; k++) {
        lastSample = stepOnce();
        if (Math.round(s.t / dt) % 25 === 0) {   // keep one point per 10 ms
          s.hist.push({ t: s.t, ref: lastSample.ref, actual: lastSample.actual, torque: lastSample.torque });
        }
        if (s.t >= T) { reset(); break; }
      }
      info.textContent = MODES[mode].info;
      draw(lastSample);
    });

    if (reduceMotion) {
      for (let k = 0; k < T / dt - 10; k++) {
        lastSample = stepOnce();
        if (Math.round(s.t / dt) % 25 === 0) s.hist.push({ t: s.t, ...lastSample });
      }
      info.textContent = MODES[mode].info;
      draw(lastSample);
    }
  }

  // --- 2. the CAN bus -------------------------------------------------------

  function busAnim(host) {
    const stage = makeCanvas(host, 250);
    const legend = document.createElement("p");
    legend.className = "epos-anim__info";
    legend.innerHTML =
      '<span style="color:' + RED + '">■</span> SYNC 0x080 &nbsp; ' +
      '<span style="color:' + BLUE + '">■</span> RPDO 0x200/0x300 + id (Controlword, targets) &nbsp; ' +
      '<span style="color:' + GREEN + '">■</span> TPDO 0x180/0x280 + id (Statusword, feedback) &nbsp; ' +
      '<span style="color:' + AMBER + '">■</span> heartbeat 0x700 + id. ' +
      'One 10 ms SYNC period, slowed down 200 times.';
    host.appendChild(legend);

    const nodes = [2, 3, 4];
    const period = 2.0;   // seconds per SYNC period on screen
    let t = 0;
    let cycle = 0;

    function draw() {
      const ctx = stage.ctx;
      const W = stage.width;
      const H = stage.height;
      ctx.clearRect(0, 0, W, H);
      ctx.fillStyle = PAPER; ctx.fillRect(0, 0, W, H);
      const busY = 130;
      const mx = 20, mw = Math.min(150, W * 0.24);
      const dw = Math.min(120, (W - mw - 80) / 3 - 20);
      const dx = (k) => mx + mw + 40 + k * ((W - mx - mw - 60) / 3) + 10;

      // bus line
      ctx.strokeStyle = INK; ctx.lineWidth = 3;
      ctx.beginPath(); ctx.moveTo(mx + mw / 2, busY); ctx.lineTo(W - 20, busY); ctx.stroke();
      ctx.fillStyle = GREY; ctx.font = "11px " + FONT; ctx.textAlign = "left";
      ctx.fillText("CAN_H / CAN_L - 1 Mbit/s", mx + mw / 2 + 6, busY + 16);

      box(ctx, mx, 30, mw, 54, "Master", "EposLib (node 1)", "#fff6f6", RED);
      ctx.strokeStyle = INK; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.moveTo(mx + mw / 2, 84); ctx.lineTo(mx + mw / 2, busY); ctx.stroke();
      nodes.forEach((id, k) => {
        const x0 = dx(k);
        box(ctx, x0, 170, dw, 54, "EPOS4", "node " + id);
        ctx.beginPath(); ctx.moveTo(x0 + dw / 2, busY); ctx.lineTo(x0 + dw / 2, 170);
        ctx.strokeStyle = INK; ctx.lineWidth = 2; ctx.stroke();
      });

      const u = t / period;   // 0..1 within the cycle
      function packet(x0, x1, y0, y1, p, color, label) {
        if (p < 0 || p > 1) return;
        const e = p < 0.5 ? 2 * p * p : 1 - Math.pow(-2 * p + 2, 2) / 2;
        const px = x0 + (x1 - x0) * e;
        const py = y0 + (y1 - y0) * e;
        roundRect(ctx, px - 22, py - 9, 44, 18, 3);
        ctx.fillStyle = color; ctx.fill();
        ctx.fillStyle = "#fff"; ctx.font = "600 10px " + MONO; ctx.textAlign = "center"; ctx.textBaseline = "middle";
        ctx.fillText(label, px, py);
      }
      const mX = mx + mw / 2;
      // SYNC: broadcast from the master to every drive
      nodes.forEach((id, k) => {
        packet(mX, dx(k) + dw / 2, 84, 170, (u - 0.02) / 0.18, RED, "SYNC");
      });
      // RPDOs to each drive, one after the other
      nodes.forEach((id, k) => {
        packet(mX, dx(k) + dw / 2, 84, 170, (u - 0.22 - k * 0.07) / 0.2, BLUE, "0x2" + "0" + id);
      });
      // TPDOs back, after the drive sampled on SYNC
      nodes.forEach((id, k) => {
        packet(dx(k) + dw / 2, mX, 170, 84, (u - 0.5 - k * 0.07) / 0.2, GREEN, "0x18" + id);
      });
      // a heartbeat every other cycle
      if (cycle % 2 === 1) packet(dx(1) + dw / 2, mX, 170, 84, (u - 0.8) / 0.18, AMBER, "0x703");
      if (cycle % 2 === 0) {
        nodes.forEach((id, k) => packet(mX, dx(k) + dw / 2, 84, 170, (u - 0.8) / 0.18, AMBER, "0x701"));
      }

      ctx.fillStyle = INK; ctx.font = "12px " + MONO; ctx.textAlign = "right"; ctx.textBaseline = "top";
      ctx.fillText("t = " + (cycle * 10 + u * 10).toFixed(1) + " ms", W - 20, 10);
    }

    const anim = loop((dt) => {
      t += dt;
      if (t > period) { t -= period; cycle += 1; }
      draw();
    });
    const bar = document.createElement("div");
    bar.className = "epos-anim__bar";
    host.insertBefore(bar, host.firstChild);
    const play = button(bar, reduceMotion ? "▶ Play" : "❚❚ Pause", () => {
      anim.running = !anim.running;
      play.textContent = anim.running ? "❚❚ Pause" : "▶ Play";
    });
    draw();
  }

  // --- 3. the control cascade ----------------------------------------------

  function cascadeAnim(host) {
    const stage = makeCanvas(host, 250);
    const note = document.createElement("p");
    note.className = "epos-anim__info";
    note.textContent = "Position modes (PPM, HMM, CSP) close the position PID, whose output is a current demand; velocity modes (PVM, CSV) close the velocity PI instead; CST commands the current loop directly. Each dot is one update: the current loop (25 kHz) runs ten times for every update of the outer loops (2.5 kHz), and a setpoint from the master arrives once every 25 of those (100 Hz). Slowed down.";
    host.appendChild(note);
    let t = 0;

    function arrow(ctx, x0, y0, x1, y1, color) {
      ctx.strokeStyle = color || INK; ctx.fillStyle = color || INK; ctx.lineWidth = 1.5;
      ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
      const a = Math.atan2(y1 - y0, x1 - x0);
      ctx.beginPath(); ctx.moveTo(x1, y1);
      ctx.lineTo(x1 - 7 * Math.cos(a - 0.4), y1 - 7 * Math.sin(a - 0.4));
      ctx.lineTo(x1 - 7 * Math.cos(a + 0.4), y1 - 7 * Math.sin(a + 0.4));
      ctx.closePath(); ctx.fill();
    }
    function dot(ctx, x0, y0, x1, y1, phase, color) {
      ctx.beginPath();
      ctx.arc(x0 + (x1 - x0) * phase, y0 + (y1 - y0) * phase, 5, 0, 2 * Math.PI);
      ctx.fillStyle = color; ctx.fill();
    }

    function draw() {
      const ctx = stage.ctx;
      const W = stage.width;
      ctx.clearRect(0, 0, W, stage.height);
      ctx.fillStyle = PAPER; ctx.fillRect(0, 0, W, stage.height);
      const bw = Math.min(140, (W - 120) / 4), bh = 48;
      const gap = (W - 20 - 4 * bw) / 3;
      const X = [10, 10 + bw + gap, 10 + 2 * (bw + gap), 10 + 3 * (bw + gap)];
      const yMid = 95, yTop = 30, yBot = 160;

      box(ctx, X[0], yMid - bh / 2, bw, bh, "Master", "setpoints, 100 Hz", "#fff6f6", RED);
      box(ctx, X[1], yTop, bw, bh, "Position PID", "2.5 kHz");
      box(ctx, X[1], yBot - bh, bw, bh, "Velocity PI", "2.5 kHz");
      box(ctx, X[2], yMid - bh / 2, bw, bh, "Current PI", "25 kHz");
      box(ctx, X[3], yMid - bh / 2, bw, bh, "Motor", "τ = Kt·i", "#f4f2ee");

      const m = [X[0] + bw, yMid];
      const pIn = [X[1], yTop + bh / 2], vIn = [X[1], yBot - bh / 2];
      const pOut = [X[1] + bw, yTop + bh / 2], vOut = [X[1] + bw, yBot - bh / 2];
      const cIn = [X[2], yMid], cOut = [X[2] + bw, yMid], mIn = [X[3], yMid];
      arrow(ctx, m[0], m[1], pIn[0], pIn[1]);
      arrow(ctx, m[0], m[1], vIn[0], vIn[1]);
      arrow(ctx, pOut[0], pOut[1], cIn[0], cIn[1] - 8);
      arrow(ctx, vOut[0], vOut[1], cIn[0], cIn[1] + 8);
      arrow(ctx, cOut[0], cOut[1], mIn[0], mIn[1]);
      // CST: straight to the current loop
      ctx.setLineDash([3, 3]);
      arrow(ctx, m[0], m[1] + 4, cIn[0], cIn[1] + 2, GREY);
      ctx.setLineDash([]);

      ctx.fillStyle = GREY; ctx.font = "11px " + FONT; ctx.textAlign = "center"; ctx.textBaseline = "bottom";
      ctx.fillText("θ*", (m[0] + pIn[0]) / 2 - 8, (m[1] + pIn[1]) / 2 - 4);
      ctx.fillText("ω*", (m[0] + vIn[0]) / 2 - 8, (m[1] + vIn[1]) / 2 + 16);
      ctx.fillText("τ* (CST)", (m[0] + cIn[0]) / 2, yMid + 2);
      ctx.fillText("i*", (pOut[0] + cIn[0]) / 2, (pOut[1] + cIn[1]) / 2 - 4);
      ctx.fillText("i*", (vOut[0] + cIn[0]) / 2, (vOut[1] + cIn[1]) / 2 + 16);
      ctx.fillText("PWM", (cOut[0] + mIn[0]) / 2, yMid - 4);

      // feedback
      ctx.strokeStyle = GREY; ctx.setLineDash([4, 3]); ctx.lineWidth = 1.2;
      const mx = X[3] + bw / 2;
      ctx.beginPath(); ctx.moveTo(mx, yMid + bh / 2); ctx.lineTo(mx, 215); ctx.lineTo(X[1] + bw / 2, 215);
      ctx.lineTo(X[1] + bw / 2, yBot); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(mx, yMid - bh / 2); ctx.lineTo(mx, 12); ctx.lineTo(X[1] + bw / 2, 12);
      ctx.lineTo(X[1] + bw / 2, yTop); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(mx - 20, yMid + bh / 2); ctx.lineTo(mx - 20, 190); ctx.lineTo(X[2] + bw / 2, 190);
      ctx.lineTo(X[2] + bw / 2, yMid + bh / 2); ctx.stroke();
      ctx.setLineDash([]);
      ctx.fillStyle = GREY; ctx.textBaseline = "top";
      ctx.fillText("position θ (encoder)", (mx + X[1] + bw / 2) / 2, 14);
      ctx.fillText("current i", (mx + X[2] + bw / 2) / 2 - 10, 192);
      ctx.fillText("velocity ω (observer)", (mx + X[1] + bw / 2) / 2, 217);

      // the updates
      dot(ctx, m[0], m[1], pIn[0], pIn[1], (t * 0.25) % 1, RED);
      dot(ctx, m[0], m[1], vIn[0], vIn[1], (t * 0.25 + 0.5) % 1, RED);
      dot(ctx, pOut[0], pOut[1], cIn[0], cIn[1] - 8, (t * 2.5) % 1, INK);
      dot(ctx, vOut[0], vOut[1], cIn[0], cIn[1] + 8, (t * 2.5 + 0.5) % 1, INK);
      dot(ctx, cOut[0], cOut[1], mIn[0], mIn[1], (t * 25 / 4) % 1, RED);
    }
    loop((dt) => { t += dt; draw(); });
    draw();
  }

  // --- 4. the state machine -------------------------------------------------

  function stateAnim(host) {
    const bar = document.createElement("div");
    bar.className = "epos-anim__bar";
    host.appendChild(bar);
    const stage = makeCanvas(host, 300);
    const caption = document.createElement("p");
    caption.className = "epos-anim__info";
    host.appendChild(caption);

    const S = {
      SOD: [0.10, 0.22, "Switch on disabled"], RTSO: [0.37, 0.22, "Ready to switch on"],
      SO: [0.63, 0.22, "Switched on"], OE: [0.89, 0.22, "Operation enabled"],
      QSA: [0.89, 0.62, "Quick stop active"], FRA: [0.55, 0.85, "Fault reaction active"],
      F: [0.15, 0.85, "Fault"],
    };
    const SEQS = {
      "Enable()": [["SOD", "Controlword 0x0006 - Shutdown"], ["RTSO", "Controlword 0x0007 - Switch on"],
        ["SO", "Controlword 0x000F - Enable operation"], ["OE", "Power to the motor"]],
      "A fault": [["OE", "Running"], ["FRA", "Error detected: the drive stops the motor (fault reaction)"],
        ["F", "Fault: no power. Enable() stops here - it never resets a fault."]],
      "ClearFault()": [["F", "Fault"], ["SOD", "Controlword bit 7: 0 → 1 (fault reset)"],
        ["RTSO", "Enable() again: Shutdown"], ["SO", "Switch on"], ["OE", "Enable operation"]],
      "QuickStop()": [["OE", "Running"], ["QSA", "Controlword bit 2 low: decelerate on the quick stop ramp"],
        ["OE", "Enable() from Quick stop active: transition 16"]],
    };
    let seq = "Enable()";
    let t = 0;
    const btns = {};
    Object.keys(SEQS).forEach((k) => {
      btns[k] = button(bar, k, () => {
        seq = k; t = 0;
        Object.values(btns).forEach((b) => b.classList.remove("is-active"));
        btns[k].classList.add("is-active");
      }, k === seq);
    });

    function draw() {
      const ctx = stage.ctx;
      const W = stage.width;
      const H = stage.height;
      ctx.clearRect(0, 0, W, H);
      ctx.fillStyle = PAPER; ctx.fillRect(0, 0, W, H);
      const steps = SEQS[seq];
      const k = Math.min(steps.length - 1, Math.floor(t / 1.4));
      const cur = steps[k][0];
      const bw = Math.min(150, W * 0.2), bh = 46;
      const pos = (key) => [S[key][0] * W - bw / 2, S[key][1] * H - bh / 2];
      // power region
      ctx.fillStyle = "#fdeced";
      roundRect(ctx, W * 0.76, 12, W * 0.235, H - 24, 6); ctx.fill();
      ctx.fillStyle = RED; ctx.font = "600 11px " + FONT; ctx.textAlign = "center"; ctx.textBaseline = "top";
      ctx.fillText("power to the motor", W * 0.877, 16);
      // edges of the sequence so far
      ctx.strokeStyle = RED; ctx.lineWidth = 2.5;
      for (let j = 1; j <= k; j++) {
        const [ax, ay] = pos(steps[j - 1][0]);
        const [bx, by] = pos(steps[j][0]);
        if (steps[j - 1][0] === steps[j][0]) continue;
        ctx.beginPath(); ctx.moveTo(ax + bw / 2, ay + bh / 2); ctx.lineTo(bx + bw / 2, by + bh / 2); ctx.stroke();
      }
      Object.keys(S).forEach((key) => {
        const [x0, y0] = pos(key);
        const on = key === cur;
        box(ctx, x0, y0, bw, bh, S[key][2], null, on ? RED : "#faf8f5", on ? RED : GREY);
        if (on) {
          ctx.fillStyle = "#fff"; ctx.font = "600 13px " + FONT; ctx.textAlign = "center"; ctx.textBaseline = "middle";
          ctx.fillText(S[key][2], x0 + bw / 2, y0 + bh / 2);
        }
      });
      caption.textContent = seq + " - step " + (k + 1) + "/" + steps.length + ": " + steps[k][1];
    }
    loop((dt) => {
      t += dt;
      if (t > SEQS[seq].length * 1.4 + 1.5) t = 0;
      draw();
    });
    draw();
  }

  // --- mount ------------------------------------------------------------------

  const FACTORIES = { "control-sim": controlSim, bus: busAnim, cascade: cascadeAnim, "state-machine": stateAnim };

  function mount() {
    document.querySelectorAll(".epos-anim[data-anim]").forEach((el) => {
      if (el.dataset.mounted) return;
      el.dataset.mounted = "1";
      const f = FACTORIES[el.dataset.anim];
      if (f) f(el);
    });
  }

  if (typeof document$ !== "undefined") {
    document$.subscribe(mount);
  } else {
    document.addEventListener("DOMContentLoaded", mount);
  }
})();
