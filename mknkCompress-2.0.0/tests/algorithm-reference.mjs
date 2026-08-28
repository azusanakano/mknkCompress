import assert from 'node:assert/strict';

function definitions(quality) {
  const steps = quality >= 90 ? [0, 3, 6, 9, 12, 16]
    : quality >= 75 ? [0, 4, 7, 10, 14, 18] : [0, 5, 9, 13, 17, 22];
  const out = [];
  for (const step of steps) {
    const q = Math.max(35, quality - step);
    if (q >= 88) out.push([q, 3]);
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

assert.equal(definitions(82).length, 6);
assert.equal(definitions(92).length, 8);
assert.deepEqual(definitions(70)[0], [70, 1]);
const original = [0,0,0, 255,255,255, 0,0,255, 0,255,0];
assert.equal(score(original, original, 2, 2), 1);
const changed = [...original]; changed[0] = 30;
assert.ok(score(original, changed, 2, 2) < 1);
assert.ok(score(original, changed, 2, 2) > .9);
console.log('6 algorithm checks passed');
