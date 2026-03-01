#!/usr/bin/env python3
"""
Generate a complete nanocut-control theme from seed parameters.

Derives all ImGui colors systematically from a small set of HSL-based inputs,
then validates the result for contrast and harmony.

Usage:
    python scripts/generate_theme.py --name "Solar Flare" --base-hue 30 --mode dark
    python scripts/generate_theme.py --name "Ocean" --base-hue 195 --mode light --accent-scheme triadic
    python scripts/generate_theme.py --name "Forest" --base-hue 120 --mode medium --saturation muted
"""

import argparse
import colorsys
import json
import subprocess
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Color utilities
# ---------------------------------------------------------------------------

def hsl(h, s, l):
    """HSL (H 0-360, S 0-100, L 0-100) -> RGB (0-1) tuple."""
    r, g, b = colorsys.hls_to_rgb((h % 360) / 360, l / 100, s / 100)
    return (round(r, 3), round(g, 3), round(b, 3))


def rgba(rgb, a=1.0):
    """RGB tuple + alpha -> 4-element list."""
    return [round(rgb[0], 3), round(rgb[1], 3), round(rgb[2], 3), round(a, 2)]


def relative_luminance(rgb):
    def lin(c):
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * lin(rgb[0]) + 0.7152 * lin(rgb[1]) + 0.0722 * lin(rgb[2])


def contrast_ratio(rgb1, rgb2):
    l1 = relative_luminance(rgb1)
    l2 = relative_luminance(rgb2)
    return (max(l1, l2) + 0.05) / (min(l1, l2) + 0.05)


def adjust_lightness_for_contrast(h, s, l, bg_rgb, min_ratio, direction):
    """Adjust lightness until contrast with bg meets min_ratio."""
    step = 1 if direction == "lighter" else -1
    for new_l in range(int(l), 101 if step > 0 else -1, step):
        candidate = hsl(h, s, new_l)
        if contrast_ratio(candidate, bg_rgb) >= min_ratio:
            return new_l
    return l


# ---------------------------------------------------------------------------
# Accent scheme generation
# ---------------------------------------------------------------------------

def accent_hues(base_hue, scheme):
    """Return list of accent hues from scheme name."""
    if scheme == "complementary":
        return [base_hue, (base_hue + 180) % 360]
    elif scheme == "analogous":
        return [base_hue, (base_hue + 30) % 360, (base_hue - 30) % 360]
    elif scheme == "triadic":
        return [base_hue, (base_hue + 120) % 360, (base_hue - 120) % 360]
    elif scheme == "split-complementary":
        return [base_hue, (base_hue + 150) % 360, (base_hue + 210) % 360]
    return [base_hue]


def apply_warmth(hue, warmth):
    """Shift hue 10 degrees toward the warm pole (orange, 30deg) or cool pole (blue, 210deg)."""
    if warmth == "neutral":
        return hue
    target = 30 if warmth == "warm" else 210
    # Signed shortest angular distance toward target
    diff = (target - hue + 180) % 360 - 180
    shift = min(10, abs(diff)) * (1 if diff >= 0 else -1)
    return (hue + shift) % 360


# ---------------------------------------------------------------------------
# Theme generation
# ---------------------------------------------------------------------------

def generate_theme(name, base_hue, mode, accent_scheme, saturation, warmth):
    base_hue = apply_warmth(base_hue, warmth)
    sat_levels = {"vibrant": 80, "moderate": 50, "muted": 30}
    sat = sat_levels[saturation]

    # Surface saturation scales with --saturation (vibrant tints surfaces more)
    surface_sat_scale = {"vibrant": 2.2, "moderate": 1.0, "muted": 0.5}[saturation]

    hues = accent_hues(base_hue, accent_scheme)
    primary_hue = hues[0]
    secondary_hue = hues[1] if len(hues) > 1 else (primary_hue + 180) % 360
    tertiary_hue = hues[2] if len(hues) > 2 else (primary_hue + 90) % 360

    # --- Background surfaces ---
    if mode == "light":
        bg_l = 94
        bg_s = int(8 * surface_sat_scale)
        frame_l = bg_l - 7
        title_l = bg_l - 10
        popup_l = bg_l - 5
        menu_l = bg_l - 6
    elif mode == "medium":
        bg_l = 40
        bg_s = int(18 * surface_sat_scale)
        frame_l = bg_l + 5
        title_l = bg_l - 5
        popup_l = bg_l + 3
        menu_l = bg_l - 4
    else:  # dark
        bg_l = 14
        bg_s = int(15 * surface_sat_scale)
        frame_l = bg_l + 6
        title_l = bg_l - 3
        popup_l = bg_l + 4
        menu_l = bg_l - 2

    window_bg = hsl(base_hue, bg_s, bg_l)
    child_bg = window_bg
    frame_bg = hsl(base_hue, bg_s, frame_l)
    title_bg = hsl(base_hue, bg_s, max(title_l, 2))
    title_bg_active = hsl(base_hue, bg_s, min(max(title_l + 10, 2), 98))
    title_bg_collapsed = hsl(base_hue, bg_s, max(title_l, 2))
    popup_bg = hsl(base_hue, bg_s, min(popup_l, 98))
    menu_bar_bg = hsl(base_hue, bg_s, max(menu_l, 2))

    # --- Text colors ---
    if mode == "light":
        text_l, text_dis_l = 10, 30
    elif mode == "medium":
        text_l, text_dis_l = 90, 65
    else:
        text_l, text_dis_l = 90, 60

    text_rgb = hsl(base_hue, 5, text_l)
    text_disabled_rgb = hsl(base_hue, 5, text_dis_l)

    # Ensure text contrast meets target
    text_cr = contrast_ratio(text_rgb, window_bg)
    if text_cr < 4.5:
        new_l = adjust_lightness_for_contrast(
            base_hue, 5, text_l, window_bg, 4.5,
            "darker" if mode == "light" else "lighter")
        text_rgb = hsl(base_hue, 5, new_l)

    text_dis_cr = contrast_ratio(text_disabled_rgb, window_bg)
    if text_dis_cr < 3.0:
        new_l = adjust_lightness_for_contrast(
            base_hue, 5, text_dis_l, window_bg, 3.0,
            "darker" if mode == "light" else "lighter")
        text_disabled_rgb = hsl(base_hue, 5, new_l)

    # --- Accent colors ---
    if mode == "light":
        accent_l = 45
        accent_hover_l = 38
        accent_active_l = 32
    elif mode == "medium":
        accent_l = 60
        accent_hover_l = 68
        accent_active_l = 75
    else:
        accent_l = 60
        accent_hover_l = 68
        accent_active_l = 75

    primary_accent = hsl(primary_hue, sat, accent_l)
    primary_hover = hsl(primary_hue, sat, accent_hover_l)
    primary_active = hsl(primary_hue, sat, accent_active_l)

    secondary_accent = hsl(secondary_hue, sat, accent_l)
    secondary_hover = hsl(secondary_hue, sat, accent_hover_l)
    secondary_active = hsl(secondary_hue, sat, accent_active_l)

    tertiary_accent = hsl(tertiary_hue, sat, accent_l)
    tertiary_hover = hsl(tertiary_hue, sat, accent_hover_l)
    tertiary_active = hsl(tertiary_hue, sat, accent_active_l)

    # --- Interactive element surfaces ---
    if mode == "light":
        # Buttons: tinted with primary accent at low saturation
        btn_l = 82
        btn_hover_l = 72
        btn_active_l = 62
    elif mode == "medium":
        btn_l = bg_l + 8
        btn_hover_l = bg_l + 14
        btn_active_l = bg_l + 20
    else:
        btn_l = bg_l + 8
        btn_hover_l = bg_l + 14
        btn_active_l = bg_l + 20

    # Saturation caps for interactive surfaces scale with --saturation
    btn_sat = {"vibrant": min(sat, 70), "moderate": min(sat, 45), "muted": min(sat, 30)}[saturation]
    btn_sat_hi = {"vibrant": min(sat, 80), "moderate": min(sat, 55), "muted": min(sat, 35)}[saturation]

    button = hsl(primary_hue, btn_sat, btn_l)
    button_hovered = hsl(primary_hue, btn_sat_hi, btn_hover_l)
    button_active = hsl(primary_hue, btn_sat_hi, btn_active_l)

    header = hsl(primary_hue, btn_sat, btn_l)
    if mode == "light":
        # Header hover must contrast with both WindowBg and cuttable plane
        # Use dark tertiary accent (opaque) for mouse-over path visibility
        header_hovered = hsl(tertiary_hue, btn_sat_hi, 30)
        header_active = hsl(tertiary_hue, btn_sat_hi, 22)
    else:
        header_hovered = hsl(tertiary_hue, btn_sat_hi, btn_hover_l)
        header_active = hsl(tertiary_hue, btn_sat_hi, btn_active_l)

    frame_bg_hovered = hsl(primary_hue, btn_sat, frame_l + 6)
    frame_bg_active = hsl(primary_hue, btn_sat, frame_l + 12)

    # --- Tabs (secondary hue for visual variety) ---
    tab = hsl(base_hue, bg_s + 3, bg_l - 3 if mode != "light" else bg_l - 6)
    tab_hovered = hsl(secondary_hue, btn_sat_hi, accent_l)
    tab_active = hsl(secondary_hue, btn_sat, btn_l)
    tab_unfocused = hsl(base_hue, bg_s, bg_l - 1 if mode != "light" else bg_l - 2)
    tab_unfocused_active = hsl(secondary_hue, min(btn_sat, 30), btn_l - 3 if mode != "light" else btn_l + 3)

    # --- Scrollbar ---
    scrollbar_bg = hsl(base_hue, bg_s, bg_l + 2 if mode != "light" else bg_l - 2)
    scrollbar_grab = hsl(base_hue, bg_s + 5, bg_l + 20 if mode != "light" else bg_l - 20)
    scrollbar_grab_hover = hsl(base_hue, bg_s + 5, bg_l + 30 if mode != "light" else bg_l - 30)
    scrollbar_grab_active = hsl(base_hue, bg_s + 5, bg_l + 35 if mode != "light" else bg_l - 35)

    # --- Borders ---
    if mode == "light":
        border_l = 70
    elif mode == "medium":
        border_l = 35
    else:
        border_l = 25
    border = hsl(base_hue, bg_s, border_l)

    # --- Separator (secondary hue) ---
    sep = hsl(base_hue, bg_s, border_l)
    sep_hovered = secondary_hover
    sep_active = secondary_active

    # --- Resize grip (secondary hue) ---
    resize = secondary_accent
    resize_hovered = secondary_hover
    resize_active = secondary_active

    # --- Plot colors (green for work coords, orange for abs coords) ---
    green_hue = 120
    orange_hue = 30
    plot_lines = hsl(green_hue, 70, 40 if mode == "light" else 55)
    plot_lines_hovered = hsl(orange_hue, 85, 45 if mode == "light" else 60)
    plot_histogram = hsl(45, 80, 45 if mode == "light" else 55)
    plot_histogram_hovered = hsl(25, 90, 45 if mode == "light" else 55)

    # --- Table ---
    table_header_bg = hsl(primary_hue, min(sat, 30), btn_l - 5 if mode == "light" else btn_l + 3)
    table_border_strong = hsl(base_hue, bg_s, border_l)
    table_border_light = hsl(base_hue, bg_s, border_l + 5 if mode != "light" else border_l - 5)

    # --- Cuttable & machine planes ---
    cut_s_scale = {"vibrant": 1.6, "moderate": 1.0, "muted": 0.7}[saturation]
    if mode == "light":
        cut_l = 55
        cut_s = int(35 * cut_s_scale)
        mach_l = 25
    elif mode == "medium":
        cut_l = 30
        cut_s = int(35 * cut_s_scale)
        mach_l = 15
    else:
        cut_l = 15
        cut_s = int(40 * cut_s_scale)
        mach_l = 6

    cuttable_plane = hsl(base_hue, cut_s, cut_l)
    machine_plane = hsl(base_hue, 10, mach_l)

    # Verify text vs cuttable plane contrast - adjust cuttable plane if needed
    for target_color, target_ratio in [(text_rgb, 3.0), (text_disabled_rgb, 2.5)]:
        if contrast_ratio(target_color, cuttable_plane) < target_ratio:
            if mode == "light":
                for delta in range(0, 40):
                    candidate = hsl(base_hue, cut_s, min(cut_l + delta, 98))
                    if contrast_ratio(target_color, candidate) >= target_ratio:
                        cuttable_plane = candidate
                        cut_l = min(cut_l + delta, 98)
                        break
            else:
                for delta in range(0, 30):
                    candidate = hsl(base_hue, cut_s, max(cut_l - delta, 2))
                    if contrast_ratio(target_color, candidate) >= target_ratio:
                        cuttable_plane = candidate
                        cut_l = max(cut_l - delta, 2)
                        break

    # Verify plane separation
    if contrast_ratio(cuttable_plane, machine_plane) < 2.0:
        if mode == "light":
            machine_plane = hsl(base_hue, 10, max(mach_l - 10, 2))
        else:
            machine_plane = hsl(base_hue, 10, max(mach_l - 5, 2))

    # Verify HeaderHovered vs cuttable plane (uses tertiary hue)
    if contrast_ratio(header_hovered, cuttable_plane) < 3.0:
        hh_l = 30 if mode == "light" else btn_hover_l
        direction = "darker" if mode == "light" else "lighter"
        hh_l = adjust_lightness_for_contrast(
            tertiary_hue, btn_sat_hi, hh_l, cuttable_plane, 3.0, direction)
        header_hovered = hsl(tertiary_hue, btn_sat_hi, hh_l)

    # --- TextSelectedBg, DragDropTarget ---
    text_selected_bg = hsl(primary_hue, sat, accent_l)
    drag_drop_target = hsl(secondary_hue, sat, accent_l)

    # Verify DragDropTarget vs cuttable plane
    if contrast_ratio(drag_drop_target, cuttable_plane) < 3.0:
        ddl = accent_l
        direction = "lighter" if mode != "light" else "darker"
        ddl = adjust_lightness_for_contrast(
            secondary_hue, sat, ddl, cuttable_plane, 3.0, direction)
        drag_drop_target = hsl(secondary_hue, sat, ddl)

    # --- Build theme dict ---
    description_modes = {"light": "Light", "medium": "Medium", "dark": "Dark"}
    theme = {
        "name": name,
        "description": f"{description_modes[mode]} theme generated with {accent_scheme} {saturation} accents",
        "app_colors": {
            "machine_plane_color": list(machine_plane),
            "cuttable_plane_color": list(cuttable_plane),
        },
        "imgui_colors": {
            "Text": rgba(text_rgb),
            "TextDisabled": rgba(text_disabled_rgb),
            "WindowBg": rgba(window_bg),
            "ChildBg": rgba(child_bg),
            "PopupBg": rgba(popup_bg, 0.98),
            "Border": rgba(border, 0.50),
            "BorderShadow": rgba((0, 0, 0), 0.0),
            "FrameBg": rgba(frame_bg),
            "FrameBgHovered": rgba(frame_bg_hovered),
            "FrameBgActive": rgba(frame_bg_active),
            "TitleBg": rgba(title_bg),
            "TitleBgActive": rgba(title_bg_active),
            "TitleBgCollapsed": rgba(title_bg_collapsed, 0.75),
            "MenuBarBg": rgba(menu_bar_bg),
            "ScrollbarBg": rgba(scrollbar_bg, 0.53),
            "ScrollbarGrab": rgba(scrollbar_grab, 0.80),
            "ScrollbarGrabHovered": rgba(scrollbar_grab_hover, 0.80),
            "ScrollbarGrabActive": rgba(scrollbar_grab_active),
            "CheckMark": rgba(primary_accent),
            "SliderGrab": rgba(tertiary_accent),
            "SliderGrabActive": rgba(tertiary_active),
            "Button": rgba(button),
            "ButtonHovered": rgba(button_hovered),
            "ButtonActive": rgba(button_active),
            "Header": rgba(header),
            "HeaderHovered": rgba(header_hovered),
            "HeaderActive": rgba(header_active),
            "Separator": rgba(sep, 0.62),
            "SeparatorHovered": rgba(sep_hovered, 0.78),
            "SeparatorActive": rgba(sep_active),
            "ResizeGrip": rgba(resize, 0.25),
            "ResizeGripHovered": rgba(resize_hovered, 0.67),
            "ResizeGripActive": rgba(resize_active, 0.95),
            "Tab": rgba(tab, 0.86),
            "TabHovered": rgba(tab_hovered, 0.80),
            "TabActive": rgba(tab_active),
            "TabUnfocused": rgba(tab_unfocused, 0.97),
            "TabUnfocusedActive": rgba(tab_unfocused_active),
            "PlotLines": rgba(plot_lines),
            "PlotLinesHovered": rgba(plot_lines_hovered),
            "PlotHistogram": rgba(plot_histogram),
            "PlotHistogramHovered": rgba(plot_histogram_hovered),
            "TableHeaderBg": rgba(table_header_bg),
            "TableBorderStrong": rgba(table_border_strong),
            "TableBorderLight": rgba(table_border_light),
            "TableRowBg": rgba((0, 0, 0), 0.0),
            "TableRowBgAlt": rgba((0.5, 0.5, 0.5), 0.06 if mode != "light" else 0.09),
            "TextSelectedBg": rgba(text_selected_bg, 0.35),
            "DragDropTarget": rgba(drag_drop_target, 0.95),
            "NavHighlight": rgba(primary_accent, 0.80),
            "NavWindowingHighlight": rgba((1, 1, 1) if mode != "light" else (0.3, 0.3, 0.3), 0.70),
            "NavWindowingDimBg": rgba((0.2, 0.2, 0.2), 0.20),
            "ModalWindowDimBg": rgba((0.0, 0.0, 0.0) if mode != "light" else (0.5, 0.5, 0.5), 0.35),
        },
        "imgui_style": {
            "WindowRounding": 5.0,
            "ChildRounding": 5.0,
            "FrameRounding": 3.0,
            "PopupRounding": 3.0,
            "ScrollbarRounding": 7.0,
            "GrabRounding": 3.0,
            "TabRounding": 3.0,
            "WindowPadding": [8.0, 8.0],
            "FramePadding": [5.0, 3.0],
            "ItemSpacing": [8.0, 4.0],
            "ItemInnerSpacing": [4.0, 4.0],
            "IndentSpacing": 21.0,
            "ScrollbarSize": 13.0,
            "GrabMinSize": 9.0,
            "WindowBorderSize": 1.0,
            "ChildBorderSize": 1.0,
            "PopupBorderSize": 1.0,
            "FrameBorderSize": 0.0,
            "TabBorderSize": 0.0,
            "DockingSeparatorSize": 2.0,
            "SeparatorTextBorderSize": 1.0,
        },
    }

    return theme


def main():
    parser = argparse.ArgumentParser(description="Generate a nanocut-control theme")
    parser.add_argument("--name", required=True, help="Theme display name")
    parser.add_argument("--base-hue", type=int, required=True,
                        help="Base hue 0-360 (0=red, 120=green, 240=blue)")
    parser.add_argument("--mode", choices=["dark", "light", "medium"], default="dark",
                        help="Theme brightness mode")
    parser.add_argument("--accent-scheme",
                        choices=["complementary", "analogous", "triadic", "split-complementary"],
                        default="complementary",
                        help="Color scheme for accent colors")
    parser.add_argument("--saturation",
                        choices=["vibrant", "moderate", "muted"],
                        default="moderate",
                        help="Accent saturation level")
    parser.add_argument("--warmth",
                        choices=["warm", "cool", "neutral"],
                        default="neutral",
                        help="Overall warmth shift")
    parser.add_argument("--output", "-o", help="Output file path (default: assets/themes/<slug>.json)")
    parser.add_argument("--validate", action="store_true", default=True,
                        help="Run analyzer on generated theme (default: true)")
    parser.add_argument("--no-validate", action="store_true",
                        help="Skip validation")
    args = parser.parse_args()

    theme = generate_theme(
        name=args.name,
        base_hue=args.base_hue,
        mode=args.mode,
        accent_scheme=args.accent_scheme,
        saturation=args.saturation,
        warmth=args.warmth,
    )

    # Determine output path
    if args.output:
        out_path = Path(args.output)
    else:
        slug = args.name.lower().replace(" ", "_").replace("-", "_")
        out_path = Path("assets/themes") / f"{slug}.json"

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(theme, f, indent=2)
        f.write("\n")

    print(f"Generated theme '{args.name}' -> {out_path}")

    # Validate
    if args.validate and not args.no_validate:
        print("\nRunning validation...")
        result = subprocess.run(
            [sys.executable, "scripts/analyze_themes.py", "--detailed", str(out_path)],
            capture_output=False,
        )
        if result.returncode != 0:
            print("Validation had issues - review output above")


if __name__ == "__main__":
    main()
