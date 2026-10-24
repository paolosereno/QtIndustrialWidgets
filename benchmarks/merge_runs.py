#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Merge the runs of one benchmark session into one set of Markdown tables.

Usage: merge_runs.py run1.md run2.md [run3.md ...] > merged.md

Every input is the stdout of one run of bench_StripChart or of bench_AnalysisChart (all runs of the
same benchmark, e.g. interleaved runs of one session). Every run is published:

- each table keeps its identifying columns (channels, samples, signal, axis; or the case);
- columns that must be identical in every run (trace pixels, samples per frame, work per trial,
  trials, samples in view, decimated channels) are checked: the script stops if a run differs,
  because then the runs did not measure the same work;
- every median and p95 column lists the value of each run, "run 1 · run 2 · ...", followed by a
  column with the range of the runs (min–max);
- every other column (min, allocations, target, ...) lists the value of each run in the same way;
- the environment table shows a value once when all runs agree, and the value of each run otherwise
  (for example the source commit).
"""

import sys

BENCHMARKS = ('bench_StripChart', 'bench_AnalysisChart')

# Identifying columns of a row, by name
KEY_COLUMNS = ('Channels', 'Samples / ch', 'Signal', 'X axis', 'Case', 'Item')
# Columns that must be equal in every run (deterministic for a given benchmark source)
EXACT_COLUMNS = ('Trace px', 'Trace px 100k', 'Trace px 4M', 'New samples / ch / frame', 'Work per trial', 'Trials',
                 'Samples per channel in view', 'Decimated channels')
SEPARATOR = ' · '


def fail(message):
    sys.exit('merge_runs.py: ' + message)


def split_cells(line):
    return [c.strip() for c in line.strip().strip('|').split('|')]


def parse(path):
    """Return (benchmark name, sections); a section is a dict with heading, pre, header, align, rows."""
    text = open(path, encoding='utf-8').read()
    found = [b for b in BENCHMARKS if f'<!-- {b} results: begin -->' in text and f'<!-- {b} results: end -->' in text]
    if len(found) != 1:
        fail(f'{path}: no bench_StripChart or bench_AnalysisChart results block')
    bench = found[0]
    body = text.split(f'<!-- {bench} results: begin -->', 1)[1].split(f'<!-- {bench} results: end -->', 1)[0]
    sections = []
    current = None
    for line in body.splitlines():
        if line.startswith('#### '):
            current = {'heading': line, 'pre': [], 'header': None, 'align': None, 'rows': []}
            sections.append(current)
        elif current is None:
            continue
        elif line.startswith('|'):
            if current['header'] is None:
                current['header'] = split_cells(line)
            elif current['align'] is None:
                current['align'] = split_cells(line)
            else:
                current['rows'].append(split_cells(line))
        elif line.strip() and current['header'] is None:
            current['pre'].append(line)
    return bench, sections


def kind(name):
    if name in KEY_COLUMNS:
        return 'key'
    if name in EXACT_COLUMNS:
        return 'exact'
    lower = name.lower()
    if 'median' in lower or 'p95' in lower:
        return 'stat'
    return 'other'


NOT_APPLICABLE = ('–', '-', 'n/a')


def value_range(values, where):
    """min–max of the runs, with the decimals of the inputs; '–' when no run has a value."""
    present = [v for v in values if v not in NOT_APPLICABLE]
    if not present:
        return '–'
    if len(present) != len(values):
        fail(f'{where}: some runs have a value and some do not ({", ".join(values)})')
    try:
        nums = [float(v) for v in present]
    except ValueError:
        fail(f'{where}: not a number ({", ".join(values)})')
    decimals = max(len(v.split('.')[1]) if '.' in v else 0 for v in present)
    return f'{min(nums):.{decimals}f}–{max(nums):.{decimals}f}'


def row_line(cells):
    return '| ' + ' | '.join(cells) + ' |'


def merge_item_table(section, others):
    """Item / Value tables (environment, setChannelData): one value when all runs agree."""
    rows = []
    for i, r in enumerate(section['rows']):
        values = [r[1]] + [o['rows'][i][1] for o in others]
        if all(v == values[0] for v in values):
            rows.append([r[0], values[0]])
        else:
            rows.append([r[0], '; '.join(f'run {n + 1}: {v}' for n, v in enumerate(values))])
    return section['header'], ['---', '---'], rows


def merge_measure_table(section, others):
    header = section['header']
    kinds = [kind(name) for name in header]
    runs_label = f'runs 1–{len(others) + 1}'
    new_header = []
    new_align = []
    for name, k, a in zip(header, kinds, section['align']):
        if k in ('key', 'exact'):
            new_header.append(name)
            new_align.append(a)
        elif k == 'stat':
            new_header += [f'{name}, {runs_label}', f'{name}, range']
            new_align += ['---:', '---:']
        else:
            new_header.append(f'{name}, {runs_label}')
            new_align.append('---:')
    rows = []
    for i, r in enumerate(section['rows']):
        runs = [r] + [o['rows'][i] for o in others]
        cells = []
        for c, (name, k) in enumerate(zip(header, kinds)):
            values = [run[c] for run in runs]
            where = f'{section["heading"][5:]} / {r[0]} / {name}'
            if k == 'key':
                cells.append(values[0])
            elif k == 'exact':
                if any(v != values[0] for v in values):
                    fail(f'{where}: differs between runs ({", ".join(values)}); the runs did not measure the same work')
                cells.append(values[0])
            elif k == 'stat':
                cells.append(SEPARATOR.join(values))
                cells.append(value_range(values, where))
            else:
                cells.append(SEPARATOR.join(values))
        rows.append(cells)
    return new_header, new_align, rows


def main(argv):
    # The tables are UTF-8 (≤, –, ·), whatever the console encoding (cp1252 on Windows)
    sys.stdout.reconfigure(encoding='utf-8')
    if len(argv) < 3:
        fail('usage: merge_runs.py run1.md run2.md [run3.md ...] (at least two runs)')
    parsed = [parse(path) for path in argv[1:]]
    benches = {b for b, _ in parsed}
    if len(benches) != 1:
        fail('the runs come from different benchmarks: ' + ', '.join(sorted(benches)))
    bench = parsed[0][0]
    first = parsed[0][1]
    for path, (_, sections) in zip(argv[2:], parsed[1:]):
        if [s['heading'] for s in sections] != [s['heading'] for s in first]:
            fail(f'{path}: its tables differ from those of {argv[1]}')
        for s0, s in zip(first, sections):
            if s['header'] != s0['header']:
                fail(f'{path}: table header differs: {s0["heading"]}')
            keys0 = [[c for c, n in zip(r, s0['header']) if n in KEY_COLUMNS] for r in s0['rows']]
            keys = [[c for c, n in zip(r, s['header']) if n in KEY_COLUMNS] for r in s['rows']]
            if keys != keys0:
                fail(f'{path}: the rows of "{s0["heading"][5:]}" differ from those of {argv[1]}')

    out = [f'<!-- {bench} results: begin -->', '',
           f'Merged with `benchmarks/merge_runs.py` from {len(parsed)} runs '
           f'({", ".join(f"run {n + 1}: {p}" for n, p in enumerate(argv[1:]))}).', '']
    for i, section in enumerate(first):
        others = [sections[i] for _, sections in parsed[1:]]
        out.append(section['heading'])
        out.append('')
        for line in section['pre']:
            out.append(line)
            out.append('')
        if section['header'] and section['header'][0] == 'Item':
            header, align, rows = merge_item_table(section, others)
        else:
            header, align, rows = merge_measure_table(section, others)
        out.append(row_line(header))
        out.append(row_line(align))
        out += [row_line(r) for r in rows]
        out.append('')
    out.append(f'<!-- {bench} results: end -->')
    print('\n'.join(out))


if __name__ == '__main__':
    main(sys.argv)
