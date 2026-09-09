/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Sample points, rather than rectangle widths, to retain rotation and skew.
export function frameTransform(utils) {
  const p = utils.toTopLevelWidgetRect(0, 0, 0, 0);
  const x = utils.toTopLevelWidgetRect(100, 0, 0, 0);
  const y = utils.toTopLevelWidgetRect(0, 100, 0, 0);
  return [(x.x - p.x) / 100, (x.y - p.y) / 100,
          (y.x - p.x) / 100, (y.y - p.y) / 100, p.x, p.y];
}

export function transformPoint(matrix, x, y) {
  return { x: matrix[0] * x + matrix[2] * y + matrix[4],
           y: matrix[1] * x + matrix[3] * y + matrix[5] };
}

export function inverseTransform(m) {
  const d = m[0] * m[3] - m[1] * m[2];
  if (!m.every(Number.isFinite) || Math.abs(d) < 1e-10) {
    throw new Error("Unavailable frame transform");
  }
  return [m[3] / d, -m[1] / d, -m[2] / d, m[0] / d,
          (m[2] * m[5] - m[3] * m[4]) / d,
          (m[1] * m[4] - m[0] * m[5]) / d];
}

// Only the documented geometry fields are transformed. Other numeric fields
// (selection indexes, request IDs, cursor positions) are not coordinates.
export function mapGeometry(data, from, to) {
  const inverse = inverseTransform(to);
  const point = (x, y) => {
    const p = transformPoint(from, x, y);
    return transformPoint(inverse, p.x, p.y);
  };
  const result = structuredClone(data);
  const visited = new Set();
  function visit(value) {
    if (!value || typeof value !== "object" || visited.has(value)) return;
    visited.add(value);
    if (Number.isFinite(value.xPos) && Number.isFinite(value.yPos)) {
      const p = point(value.xPos, value.yPos);
      value.xPos = p.x;
      value.yPos = p.y;
    }
    if ([value.left, value.top, value.right, value.bottom].every(Number.isFinite)) {
      const corners = [point(value.left, value.top), point(value.right, value.top),
                       point(value.left, value.bottom), point(value.right, value.bottom)];
      value.left = Math.min(...corners.map(p => p.x));
      value.right = Math.max(...corners.map(p => p.x));
      value.top = Math.min(...corners.map(p => p.y));
      value.bottom = Math.max(...corners.map(p => p.y));
    }
    for (const key of ["start", "end", "caret", "selection", "element"]) {
      visit(value[key]);
    }
  }
  visit(result);
  return result;
}
