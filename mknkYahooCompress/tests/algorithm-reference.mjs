import assert from 'node:assert/strict';

function definitions() {
  const qualities = [100,98,96,94,92,90,88,86,84,82,80,77,74,70,66,62,58,54,50,45,40,35];
  const out = [];
  for (const q of qualities) {
    if (q >= 84) out.push([q, 3]);
    out.push([q, 1]);
  }
  return out;
}

function score(left, right, width, height) {
  let luma = 0, chroma = 0, edge = 0, edgeCount = 0;
  const yAt = (p, i) => .299 * p[i + 2] + .587 * p[i + 1] + .114 * p[i];
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    const i = (y * width + x) * 3;
    const [lb, lg, lr] = left.slice(i, i + 3), [rb, rg, rr] = right.slice(i, i + 3);
    const ly = .299*lr+.587*lg+.114*lb, ry = .299*rr+.587*rg+.114*rb;
    const lcb = -.168736*lr-.331264*lg+.5*lb, rcb = -.168736*rr-.331264*rg+.5*rb;
    const lcr = .5*lr-.418688*lg-.081312*lb, rcr = .5*rr-.418688*rg-.081312*rb;
    luma += (ly-ry)**2; chroma += ((lcb-rcb)**2+(lcr-rcr)**2)/2;
    if (x) { const p=i-3, d=(ly-yAt(left,p))-(ry-yAt(right,p)); edge+=d*d; edgeCount++; }
    if (y) { const p=i-width*3, d=(ly-yAt(left,p))-(ry-yAt(right,p)); edge+=d*d; edgeCount++; }
  }
  const pixels = width*height;
  const error = (.68*Math.sqrt(luma/pixels)+.20*Math.sqrt(chroma/pixels)+.12*Math.sqrt(edge/Math.max(1,edgeCount)))/255;
  return Math.max(0, Math.min(1, 1-error));
}

assert.equal(definitions().length, 31);
assert.deepEqual(definitions()[0], [100, 3]);
assert.deepEqual(definitions().at(-1), [35, 1]);
const original = [0,0,0, 255,255,255, 0,0,255, 0,255,0];
assert.equal(score(original, original, 2, 2), 1);
const changed = [...original]; changed[0] = 30;
assert.ok(score(original, changed, 2, 2) < 1);
assert.ok(score(original, changed, 2, 2) > .9);
const candidates = [
  {size: 5_100_000, score: .9999, quality: 100},
  {size: 4_900_000, score: .9980, quality: 98},
  {size: 3_200_000, score: .9900, quality: 92},
];
const fitting = candidates.filter(candidate => candidate.size <= 5_000_000)
  .sort((a, b) => b.score - a.score || b.quality - a.quality);
assert.equal(fitting[0].quality, 98);
assert.ok(fitting[0].size <= 5_000_000);
console.log('8 algorithm checks passed');
