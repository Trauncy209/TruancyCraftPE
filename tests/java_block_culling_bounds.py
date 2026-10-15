"""Verify all generated Java block vertices fit Chunk's conservative margins."""
from pathlib import Path
import json
root = Path(__file__).resolve().parents[1]
source = root / 'original-tcpe/src/network/java/JavaBakedModels.inc'
minimum = [float('inf')] * 3
maximum = [float('-inf')] * 3
count = 0
for line in source.open():
    if not line.startswith('{{'):
        continue
    xyz = [float(value.rstrip('f')) for value in line[2:line.index('}')].split(',')]
    assert len(xyz) == 12
    for index, value in enumerate(xyz):
        axis = index % 3
        minimum[axis] = min(minimum[axis], value)
        maximum[axis] = max(maximum[axis], value)
    count += 1
assert count > 0
# A block at local 15 must also fit the outer section bound.
assert all(value >= -1 for value in minimum)
assert maximum[0] <= 2 and maximum[2] <= 2
assert maximum[1] <= 3
print(json.dumps({'quads': count, 'minimum': minimum, 'maximum': maximum,
                  'result': 'PASS', 'scope': 'compiled canonical Java block geometry'}))
