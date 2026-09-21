"""Convert the reviewed punch blocks from extract_dwg.py JSON, in millimetres.

No guessed bridging, scaling or silhouette fitting. A connected degree-two
endpoint graph and a simple polygon are required. Requires shapely/matplotlib.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
from collections import defaultdict
from shapely.geometry import Polygon

BLOCKS = ['RZ8895', 'RZ30104', 'RW8867', 'RW8890', 'RW88100', 'RW88120', '000', '001', '003', '004']
SAGITTA = 0.005
JOIN_TOL = 0.00001


def arc_points(e):
    assert e['Normal'] == [0, 0, 1], 'Only XY arcs are supported'
    cx, cy, cz = e['Center']
    assert abs(cz) < JOIN_TOL
    radius = e['Radius']
    start = e['StartAngle']
    end = start + (e['EndAngle'] - start) % math.tau
    # Explicit cardinal extrema keep the lowest tip and all bounds exact.
    limits = [start] + [i * math.pi / 2 for i in range(-4, 13)
                        if start < i * math.pi / 2 < end] + [end]
    max_step = 2 * math.acos(1 - min(SAGITTA / radius, 1))
    angles = [start]
    error = 0.0
    for a, b in zip(limits, limits[1:]):
        count = math.ceil((b - a) / max_step)
        error = max(error, radius * (1 - math.cos((b - a) / count / 2)))
        angles.extend(a + (b - a) * i / count for i in range(1, count + 1))
    return [(cx + radius * math.cos(a), cy + radius * math.sin(a)) for a in angles], error


def convert(name, block):
    curves, ignored, duplicates, corrections = [], [], [], []
    max_error = 0.0
    for e in block['entities']:
        if e['layer'] == 'CENTER':
            ignored.append(e['handle'])
            continue
        if e['type'] == 'AcDbLine':
            assert abs(e['StartPoint'][2]) < JOIN_TOL and abs(e['EndPoint'][2]) < JOIN_TOL
            points = [tuple(e['StartPoint'][:2]), tuple(e['EndPoint'][:2])]
        elif e['type'] == 'AcDbArc':
            points, error = arc_points(e)
            max_error = max(max_error, error)
        else:
            raise ValueError(f'{name}: unreviewed entity {e["type"]}')
        duplicate = next((c for c in curves if len(c['points']) == len(points)
                          and all(math.dist(a, b) < JOIN_TOL for a, b in zip(c['points'], points))), None)
        if duplicate:
            duplicates.append({'handle': e['handle'], 'same_as': duplicate['handle']})
        else:
            curves.append({'handle': e['handle'], 'points': points})

    if name == 'RZ8895':
        # This source block contains a tangent R0.2 arc drawn over two untrimmed
        # tip lines. Trim only their virtual apex ends to the existing arc ends.
        arc = next(c for c in curves if c['handle'] == '255')
        for line_handle, index, endpoint in [('253', 1, arc['points'][0]), ('254', 0, arc['points'][-1])]:
            line = next(c for c in curves if c['handle'] == line_handle)
            a, b = line['points']
            dx, dy = b[0] - a[0], b[1] - a[1]
            t = ((endpoint[0] - a[0]) * dx + (endpoint[1] - a[1]) * dy) / (dx * dx + dy * dy)
            projection = (a[0] + t * dx, a[1] + t * dy)
            assert 0 < t < 1 and math.dist(endpoint, projection) < JOIN_TOL
            corrections.append({'line': line_handle, 'old_endpoint': line['points'][index],
                                'new_endpoint': endpoint, 'reason': 'trim virtual apex to source tangent arc 255'})
            line['points'][index] = endpoint

    vertices, graph, max_join = [], defaultdict(list), 0.0
    def vertex(p):
        nonlocal max_join
        matches = [i for i, v in enumerate(vertices) if math.dist(p, v) < JOIN_TOL]
        assert len(matches) <= 1, 'Ambiguous endpoint match'
        if matches:
            max_join = max(max_join, math.dist(p, vertices[matches[0]]))
            return matches[0]
        vertices.append(p)
        return len(vertices) - 1
    for i, c in enumerate(curves):
        a, b = vertex(c['points'][0]), vertex(c['points'][-1])
        assert a != b, 'Degenerate entity'
        c['ends'] = [a, b]
        graph[a].append(i)
        graph[b].append(i)
    bad = {str(vertices[v]): [curves[i]['handle'] for i in es] for v, es in graph.items() if len(es) != 2}
    assert not bad, f'{name}: open or branched contour: {bad}'
    points, used, current = [], set(), 0
    while True:
        choices = [i for i in graph[current] if i not in used]
        if not choices:
            break
        i = choices[0]
        c = curves[i]
        a, b = c['ends']
        seq = list(c['points'])
        if current == b:
            seq.reverse()
            a, b = b, a
        seq[0], seq[-1] = vertices[a], vertices[b]
        points.extend(seq[:-1])
        used.add(i)
        current = b
    assert current == 0 and len(used) == len(curves), f'{name}: disconnected contours'
    poly = Polygon(points)
    assert poly.is_valid and poly.area > 0 and len(poly.interiors) == 0
    min_y = min(p[1] for p in points)
    bottom = [p for p in points if abs(p[1] - min_y) < 1e-8]
    if name == '004':
        assert len(bottom) == 2, 'Expected a flat bottom edge'
        origin = ((bottom[0][0] + bottom[1][0]) / 2, min_y)
        for i, p in enumerate(points):
            q = points[(i + 1) % len(points)]
            if p in bottom and q in bottom:
                points.insert(i + 1, origin)
                break
        else:
            raise ValueError('Flat datum is not an existing boundary edge')
    else:
        assert len(bottom) == 1, f'{name}: ambiguous tip datum'
        origin = bottom[0]
    profile = [(round(x - origin[0], 8), round(y - origin[1], 8)) for x, y in points]
    assert (0, 0) in profile and min(p[1] for p in profile) == 0 and len(profile) <= 256
    poly = Polygon(profile)
    assert poly.is_valid
    if not poly.exterior.is_ccw:
        profile.reverse()
    index = profile.index((0, 0))
    profile = profile[index:] + profile[:index]
    bounds = Polygon(profile).bounds
    report = {'block': name, 'source_handles': [c['handle'] for c in curves],
              'ignored_center_lines': ignored, 'duplicate_entities': duplicates, 'corrections': corrections,
              'source_origin_mm': origin, 'datum': 'bottom edge midpoint (flat punch)' if name == '004' else 'lowest point on tip arc',
              'width_mm': bounds[2] - bounds[0], 'height_mm': bounds[3], 'bounds_mm': bounds,
              'points': len(profile), 'area_mm2': poly.area, 'max_arc_sagitta_mm': max_error,
              'max_endpoint_join_mm': max_join}
    return profile, report, curves


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('snapshot', type=Path)
    args = parser.parse_args()
    source = json.loads((args.snapshot / 'entities.json').read_text(encoding='utf-8'))
    assert source['active_drawing_variables']['INSUNITS'] == 4, 'Drawing must be verified in mm'
    assert hashlib.sha256((args.snapshot / 'source.dwg').read_bytes()).hexdigest() == source['sha256']
    out = args.snapshot / 'tools'
    out.mkdir(exist_ok=True)
    converted = [(name, *convert(name, source['blocks'][name])) for name in BLOCKS]
    reports = []
    for name, profile, report, curves in converted:
        label = f'DWG {name}' + (' 平底压刀' if name == '004' else '')
        lines = ['ZH_TOOL_V1', f'name={label}', f'# source: {Path(source["source"]).name}; block: {name}',
                 f'# DWG SHA256: {source["sha256"]}', '# units: mm; no scaling; DWG XY -> tool XZ',
                 f'# total height including shank: {report["height_mm"]:.8f} mm',
                 f'# maximum chord error: {report["max_arc_sagitta_mm"]:.8f} mm',
                 f'# datum: {report["datum"]}',
                 '# See catalog.json for source handles, duplicate removal and any explicit trimming.']
        lines.extend(f'{x:.8f},{z:.8f}' for x, z in profile)
        file = out / f'DWG_{name}.ztool'
        file.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        report.update(file=file.name, sha256=hashlib.sha256(file.read_bytes()).hexdigest())
        reports.append(report)
    catalog = {'source': source['source'], 'source_sha256': source['sha256'], 'units': 'mm',
               'excluded': {'20070626131728': 'duplicate wrapper around RZ8895',
                            '20070626131823': 'lower die; current simulation supports upper punch only',
                            'other_blocks': 'machine/support/fastener/annotation; not upper punches'},
               'tools': reports}
    (args.snapshot / 'catalog.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axs = plt.subplots(2, 5, figsize=(15, 9), constrained_layout=True)
    for ax, (name, profile, report, curves) in zip(axs.flat, converted):
        xs, ys = zip(*(profile + profile[:1]))
        ax.fill(xs, ys, color='#234b68', alpha=0.9)
        # Source entities superimposed after the exact same translation.
        ox, oy = report['source_origin_mm']
        for c in curves:
            x, y = zip(*c['points'])
            ax.plot([v - ox for v in x], [v - oy for v in y], color='#54bed0', lw=.6)
        ax.plot(0, 0, 'o', color='#d1512d', ms=3)
        ax.set_aspect('equal')
        ax.set_ylim(-6, 156)
        ax.set_title(f'{name}' + (' / flat punch' if name == '004' else '') +
                     f'\n{report["width_mm"]:.3f} x {report["height_mm"]:.3f} mm', fontsize=10)
        ax.grid(alpha=.18)
        ax.tick_params(labelsize=7)
    fig.suptitle('DWG upper tool profiles | original scale in mm | total height includes shank\n'
                 'Blue edge: source geometry / Orange point: placement datum / Arc chord error <= 0.005 mm', fontsize=12)
    fig.savefig(args.snapshot / 'catalog.png', dpi=140)
    plt.close(fig)
    print(json.dumps([{'block': r['block'], 'points': r['points'], 'width': r['width_mm'],
                       'height': r['height_mm'], 'arc_error': r['max_arc_sagitta_mm']} for r in reports], indent=2))


if __name__ == '__main__':
    main()
