# -*- coding: utf-8 -*-
import os, re, io, collections

root = '.'
inc_re = re.compile(r'#include\s+"([\w/]+\.h)"')

# map header filename -> folder
header_folder = {}
for dirpath, dirs, files in os.walk(root):
    folder = os.path.relpath(dirpath, root).replace(os.sep, '/')
    for f in files:
        if f.endswith('.h'):
            header_folder[f] = folder

# folder -> counter of target folders it depends on (via local includes)
dep = collections.defaultdict(lambda: collections.Counter())
for dirpath, dirs, files in os.walk(root):
    src_folder = os.path.relpath(dirpath, root).replace(os.sep, '/')
    if src_folder == '.':
        continue
    for f in files:
        if not (f.endswith('.h') or f.endswith('.cpp')):
            continue
        path = os.path.join(dirpath, f)
        try:
            txt = open(path, encoding='utf-8', errors='ignore').read()
        except Exception:
            continue
        for m in inc_re.finditer(txt):
            h = m.group(1).split('/')[-1]
            if h in header_folder:
                tgt = header_folder[h]
                if tgt != src_folder:
                    dep[src_folder][tgt] += 1

out = io.open('_deps.txt', 'w', encoding='utf-8')
for src in sorted(dep):
    deps = [f'{t}({c})' for t, c in sorted(dep[src].items(), key=lambda x: -x[1])]
    out.write(f'{src}  ->  {"  ".join(deps)}\n')
out.close()
print('done')
