#!/usr/bin/env python3
"""
Theme analyzer for nanocut-control.

Checks contrast ratios, color harmony, surface hierarchy, and interactive state
progression across all themes. Supports CAD/CAM-specific checks (path visibility
on cuttable plane) and standard UI readability checks.

Usage:
    python scripts/analyze_themes.py                  # Basic path contrast analysis
    python scripts/analyze_themes.py --detailed        # Full quality analysis per theme
    python scripts/analyze_themes.py --summary         # Cross-theme diversity report
    python scripts/analyze_themes.py --fix-suggestions # Suggest fixes for contrast failures
"""

import argparse
import json
import math
import colorsys
from pathlib import Path


# ---------------------------------------------------------------------------
# Color utilities
# ---------------------------------------------------------------------------

def rgb_to_hex(rgb):
    """Convert RGB values (0-1) to hex string."""
    r, g, b = rgb[:3]
    return f"#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}"


def relative_luminance(rgb):
    """Compute relative luminance per WCAG 2.1 (sRGB input, 0-1 range)."""
    def linearize(c):
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = rgb[:3]
    return 0.2126 * linearize(r) + 0.7152 * linearize(g) + 0.0722 * linearize(b)


def contrast_ratio(rgb1, rgb2):
    """WCAG 2.1 contrast ratio between two RGB colors (0-1 range)."""
    l1 = relative_luminance(rgb1)
    l2 = relative_luminance(rgb2)
    lighter = max(l1, l2)
    darker = min(l1, l2)
    return (lighter + 0.05) / (darker + 0.05)


def composite_over(fg_rgba, bg_rgb):
    """Alpha-composite *fg* over *bg* and return opaque RGB tuple."""
    a = fg_rgba[3] if len(fg_rgba) > 3 else 1.0
    return tuple(fg_rgba[i] * a + bg_rgb[i] * (1 - a) for i in range(3))


def rgb_to_hsl(rgb):
    """Return (H 0-360, S 0-100, L 0-100) from RGB 0-1."""
    h, l, s = colorsys.rgb_to_hls(rgb[0], rgb[1], rgb[2])
    return (h * 360, s * 100, l * 100)


def hsl_to_rgb(h, s, l):
    """Return RGB 0-1 tuple from H 0-360, S 0-100, L 0-100."""
    r, g, b = colorsys.hls_to_rgb(h / 360, l / 100, s / 100)
    return (r, g, b)


def is_light_theme(window_bg):
    """True if WindowBg luminance > 0.18 (light theme)."""
    return relative_luminance(window_bg) > 0.18


# ---------------------------------------------------------------------------
# Contrast rating helpers
# ---------------------------------------------------------------------------

def rate_path_contrast(ratio, element_type="main"):
    """
    Rate contrast for CAD/CAM path elements (permissive thresholds).

    Returns (emoji_label, category).
    """
    thresholds = {
        "main":      [(4.5, "Excellent"), (3.0, "Good"), (2.0, "Acceptable")],
        "secondary": [(3.5, "Excellent"), (2.5, "Good"), (1.8, "Acceptable")],
        "highlight": [(4.0, "Excellent"), (3.0, "Good"), (2.0, "Acceptable")],
    }
    for min_ratio, label in thresholds.get(element_type, thresholds["main"]):
        if ratio >= min_ratio:
            icon = "\u2705"
            return f"{icon} {label}", label.lower()
    return "\u274c Needs improvement", "poor"


def rate_ui_contrast(ratio, target):
    """Rate contrast against a numeric target. Returns (label, pass/fail)."""
    if ratio >= target:
        return f"\u2705 {ratio:.2f}:1 (>= {target}:1)", True
    return f"\u274c {ratio:.2f}:1 (need {target}:1)", False


# ---------------------------------------------------------------------------
# Fix suggestion helpers
# ---------------------------------------------------------------------------

def suggest_fix(fg_rgb, bg_rgb, target_ratio, role_name):
    """Suggest an adjusted foreground color that meets *target_ratio* vs *bg*."""
    bg_lum = relative_luminance(bg_rgb)
    h, s, l = rgb_to_hsl(fg_rgb)

    if bg_lum < 0.18:
        for new_l in range(int(l), 101):
            candidate = hsl_to_rgb(h, s, new_l)
            if contrast_ratio(candidate, bg_rgb) >= target_ratio:
                return candidate
    else:
        for new_l in range(int(l), -1, -1):
            candidate = hsl_to_rgb(h, s, new_l)
            if contrast_ratio(candidate, bg_rgb) >= target_ratio:
                return candidate
    return None


# ---------------------------------------------------------------------------
# Core analysis
# ---------------------------------------------------------------------------

def load_theme(path):
    with open(path) as f:
        return json.load(f)


def get_color(theme, key, fallback=None):
    """Get an ImGui color, return first 3 components (RGB)."""
    c = theme["imgui_colors"].get(key, fallback)
    if c is None:
        return None
    return c[:3]


def get_color_rgba(theme, key, fallback=None):
    """Get an ImGui color, return all 4 components (RGBA)."""
    c = theme["imgui_colors"].get(key, fallback)
    if c is None:
        return None
    if len(c) == 3:
        return list(c) + [1.0]
    return c


def analyze_path_contrast(theme):
    """Original path-vs-plane contrast analysis."""
    cuttable = theme["app_colors"]["cuttable_plane_color"]
    machine = theme["app_colors"]["machine_plane_color"]

    path_keys = {
        "Outside Contour (Text)": ("Text", "main"),
        "Inside Contour (TextDisabled)": ("TextDisabled", "secondary"),
        "Open Contour (DragDropTarget)": ("DragDropTarget", "highlight"),
        "Mouse Over (HeaderHovered)": ("HeaderHovered", "highlight"),
    }

    checks = []
    for label, (key, etype) in path_keys.items():
        fg_rgba = get_color_rgba(theme, key)
        fg = fg_rgba[:3]
        if fg_rgba[3] < 1.0:
            fg = composite_over(fg_rgba, cuttable)

        ratio = contrast_ratio(fg, cuttable)
        rating, cat = rate_path_contrast(ratio, etype)
        checks.append({
            "label": f"{label} vs Cuttable Plane",
            "ratio": ratio,
            "rating": rating,
            "category": cat,
            "fg": fg,
            "bg": cuttable,
        })

    plane_ratio = contrast_ratio(cuttable, machine)
    return checks, plane_ratio


def analyze_ui_text(theme):
    """UI text readability checks."""
    window_bg = get_color(theme, "WindowBg")
    frame_bg = get_color(theme, "FrameBg")
    text = get_color(theme, "Text")
    text_dis = get_color(theme, "TextDisabled")
    button_rgba = get_color_rgba(theme, "Button")
    button_eff = composite_over(button_rgba, window_bg)

    checks = []
    for label, fg, bg, target in [
        ("Text vs WindowBg", text, window_bg, 4.5),
        ("TextDisabled vs WindowBg", text_dis, window_bg, 3.0),
        ("Text vs FrameBg", text, frame_bg, 3.0),
        ("Text vs Button (composited)", text, button_eff, 3.0),
    ]:
        ratio = contrast_ratio(fg, bg)
        rating, passed = rate_ui_contrast(ratio, target)
        checks.append({
            "label": label,
            "ratio": ratio,
            "rating": rating,
            "passed": passed,
            "fg": fg,
            "bg": bg,
            "target": target,
        })
    return checks


def analyze_color_harmony(theme):
    """Informational color harmony analysis."""
    accent_keys = [
        "CheckMark", "SliderGrab", "ButtonHovered", "HeaderHovered",
        "DragDropTarget", "PlotLines", "PlotLinesHovered",
        "SeparatorHovered", "Tab", "TabActive",
    ]
    hues, sats, lights = [], [], []
    for key in accent_keys:
        c = get_color(theme, key)
        if c is None:
            continue
        h, s, l = rgb_to_hsl(c)
        if s >= 5:
            hues.append(h)
        sats.append(s)
        lights.append(l)

    surface_keys = ["WindowBg", "FrameBg", "PopupBg", "TitleBgActive", "MenuBarBg"]
    surface_lights = []
    for key in surface_keys:
        c = get_color(theme, key)
        if c:
            _, _, l = rgb_to_hsl(c)
            surface_lights.append(l)

    warnings = []
    if hues:
        hue_range = _circular_range(hues)
        if hue_range < 30:
            warnings.append(f"Accent hues cluster within {hue_range:.0f}\u00b0 (monotonous)")
    if sats and max(sats) < 30:
        warnings.append(f"All accent saturations < 30% (too muted, max={max(sats):.0f}%)")
    if surface_lights and max(surface_lights) < 20:
        warnings.append("All surfaces < 20% lightness (very dark)")
    if surface_lights and min(surface_lights) > 80:
        warnings.append("All surfaces > 80% lightness (very washed out)")

    return {
        "hues": hues,
        "saturations": sats,
        "lightnesses": lights,
        "surface_lightnesses": surface_lights,
        "warnings": warnings,
    }


def _circular_range(angles):
    """Minimum arc that contains all angles (degrees 0-360)."""
    if len(angles) <= 1:
        return 0
    sorted_a = sorted(a % 360 for a in angles)
    gaps = [sorted_a[i + 1] - sorted_a[i] for i in range(len(sorted_a) - 1)]
    gaps.append(360 - sorted_a[-1] + sorted_a[0])
    return 360 - max(gaps)


def analyze_surface_hierarchy(theme):
    """Check that surfaces are distinguishable."""
    window_bg = get_color(theme, "WindowBg")
    checks = []
    for key, target in [
        ("FrameBg", 1.1),
        ("TitleBgActive", 1.1),
        ("PopupBg", 1.05),
    ]:
        c = get_color(theme, key)
        if c is None:
            continue
        c_rgba = get_color_rgba(theme, key)
        if c_rgba[3] < 1.0:
            c = composite_over(c_rgba, window_bg)
        ratio = contrast_ratio(c, window_bg)
        passed = ratio >= target
        checks.append({
            "label": f"{key} vs WindowBg",
            "ratio": ratio,
            "target": target,
            "passed": passed,
        })
    return checks


def analyze_state_progression(theme):
    """Check Normal -> Hovered -> Active gets progressively more prominent."""
    window_bg = get_color(theme, "WindowBg")
    groups = {
        "Button": ["Button", "ButtonHovered", "ButtonActive"],
        "Header": ["Header", "HeaderHovered", "HeaderActive"],
        "Tab": ["Tab", "TabHovered", "TabActive"],
    }
    results = []
    for group_name, keys in groups.items():
        colors = []
        for k in keys:
            c_rgba = get_color_rgba(theme, k)
            if c_rgba is None:
                break
            c = composite_over(c_rgba, window_bg) if c_rgba[3] < 1.0 else c_rgba[:3]
            colors.append(c)
        if len(colors) != 3:
            continue

        contrasts = [contrast_ratio(c, window_bg) for c in colors]
        progressive = contrasts[0] <= contrasts[1] <= contrasts[2]
        near_progressive = all(
            contrasts[i] <= contrasts[i + 1] + 0.05 for i in range(2)
        )
        results.append({
            "group": group_name,
            "contrasts": contrasts,
            "progressive": progressive,
            "near_progressive": near_progressive,
        })
    return results


# ---------------------------------------------------------------------------
# Printing helpers
# ---------------------------------------------------------------------------

def print_path_analysis(name, path_checks, plane_ratio, overall_rating):
    print(f"\n\U0001f3a8 {name} {overall_rating}")
    print(f"   \U0001f4ca Path Contrast vs Cuttable Plane:")
    for c in path_checks:
        print(f"      {c['label']}: {c['ratio']:.2f}:1 {c['rating']}")
    print(f"   Plane separation (cuttable vs machine): {plane_ratio:.2f}:1")


def print_ui_text(checks):
    print("   \U0001f4d6 UI Text Readability:")
    for c in checks:
        print(f"      {c['label']}: {c['rating']}")


def print_harmony(harmony):
    if harmony["warnings"]:
        print("   \U0001f3a8 Color Harmony Warnings:")
        for w in harmony["warnings"]:
            print(f"      \u26a0\ufe0f  {w}")
    else:
        print("   \U0001f3a8 Color Harmony: OK")
    if harmony["hues"]:
        hue_range = _circular_range(harmony["hues"])
        print(f"      Accent hue spread: {hue_range:.0f}\u00b0")
    if harmony["saturations"]:
        print(f"      Saturation range: {min(harmony['saturations']):.0f}%-{max(harmony['saturations']):.0f}%")
    if harmony["surface_lightnesses"]:
        print(f"      Surface lightness range: {min(harmony['surface_lightnesses']):.0f}%-{max(harmony['surface_lightnesses']):.0f}%")


def print_surface_hierarchy(checks):
    print("   \U0001f3d7\ufe0f  Surface Hierarchy:")
    for c in checks:
        icon = "\u2705" if c["passed"] else "\u274c"
        print(f"      {icon} {c['label']}: {c['ratio']:.2f}:1 (need {c['target']}:1)")


def print_state_progression(results):
    print("   \U0001f579\ufe0f  Interactive State Progression:")
    for r in results:
        icon = "\u2705" if r["near_progressive"] else "\u274c"
        c = r["contrasts"]
        print(f"      {icon} {r['group']}: Normal={c[0]:.2f} Hovered={c[1]:.2f} Active={c[2]:.2f}")


def print_fix_suggestions(name, path_checks, ui_checks):
    """Print concrete color fix suggestions for failing checks."""
    suggestions = []

    for c in path_checks:
        if c["category"] == "poor":
            if "Outside" in c["label"]:
                t = 3.0
            elif "Inside" in c["label"]:
                t = 2.5
            else:
                t = 3.0
            fix = suggest_fix(c["fg"], c["bg"], t, c["label"])
            if fix:
                suggestions.append((c["label"], c["fg"], fix, c["bg"], t))

    for c in ui_checks:
        if not c["passed"]:
            fix = suggest_fix(c["fg"], c["bg"], c["target"], c["label"])
            if fix:
                suggestions.append((c["label"], c["fg"], fix, c["bg"], c["target"]))

    if suggestions:
        print(f"   \U0001f527 Fix Suggestions for {name}:")
        for label, old, new, bg, target in suggestions:
            old_hex = rgb_to_hex(old)
            new_hex = rgb_to_hex(new)
            new_ratio = contrast_ratio(new, bg)
            print(f"      {label}:")
            print(f"        Current: {old_hex} -> Suggested: {new_hex} (would give {new_ratio:.2f}:1, target {target}:1)")


# ---------------------------------------------------------------------------
# Overall theme rating
# ---------------------------------------------------------------------------

def compute_overall_rating(path_checks):
    cats = [c["category"] for c in path_checks]
    if all(c in ("excellent", "good") for c in cats):
        return "\u2705 Excellent - All paths clearly visible"
    if sum(1 for c in cats if c in ("excellent", "good")) >= 3:
        return "\u2705 Good - Most paths clearly visible"
    if "poor" not in cats:
        return "\u26a0\ufe0f  Acceptable - Paths visible but could be better"
    return "\u274c Needs improvement - Some paths may be hard to see"


# ---------------------------------------------------------------------------
# Diversity report (cross-theme)
# ---------------------------------------------------------------------------

def print_diversity_report(all_results):
    print("\n" + "=" * 100)
    print("\U0001f30d THEME DIVERSITY REPORT")
    print("=" * 100)

    print("\n\U0001f4ca WindowBg Lightness Distribution:")
    light_themes = []
    medium_themes = []
    dark_themes = []
    for r in all_results:
        _, _, l = rgb_to_hsl(r["window_bg"])
        if l > 60:
            light_themes.append(r["name"])
            tag = "(light)"
        elif l > 30:
            medium_themes.append(r["name"])
            tag = "(medium)"
        else:
            dark_themes.append(r["name"])
            tag = "(dark)"
        bar = "\u2588" * int(l / 2)
        print(f"   {r['name']:25s} L={l:5.1f}% {bar} {tag}")

    print(f"\n   Dark: {len(dark_themes)}  Medium: {len(medium_themes)}  Light: {len(light_themes)}")

    print("\n\U0001f308 Primary Accent Hue Distribution:")
    hue_buckets = {"Red (0-30)": [], "Orange (30-60)": [], "Yellow (60-90)": [],
                   "Lime (90-150)": [], "Green (150-180)": [], "Cyan (180-210)": [],
                   "Blue (210-270)": [], "Purple (270-330)": [], "Pink (330-360)": []}
    bucket_ranges = [(0, 30), (30, 60), (60, 90), (90, 150), (150, 180),
                     (180, 210), (210, 270), (270, 330), (330, 360)]
    bucket_names = list(hue_buckets.keys())

    for r in all_results:
        if r["accent_hue"] is not None:
            h = r["accent_hue"] % 360
            for i, (lo, hi) in enumerate(bucket_ranges):
                if lo <= h < hi:
                    hue_buckets[bucket_names[i]].append(r["name"])
                    break

    for bucket, themes in hue_buckets.items():
        count = len(themes)
        bar = "\u2588" * count
        names = ", ".join(themes) if themes else "-"
        print(f"   {bucket:20s} {bar:10s} ({count}) {names}")

    warm_count = sum(1 for r in all_results if r.get("accent_hue") is not None and (r["accent_hue"] < 60 or r["accent_hue"] > 330))
    cool_count = sum(1 for r in all_results if r.get("accent_hue") is not None and 180 <= r["accent_hue"] <= 330)

    print("\n\U0001f50d Identified Gaps:")
    if len(light_themes) == 0:
        print("   \u26a0\ufe0f  No light themes")
    elif len(light_themes) < 3:
        print(f"   \u26a0\ufe0f  Only {len(light_themes)} light theme(s) - consider adding more")
    if len(medium_themes) == 0:
        print("   \u26a0\ufe0f  No medium-lightness themes")
    for bucket, themes in hue_buckets.items():
        if not themes:
            print(f"   \u2139\ufe0f  No themes with {bucket.lower()} accent")
    if warm_count == 0:
        print("   \u26a0\ufe0f  No warm-toned themes (red/orange/yellow accents)")
    if cool_count == 0:
        print("   \u26a0\ufe0f  No cool-toned themes (cyan/blue/purple accents)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Analyze nanocut-control themes")
    parser.add_argument("--detailed", action="store_true",
                        help="Full quality analysis per theme")
    parser.add_argument("--summary", action="store_true",
                        help="Cross-theme diversity report")
    parser.add_argument("--fix-suggestions", action="store_true",
                        help="Suggest concrete color fixes for contrast failures")
    parser.add_argument("themes", nargs="*",
                        help="Specific theme files to analyze (default: all)")
    args = parser.parse_args()

    themes_dir = Path("assets/themes")
    if args.themes:
        theme_files = [Path(t) for t in args.themes]
    else:
        theme_files = sorted(themes_dir.glob("*.json"))

    total = len(theme_files)
    print(f"Analyzing {total} themes")
    print("=" * 100)

    all_results = []
    any_failure = False

    for theme_file in theme_files:
        theme = load_theme(theme_file)
        name = theme["name"]
        window_bg = get_color(theme, "WindowBg")
        light = is_light_theme(window_bg)

        # --- Path contrast (always shown) ---
        path_checks, plane_ratio = analyze_path_contrast(theme)
        overall = compute_overall_rating(path_checks)
        print_path_analysis(name, path_checks, plane_ratio, overall)

        if any(c["category"] == "poor" for c in path_checks):
            any_failure = True

        # --- Detailed checks ---
        ui_checks = analyze_ui_text(theme)
        harmony = analyze_color_harmony(theme)
        surface = analyze_surface_hierarchy(theme)
        states = analyze_state_progression(theme)

        if args.detailed:
            mode = "Light" if light else "Dark"
            print(f"   Mode: {mode}")
            print_ui_text(ui_checks)
            print_harmony(harmony)
            print_surface_hierarchy(surface)
            print_state_progression(states)

        if args.fix_suggestions:
            print_fix_suggestions(name, path_checks, ui_checks)

        if any(not c["passed"] for c in ui_checks):
            any_failure = True

        # Determine primary accent hue
        accent_hue = None
        for key in ("CheckMark", "SliderGrab", "ButtonHovered"):
            c = get_color(theme, key)
            if c:
                h, s, l = rgb_to_hsl(c)
                if s > 10:
                    accent_hue = h
                    break

        all_results.append({
            "name": name,
            "window_bg": window_bg,
            "light": light,
            "accent_hue": accent_hue,
            "path_checks": path_checks,
            "ui_checks": ui_checks,
            "harmony": harmony,
            "overall": overall,
        })

    # --- Summary section (always shown) ---
    print("\n" + "=" * 100)
    print("\U0001f4cb SUMMARY")
    print("=" * 100)

    cats = {"Excellent": [], "Good": [], "Acceptable": [], "Needs improvement": []}
    for r in all_results:
        for cat_key in cats:
            if cat_key in r["overall"]:
                cats[cat_key].append(r["name"])
                break

    for cat_key, themes in cats.items():
        if cat_key in ("Excellent", "Good"):
            icon = "\u2705"
        elif cat_key == "Acceptable":
            icon = "\u26a0\ufe0f "
        else:
            icon = "\u274c"
        print(f"\n{icon} {cat_key.upper()} ({len(themes)}/{total}):")
        for t in themes:
            print(f"   \U0001f3a8 {t}")

    # --- Diversity report ---
    if args.summary:
        print_diversity_report(all_results)

    print()


if __name__ == "__main__":
    main()
