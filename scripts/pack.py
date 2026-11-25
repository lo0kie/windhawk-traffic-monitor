#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 src/ 下的分片合并成 Windhawk 需要的单文件 .wh.cpp。

Windhawk 只接受单文件源码，所以开发时按模块拆分、发布时合并成可读单文件：

    python scripts/pack.py                       # dist/windhawk-traffic-monitor.wh.cpp

也可以被 build.py 当模块调用（pack_file），这样构建脚本不用起子进程。
"""

from __future__ import print_function

import argparse
import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

META_START = '// ==WindhawkMod=='

INCLUDE_RE = re.compile(r'^\s*#include\s+"([^"]+)"\s*$')


# ---------------------------------------------------------------------------
# 合并
# ---------------------------------------------------------------------------
def amalgamate(path, seen, out, depth=0):
    path = os.path.normpath(path)
    base = os.path.dirname(path)
    text = io.open(path, encoding='utf-8').read()
    for line in text.split('\n'):
        m = INCLUDE_RE.match(line)
        if m:
            target = os.path.normpath(os.path.join(base, m.group(1)))
            if os.path.exists(target):
                if target in seen:
                    continue
                seen.add(target)
                amalgamate(target, seen, out, depth + 1)
                continue
        if line.strip() == '#pragma once':
            continue
        out.append(line)


# ---------------------------------------------------------------------------
# 去预览代码（保留可读格式，只删 #ifdef TM_PREVIEW 的调试块）
# ---------------------------------------------------------------------------
_DIRECTIVE_RE = re.compile(r'^#\s*(if|ifdef|ifndef|elif|else|endif)\b')


def _match_endif(lines, start):
    """返回 (#else / #elif 行号或 None, #endif 行号)。"""
    depth = 0
    branch = None
    for i in range(start, len(lines)):
        s = lines[i].strip()
        m = _DIRECTIVE_RE.match(s)
        if not m:
            continue
        kind = m.group(1)
        if kind in ('if', 'ifdef', 'ifndef'):
            depth += 1
        elif kind == 'endif':
            depth -= 1
            if depth == 0:
                return branch, i
        elif kind in ('else', 'elif') and depth == 1:
            branch = i
    raise ValueError('unbalanced #ifdef at line %d' % (start + 1))


def drop_preview(text):
    lines = text.split('\n')
    out = []
    i = 0
    while i < len(lines):
        s = lines[i].strip()
        if s.startswith('#ifdef TM_PREVIEW') or s.startswith('#ifndef TM_PREVIEW'):
            negative = s.startswith('#ifndef')
            branch, endif = _match_endif(lines, i)
            if negative:
                end = branch if branch is not None else endif
                out.extend(drop_preview('\n'.join(lines[i + 1:end])).split('\n'))
            elif branch is not None:
                out.extend(drop_preview('\n'.join(lines[branch + 1:endif])).split('\n'))
            i = endif + 1
            continue
        out.append(lines[i])
        i += 1
    return '\n'.join(out)


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def pack(root, drop_preview_on=False):
    out = []
    amalgamate(root, {os.path.normpath(root)}, out)
    text = '\n'.join(out)
    if drop_preview_on:
        text = drop_preview(text)
    text = text.rstrip('\n') + '\n'
    if not text.lstrip().startswith(META_START):
        raise SystemExit('产物没有以 // ==WindhawkMod== 开头，Windhawk 会拒绝加载')
    return text


def pack_file(src, out, quiet=False, drop_preview_on=False):
    """合并出单文件，返回 (行数, 字节数)。"""
    text = pack(os.path.abspath(src), drop_preview_on)

    out = os.path.abspath(out)
    out_dir = os.path.dirname(out)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    io.open(out, 'w', encoding='utf-8', newline='\n').write(text)

    lines = text.count('\n')
    size = len(text.encode('utf-8'))
    if not quiet:
        rel = os.path.relpath(out, ROOT).replace(os.sep, '/')
        print('%s  %d 行  %.1f KB' % (rel, lines, size / 1024.0))
    return lines, size


def main():
    parser = argparse.ArgumentParser(description='合并 Windhawk mod 源码为单文件')
    parser.add_argument('-r', '--root', default=os.path.join(ROOT, 'src', 'mod.wh.cpp'))
    parser.add_argument('-o', '--out', default=os.path.join(ROOT, 'dist',
                                                            'windhawk-traffic-monitor.wh.cpp'))
    args = parser.parse_args()

    pack_file(args.root, args.out)


if __name__ == '__main__':
    main()
