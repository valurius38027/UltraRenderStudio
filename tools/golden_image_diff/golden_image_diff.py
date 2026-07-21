#!/usr/bin/env python3
"""
渲染回归测试:比较一张新渲染出的截图和一张存档的"标准正确"(golden)截图。

超过每通道容许误差(--tolerance)的像素比例超过 --max-diff-ratio 就判定失败,
退出码非零,并在 --diff-out 指定路径写一张可视化 diff 图(差异区域标红)。

允许小的逐像素误差是必须的 —— 不同 GPU/驱动/后端(Vulkan vs D3D vs Metal)
在浮点光栅化上有细微但正常的差异,不能要求逐位精确相同,否则这个工具会
在每次驱动更新后都误报。

用法:
    python3 golden_image_diff.py --golden ref.png --actual out.png \
        --tolerance 4 --max-diff-ratio 0.001 --diff-out diff.png
"""
import argparse
import sys

try:
    from PIL import Image
except ImportError:
    print("error: 需要 Pillow (pip install pillow)", file=sys.stderr)
    sys.exit(2)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--golden", required=True, help="存档的标准正确截图")
    parser.add_argument("--actual", required=True, help="本次渲染产出的截图")
    parser.add_argument("--tolerance", type=int, default=4,
                         help="单通道容许的最大差值(0-255),默认4")
    parser.add_argument("--max-diff-ratio", type=float, default=0.001,
                         help="容许的最大超差像素比例,默认0.001(千分之一)")
    parser.add_argument("--diff-out", default=None, help="可视化diff图输出路径(可选)")
    args = parser.parse_args()

    golden = Image.open(args.golden).convert("RGBA")
    actual = Image.open(args.actual).convert("RGBA")

    if golden.size != actual.size:
        print(f"error: 尺寸不匹配 golden={golden.size} actual={actual.size}", file=sys.stderr)
        return 1

    width, height = golden.size
    golden_px = golden.load()
    actual_px = actual.load()

    diff_img = Image.new("RGBA", (width, height), (0, 0, 0, 0)) if args.diff_out else None
    diff_px = diff_img.load() if diff_img else None

    bad_pixel_count = 0
    total_pixels = width * height

    for y in range(height):
        for x in range(width):
            g = golden_px[x, y]
            a = actual_px[x, y]
            max_channel_diff = max(abs(g[c] - a[c]) for c in range(4))
            if max_channel_diff > args.tolerance:
                bad_pixel_count += 1
                if diff_px is not None:
                    diff_px[x, y] = (255, 0, 0, 255)
            elif diff_px is not None:
                diff_px[x, y] = (0, 0, 0, 0)

    diff_ratio = bad_pixel_count / total_pixels

    if diff_img is not None:
        diff_img.save(args.diff_out)

    print(f"超差像素: {bad_pixel_count}/{total_pixels} ({diff_ratio:.5f})")

    if diff_ratio > args.max_diff_ratio:
        print(f"FAIL: 超差比例 {diff_ratio:.5f} 超过阈值 {args.max_diff_ratio}", file=sys.stderr)
        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
