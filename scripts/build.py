#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""构建脚本：合并源码、编译、预览验证。

源码在 src/ 下按模块拆分，Windhawk 只接受单文件，所以先由 pack.py 摊平。
所有程序和中间产物都落在 .temp/ 下：

    .temp/build/   exe / dll / 打包用的中间单文件
    .temp/dump/    截图、渲染导出、preview.ini

    python scripts/build.py all       合并 + 语法检查 + 预览（日常用这个）
    python scripts/build.py pack      dist/windhawk-traffic-monitor.wh.cpp     可读单文件
    python scripts/build.py mod       直接编译 src/（DLL 链接检查，不打包）
    python scripts/build.py preview   编译可独立运行的预览程序
    python scripts/build.py clean     清掉 .temp/（dist/ 不动）

编译用 Windhawk 自带的 clang，不需要 Visual Studio。Windhawk 装在别处的话设
环境变量 WINDHAWK_HOME 指过去。
"""

from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
ROOT = SCRIPTS.parent
BUILD = ROOT / '.temp' / 'build'
DUMP = ROOT / '.temp' / 'dump'
DIST = ROOT / 'dist'
SRC = ROOT / 'src' / 'mod.wh.cpp'

sys.path.insert(0, str(SCRIPTS))
import pack  # noqa: E402  同目录的打包脚本，直接当模块用，不起子进程

WINDHAWK_HOME = Path(os.environ.get('WINDHAWK_HOME', r'C:\Program Files\Windhawk'))
CLANG = WINDHAWK_HOME / 'Compiler' / 'bin' / 'clang++.exe'
INCLUDE = WINDHAWK_HOME / 'Compiler' / 'include'

VERBOSE = False


def die(message):
    print('错误: %s' % message, file=sys.stderr)
    sys.exit(1)


def engine_lib_dir():
    """挑一个带 windhawk.lib 的引擎目录（版本号最大的那个）。"""
    engine = WINDHAWK_HOME / 'Engine'
    if not engine.is_dir():
        die('找不到 %s，Windhawk 装在哪？设 WINDHAWK_HOME 指过去' % engine)
    candidates = [d for d in engine.iterdir() if (d / '64' / 'windhawk.lib').is_file()]
    if not candidates:
        die('%s 下没有带 windhawk.lib 的版本目录' % engine)
    return sorted(candidates, key=lambda d: _version_key(d.name))[-1] / '64'


def _version_key(name):
    parts = []
    for piece in name.split('.'):
        parts.append(int(piece) if piece.isdigit() else 0)
    return parts


def base_flags():
    return [
        '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32',
        '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
        '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
        '-D__USE_MINGW_ANSI_STDIO=0', '-DWH_MOD', '-DWH_EDITING',
        '-Wno-pragma-pack', '-Wno-pragma-system-header-outside-header',
        '-I', str(INCLUDE), '-include', 'windhawk_api.h',
        '-static', '-O2', '-Wall', '-Wno-unused-variable',
        '-Wno-unused-but-set-variable',
    ]


def shared_libs():
    return ['-L', str(engine_lib_dir()), '-lwindhawk', '-lgdi32', '-ldwrite', '-lpdh',
            '-liphlpapi', '-lcomctl32', '-lshell32', '-ladvapi32', '-lole32']


# 预览程序不需要 windhawk 运行库：Windhawk 的 API 在 WH_EDITING 下是空实现
PREVIEW_LIBS = ['-lgdi32', '-ldwrite', '-lpdh', '-liphlpapi', '-lcomctl32', '-lshell32',
                '-ladvapi32', '-lole32']


def run(args, what):
    if VERBOSE:
        print('  ' + ' '.join(str(a) for a in args))
    result = subprocess.run([str(a) for a in args], cwd=str(ROOT),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode('utf-8', 'replace')
    if result.returncode != 0:
        print(output.rstrip(), file=sys.stderr)
        die('%s 失败（退出码 %d）' % (what, result.returncode))
    if output.strip() and VERBOSE:
        print(output.rstrip())
    return output


def check_toolchain():
    if not CLANG.is_file():
        die('找不到 %s，Windhawk 装在哪？设 WINDHAWK_HOME 指过去' % CLANG)


def write_harness(name, body):
    """预览用的 main() 只有几行，按需生成到 .temp/build 里，不额外维护源文件。"""
    path = BUILD / name
    path.write_text(body, encoding='utf-8', newline='\n')
    return path


def target_pack():
    out = DIST / 'windhawk-traffic-monitor.wh.cpp'
    # 上架产物：去掉 #ifdef TM_PREVIEW 的调试代码，保持可读格式
    pack.pack_file(SRC, out, drop_preview_on=True)
    print('OK: dist/windhawk-traffic-monitor.wh.cpp')


def target_mod():
    dll = BUILD / 'windhawk-traffic-monitor.dll'
    run([CLANG] + base_flags() + ['-shared', '-o', dll, SRC] + shared_libs(),
        '编译 src/')
    print('OK: .temp/build/windhawk-traffic-monitor.dll')


def target_preview():
    include_path = os.path.relpath(SRC, BUILD).replace(os.sep, '/')
    harness = write_harness('preview_harness.cpp', (
        '// 由 scripts/build.py 生成：把 mod 源码当独立程序跑起来\n'
        '#include <windows.h>\n'
        '#include "%s"\n'
        '\n'
        'int main() {\n'
        '    return PreviewMain();\n'
        '}\n') % include_path)
    exe = BUILD / 'preview.exe'
    run([CLANG] + base_flags() + ['-DTM_PREVIEW', '-o', exe, harness] + PREVIEW_LIBS,
        '编译预览程序')
    print('OK: .temp/build/preview.exe')


def target_all():
    target_pack()
    target_mod()
    target_preview()


def target_clean():
    if (ROOT / '.temp').exists():
        shutil.rmtree(ROOT / '.temp')
    print('OK: .temp/ 已清空')


TARGETS = {
    'pack': target_pack,
    'mod': target_mod,
    'preview': target_preview,
    'all': target_all,
    'clean': target_clean,
}


def main():
    global VERBOSE
    parser = argparse.ArgumentParser(
        description='构建 Windhawk mod（合并 / 编译 / 预览）')
    parser.add_argument('target', nargs='?', default='all',
                        choices=list(TARGETS) + ['help'])
    parser.add_argument('-v', '--verbose', action='store_true', help='打印完整编译命令行')
    args = parser.parse_args()
    VERBOSE = args.verbose

    if args.target == 'help':
        parser.print_help()
        return 0

    # pack 只做文本合并 + 去预览，不需要编译器；其余 target 才检查工具链
    if args.target not in ('clean', 'pack'):
        check_toolchain()
        BUILD.mkdir(parents=True, exist_ok=True)
        DUMP.mkdir(parents=True, exist_ok=True)
        DIST.mkdir(parents=True, exist_ok=True)

    TARGETS[args.target]()
    return 0


if __name__ == '__main__':
    sys.exit(main())
