#!/usr/bin/env python3
"""
依赖分层强制检查。

扫描 libs/<module>/{include,src}/**/*.{hpp,cpp} 里的 #include "ur/<other>/..."
或 #include <ur/<other>/...>,和下面的 ALLOWED_DEPS 表比对。任何不在白名单里
的跨模块 include 视为违规,退出码非零。CI 应该在每次 PR 上跑这个脚本。

用法: python3 cmake/module_dependency_lint.py [--repo-root PATH]
"""
import argparse
import re
import sys
from pathlib import Path

# 允许的依赖方向,对应架构讨论里定的分层图。键依赖值列表里的模块,
# 不能反过来,也不能有列表之外的模块。
ALLOWED_DEPS: dict[str, set[str]] = {
    "ur_platform": set(),
    "ur_gfx": {"ur_platform"},
    "ur_text": {"ur_platform", "ur_gfx"},
    "ur_widgets": {"ur_platform", "ur_gfx", "ur_text"},
    "ur_dock": {"ur_platform", "ur_gfx", "ur_text", "ur_widgets"},
    "ur_nodegraph": {"ur_platform", "ur_gfx", "ur_text", "ur_widgets"},
    "ur_viewport": {"ur_platform", "ur_gfx", "ur_text", "ur_widgets"},
    "ur_scene_bridge": {"ur_platform"},
}

INCLUDE_RE = re.compile(r'#include\s*[<"]ur/([a-zA-Z0-9_]+)/')
SOURCE_EXTS = {".hpp", ".h", ".cpp", ".cc", ".inl"}


def module_name_from_include_dir(dir_component: str) -> str | None:
    """include 路径里是 ur/<x>/..., <x> 对应模块名去掉 ur_ 前缀,这里转回全名。"""
    candidate = f"ur_{dir_component}"
    return candidate if candidate in ALLOWED_DEPS else None


def scan_module(libs_dir: Path, module: str) -> list[tuple[Path, str]]:
    violations = []
    module_dir = libs_dir / module
    if not module_dir.is_dir():
        return violations

    allowed = ALLOWED_DEPS[module]
    for path in module_dir.rglob("*"):
        if path.suffix not in SOURCE_EXTS or not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in INCLUDE_RE.finditer(text):
            dep = module_name_from_include_dir(match.group(1))
            if dep is None or dep == module:
                continue
            if dep not in allowed:
                violations.append((path, dep))
    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()

    libs_dir = args.repo_root / "libs"
    if not libs_dir.is_dir():
        print(f"error: libs/ 目录不存在于 {args.repo_root}", file=sys.stderr)
        return 2

    all_violations: list[tuple[Path, str]] = []
    for module in ALLOWED_DEPS:
        all_violations.extend(scan_module(libs_dir, module))

    if all_violations:
        print("依赖分层检查失败,发现违规 include:\n", file=sys.stderr)
        for path, dep in all_violations:
            print(f"  {path}: 引入了不允许的依赖 -> {dep}", file=sys.stderr)
        print(f"\n共 {len(all_violations)} 处违规。请检查 cmake/module_dependency_lint.py "
              f"里的 ALLOWED_DEPS 表是否需要更新,或者这是一次误引入。", file=sys.stderr)
        return 1

    print("依赖分层检查通过。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
