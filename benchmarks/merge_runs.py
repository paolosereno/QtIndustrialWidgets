#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Merge two bench_StripChart result files into one set of Markdown tables.

Usage: merge_runs.py run1.md run2.md > merged.md

Every table keeps the values of run 1 and gains a column with the range (min-max) of the
medians of both runs, so the run-to-run variation is published next to each value.
The trace pixel counts, chunk sizes and trial counts must be identical in both runs, otherwise
the script stops: both runs must have measured the same work. Allocation counts are taken from
run 1.
"""

import sys

BEGIN = '<!-- bench_StripChart results: begin -->'
END = '<!-- bench_StripChart results: end -->'

# Columns that identify a row (everything before the first measured column).
KEY_COLUMNS = {
    'Channels': 4,  # steady, streaming and allocation tables: channels, samples, signal, axis
    'Case': 3,      # resize and insertion tables: case, work per trial, trials
    'Item': 1,      # environment table
}
# Columns that must be equal in both runs (deterministic for a given benchmark source).
EXACT_COLUMNS = ('Trace px', 'New samples / ch / frame', 'Work per trial', 'Trials')


def split_cells(line):
    return [c.strip() for c in line.strip().strip('|').split('|')]


def parse(path):
    """Return a list of sections: (heading, preamble lines, header cells, align line, rows)."""
    text = open(path, encoding='utf-8').read()
    if BEGIN not in text or END not in text:
        sys.exit(f'{path}: no bench_StripChart results block')
    body = text.split(BEGIN, 1)[1].split(END, 1)[0]
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
                current['align'] = line
            else:
                current['rows'].append(split_cells(line))
        elif line.strip() and current['header'] is None:
            current['pre'].append(line)
    return sections


def key_of(header, row):
    return tuple(row[:KEY_COLUMNS[header[0]]])


def median_column(header):
    for i, name in enumerate(header):
        if name.lower().startswith('median') or name.lower() == 'render median':
            return i
    return None


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    run1, run2 = parse(sys.argv[1]), parse(sys.argv[2])
    if [s['heading'] for s in run1] != [s['heading'] for s in run2]:
        sys.exit('the two runs contain different tables')

    out = [BEGIN, '']
    for s1, s2 in zip(run1, run2):
        header = s1['header']
        if header != s2['header']:
            sys.exit(f'table header differs: {s1["heading"]}')
        rows2 = {key_of(header, r): r for r in s2['rows']}
        out.append(s1['heading'])
        out.append('')
        for line in s1['pre']:
            out.append(line)
            out.append('')

        if header[0] == 'Item':
            # Environment: run 1 values; flag rows that differ in run 2 (e.g. the commit).
            out.append('| ' + ' | '.join(header) + ' |')
            out.append(s1['align'])
            for r in s1['rows']:
                other = rows2.get(key_of(header, r))
                if other is not None and other != r:
                    r = r[:-1] + [f'{r[-1]} (run 2: {other[-1]})']
                out.append('| ' + ' | '.join(r) + ' |')
            out.append('')
            continue

        # Allocation counts are reported from run 1 only (no median column): they can differ by a
        # few Qt-internal allocations between processes.
        exact = [i for i, name in enumerate(header) if name in EXACT_COLUMNS]
        med = median_column(header)
        new_header = list(header)
        if med is not None:
            new_header.append('Median, 2 runs (min–max)')
        out.append('| ' + ' | '.join(new_header) + ' |')
        out.append(s1['align'] + ('---:|' if med is not None else ''))
        for r in s1['rows']:
            k = key_of(header, r)
            other = rows2.get(k)
            if other is None:
                sys.exit(f'row missing in run 2: {k}')
            for i in exact:
                if r[i] != other[i]:
                    sys.exit(f'{header[i]} differs between runs for {k}: {r[i]} vs {other[i]}')
            cells = list(r)
            if med is not None:
                a, b = float(r[med]), float(other[med])
                cells.append(f'{min(a, b):.2f}–{max(a, b):.2f}')
            out.append('| ' + ' | '.join(cells) + ' |')
        out.append('')
    out.append(END)
    print('\n'.join(out))


if __name__ == '__main__':
    main()
