// SPDX-License-Identifier: GPL-3.0-only
// Scala intervals -> the FM-1's octave-repeating, twelve-key cent table.
export function parseScala(text) {
  if (typeof text !== "string" || text.length > 65536) throw new Error("Scala file must be at most 64 KiB.");
  const lines = text.replace(/^\uFEFF/, "").split(/\r?\n/)
    .map(line => line.split("!")[0].trim()).filter(Boolean);
  const countAt = /^\d+$/.test(lines[1] || "") ? 1 : 0;
  if (!/^\d+$/.test(lines[countAt] || "")) throw new Error("Missing Scala note count.");
  const count = Number(lines[countAt]);
  if (count < 1 || count > 12) throw new Error("This firmware supports octave scales with 1–12 notes. This scale needs a different keyboard mapping.");
  if (lines.length !== countAt + count + 1) throw new Error("Scala note count does not match its intervals.");
  const intervals = lines.slice(countAt + 1).map(line => {
    const token = line.split(/\s+/)[0];
    let cents;
    if (/^\+?\d+\/\d+$/.test(token)) {
      const [a, b] = token.split("/").map(Number);
      cents = 1200 * Math.log2(a / b);
    } else if (/^\+?\d+$/.test(token)) cents = 1200 * Math.log2(Number(token));
    else if (/^[+-]?(?:\d+\.\d*|\d*\.\d+)$/.test(token)) cents = Number(token);
    else throw new Error(`Invalid Scala interval: ${token}`);
    if (!Number.isFinite(cents) || cents <= 0) throw new Error("Scala intervals must be positive and finite.");
    return cents;
  });
  if (intervals.some((v, i) => i && v <= intervals[i - 1])) throw new Error("Scala intervals must be strictly increasing.");
  if (Math.abs(intervals.at(-1) - 1200) > 0.01) throw new Error("This firmware supports octave periods (1200 cents or 2/1).");
  const degrees = intervals.slice(0, -1);
  // Ordered, distinct chromatic keys; find the closest representable mapping.
  // Ties prefer the upper key, so 350 cents becomes E half-flat.
  let best = null;
  function visit(i, previous, keys, offsets, cost) {
    if (i === degrees.length) {
      if (!best || cost < best.cost) best = { keys, offsets, cost };
      return;
    }
    for (let key = 11 - (degrees.length - i - 1); key > previous; key--) {
      const delta = degrees[i] - key * 100;
      if (Math.abs(delta) > 63) continue;
      visit(i + 1, key, [...keys, key], [...offsets, Math.round(delta)], cost + delta * delta);
    }
  }
  visit(0, 0, [], [], 0);
  if (!best) throw new Error("These intervals cannot fit distinct keys with the available ±63-cent tuning range.");
  const cents = Array(12).fill(0);
  best.keys.forEach((key, i) => { cents[key] = best.offsets[i]; });
  const mask = best.keys.reduce((mask, key) => mask | 1 << key, 1);
  return { name: countAt ? lines[0] : "Imported scale", count, cents, mask,
    maxError: Math.max(0, ...degrees.map((v, i) => Math.abs(v - (100 * best.keys[i] + best.offsets[i])))) };
}
