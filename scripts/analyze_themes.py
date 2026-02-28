#!/usr/bin/env python3

import json
import os
import colorsys
from pathlib import Path

def rgb_to_hex(rgb):
    """Convert RGB values (0-1) to hex string"""
    r, g, b = rgb
    return f"#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}"

def calculate_contrast(rgb1, rgb2):
    """Calculate contrast ratio between two RGB colors (0-1 range)"""
    def luminance(rgb):
        r, g, b = rgb
        # Convert to sRGB and apply gamma correction
        def adjust_gamma(c):
            if c <= 0.03928:
                return c / 12.92
            else:
                return ((c + 0.055) / 1.055) ** 2.4
        
        r_lin = adjust_gamma(r)
        g_lin = adjust_gamma(g) 
        b_lin = adjust_gamma(b)
        
        return 0.2126 * r_lin + 0.7152 * g_lin + 0.0722 * b_lin
    
    l1 = luminance(rgb1)
    l2 = luminance(rgb2)
    
    lighter = max(l1, l2)
    darker = min(l1, l2)
    
    return (lighter + 0.05) / (darker + 0.05)

def get_contrast_rating(ratio, element_type="main"):
    """
    Get contrast rating for CAD/CAM applications (more permissive than WCAG)
    
    For CAD/CAM applications:
    - Main paths (outside contours) need to be clearly visible
    - Secondary paths (inside contours) can have slightly lower contrast
    - Highlight paths (open contours, mouse over) should stand out
    """
    if element_type == "main":  # Outside contours - most important
        if ratio >= 4.5:
            return "✅ Excellent", "good"
        elif ratio >= 3.0:
            return "✅ Good", "good"
        elif ratio >= 2.0:
            return "⚠️  Acceptable", "acceptable"
        else:
            return "❌ Needs improvement", "poor"
    
    elif element_type == "secondary":  # Inside contours
        if ratio >= 3.5:
            return "✅ Excellent", "good"
        elif ratio >= 2.5:
            return "✅ Good", "good"
        elif ratio >= 1.8:
            return "⚠️  Acceptable", "acceptable"
        else:
            return "❌ Needs improvement", "poor"
    
    elif element_type == "highlight":  # Open contours, mouse over
        if ratio >= 4.0:
            return "✅ Excellent", "good"
        elif ratio >= 3.0:
            return "✅ Good", "good"
        elif ratio >= 2.0:
            return "⚠️  Acceptable", "acceptable"
        else:
            return "❌ Needs improvement", "poor"
    
    else:  # Default
        if ratio >= 4.5:
            return "✅ Excellent", "good"
        elif ratio >= 3.0:
            return "✅ Good", "good"
        elif ratio >= 2.0:
            return "⚠️  Acceptable", "acceptable"
        else:
            return "❌ Needs improvement", "poor"

def analyze_theme(theme_path):
    """Analyze a single theme file focusing on cuttable plane"""
    with open(theme_path, 'r') as f:
        theme_data = json.load(f)
    
    theme_name = theme_data['name']
    
    # Get cuttable plane color (the important background for paths)
    cuttable_plane = theme_data['app_colors']['cuttable_plane_color']
    cuttable_plane_hex = rgb_to_hex(cuttable_plane)
    
    # Get machine plane for comparison
    machine_plane = theme_data['app_colors']['machine_plane_color']
    machine_plane_hex = rgb_to_hex(machine_plane)
    
    # Get path/text colors from ImGui colors
    imgui_colors = theme_data['imgui_colors']
    
    # These are the colors used for paths based on the code analysis
    outside_contour = imgui_colors['Text'][:3]  # m_outside_contour_color = ThemeColor::Text
    inside_contour = imgui_colors['TextDisabled'][:3]  # m_inside_contour_color = ThemeColor::TextDisabled
    open_contour = imgui_colors['DragDropTarget'][:3]  # m_open_contour_color = ThemeColor::DragDropTarget
    mouse_over = imgui_colors['HeaderHovered'][:3]  # m_mouse_over_color = ThemeColor::HeaderHovered
    
    # Calculate contrast ratios against cuttable plane (most important!)
    cuttable_contrasts = {
        'Outside Contour vs Cuttable Plane': calculate_contrast(outside_contour, cuttable_plane),
        'Inside Contour vs Cuttable Plane': calculate_contrast(inside_contour, cuttable_plane),
        'Open Contour vs Cuttable Plane': calculate_contrast(open_contour, cuttable_plane),
        'Mouse Over vs Cuttable Plane': calculate_contrast(mouse_over, cuttable_plane)
    }
    
    # Also calculate contrast against machine plane for comparison
    machine_contrasts = {
        'Outside Contour vs Machine Plane': calculate_contrast(outside_contour, machine_plane),
        'Inside Contour vs Machine Plane': calculate_contrast(inside_contour, machine_plane),
        'Open Contour vs Machine Plane': calculate_contrast(open_contour, machine_plane),
        'Mouse Over vs Machine Plane': calculate_contrast(mouse_over, machine_plane)
    }
    
    # Check if cuttable plane has good contrast with machine plane
    plane_contrast = calculate_contrast(cuttable_plane, machine_plane)
    
    # Rate the theme overall
    ratings = []
    for contrast_name, ratio in cuttable_contrasts.items():
        if "Outside" in contrast_name:
            rating, category = get_contrast_rating(ratio, "main")
        elif "Inside" in contrast_name:
            rating, category = get_contrast_rating(ratio, "secondary")
        else:
            rating, category = get_contrast_rating(ratio, "highlight")
        ratings.append(category)
    
    # Overall theme rating
    if all(r == "good" for r in ratings):
        overall_rating = "✅ Excellent - All paths clearly visible"
    elif ratings.count("good") >= 3:
        overall_rating = "✅ Good - Most paths clearly visible"
    elif ratings.count("good") >= 2 and not "poor" in ratings:
        overall_rating = "⚠️  Acceptable - Paths visible but could be better"
    else:
        overall_rating = "❌ Needs improvement - Some paths may be hard to see"
    
    return {
        'name': theme_name,
        'cuttable_plane': cuttable_plane,
        'cuttable_plane_hex': cuttable_plane_hex,
        'machine_plane': machine_plane,
        'machine_plane_hex': machine_plane_hex,
        'plane_contrast': plane_contrast,
        'outside_contour': outside_contour,
        'outside_contour_hex': rgb_to_hex(outside_contour),
        'inside_contour': inside_contour,
        'inside_contour_hex': rgb_to_hex(inside_contour),
        'open_contour': open_contour,
        'open_contour_hex': rgb_to_hex(open_contour),
        'mouse_over': mouse_over,
        'mouse_over_hex': rgb_to_hex(mouse_over),
        'cuttable_contrasts': cuttable_contrasts,
        'machine_contrasts': machine_contrasts,
        'overall_rating': overall_rating
    }

def main():
    themes_dir = Path("assets/themes")
    theme_files = list(themes_dir.glob("*.json"))
    
    print(f"Analyzing cuttable plane contrast for {len(theme_files)} themes (CAD/CAM permissive standards)")
    print("=" * 100)
    
    all_results = []
    
    for theme_file in sorted(theme_files):
        result = analyze_theme(theme_file)
        all_results.append(result)
        
        print(f"\n🎨 {result['name']} {result['overall_rating']}")
        print(f"   Cuttable Plane: {result['cuttable_plane_hex']} (RGB: {result['cuttable_plane']})")
        print(f"   Machine Plane:  {result['machine_plane_hex']} (RGB: {result['machine_plane']})")
        print(f"   Plane Contrast:  {result['plane_contrast']:.2f}:1")
        print(f"   Outside Contour: {result['outside_contour_hex']} (RGB: {result['outside_contour']})")
        print(f"   Inside Contour:  {result['inside_contour_hex']} (RGB: {result['inside_contour']})")
        print(f"   Open Contour:    {result['open_contour_hex']} (RGB: {result['open_contour']})")
        print(f"   Mouse Over:      {result['mouse_over_hex']} (RGB: {result['mouse_over']})")
        
        print("\n   📊 Contrast Ratios (vs Cuttable Plane - MOST IMPORTANT):")
        for contrast_name, ratio in result['cuttable_contrasts'].items():
            if "Outside" in contrast_name:
                rating, _ = get_contrast_rating(ratio, "main")
            elif "Inside" in contrast_name:
                rating, _ = get_contrast_rating(ratio, "secondary")
            else:
                rating, _ = get_contrast_rating(ratio, "highlight")
            print(f"      {contrast_name}: {ratio:.2f}:1 {rating}")
        
        print("\n   📊 Contrast Ratios (vs Machine Plane - for comparison):")
        for contrast_name, ratio in result['machine_contrasts'].items():
            if "Outside" in contrast_name:
                rating, _ = get_contrast_rating(ratio, "main")
            elif "Inside" in contrast_name:
                rating, _ = get_contrast_rating(ratio, "secondary")
            else:
                rating, _ = get_contrast_rating(ratio, "highlight")
            print(f"      {contrast_name}: {ratio:.2f}:1 {rating}")
    
    print("\n" + "=" * 100)
    print("📋 SUMMARY: THEME QUALITY ASSESSMENT")
    print("=" * 100)
    
    # Categorize themes
    excellent_themes = []
    good_themes = []
    acceptable_themes = []
    needs_improvement_themes = []
    
    for result in all_results:
        if "Excellent" in result['overall_rating']:
            excellent_themes.append(result)
        elif "Good" in result['overall_rating']:
            good_themes.append(result)
        elif "Acceptable" in result['overall_rating']:
            acceptable_themes.append(result)
        else:
            needs_improvement_themes.append(result)
    
    print(f"\n✅ EXCELLENT THEMES ({len(excellent_themes)}/13):")
    for theme in excellent_themes:
        print(f"   🎨 {theme['name']} - Cuttable: {theme['cuttable_plane_hex']}")
    
    print(f"\n✅ GOOD THEMES ({len(good_themes)}/13):")
    for theme in good_themes:
        print(f"   🎨 {theme['name']} - Cuttable: {theme['cuttable_plane_hex']}")
    
    print(f"\n⚠️  ACCEPTABLE THEMES ({len(acceptable_themes)}/13):")
    for theme in acceptable_themes:
        print(f"   🎨 {theme['name']} - Cuttable: {theme['cuttable_plane_hex']}")
    
    print(f"\n❌ NEEDS IMPROVEMENT ({len(needs_improvement_themes)}/13):")
    for theme in needs_improvement_themes:
        print(f"   🎨 {theme['name']} - Cuttable: {theme['cuttable_plane_hex']}")
    
    print(f"\n📊 OVERALL QUALITY DISTRIBUTION:")
    print(f"   Excellent: {len(excellent_themes)}/13 ({len(excellent_themes)*100/13:.0f}%)")
    print(f"   Good: {len(good_themes)}/13 ({len(good_themes)*100/13:.0f}%)")
    print(f"   Acceptable: {len(acceptable_themes)}/13 ({len(acceptable_themes)*100/13:.0f}%)")
    print(f"   Needs Improvement: {len(needs_improvement_themes)}/13 ({len(needs_improvement_themes)*100/13:.0f}%)")

if __name__ == "__main__":
    main()