#!/usr/bin/env python3
"""
Script to feather the edges of the logo image.
Reads assets/logo.png, applies edge feathering, and saves as assets/logo_feathered.png
"""

from PIL import Image, ImageDraw, ImageFilter

from PIL import Image

from PIL import Image
import math

from PIL import Image
import math

from PIL import Image
import math

from PIL import Image, ImageFilter, ImageChops

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

def feather_edges(
    image,
    rect_radius=30,
    rect_sigma=15,
    circle_sigma=20,
    circle_scale=0.95,
):
    if image.mode != "RGBA":
        image = image.convert("RGBA")

    width, height = image.size
    r, g, b, alpha = image.split()
    alpha_np = np.asarray(alpha, dtype=np.float32)

    # -----------------------
    # Rectangular mask (hard)
    # -----------------------
    rect_mask = np.ones((height, width), dtype=np.float32)
    rect_mask[:rect_radius, :] = 0
    rect_mask[-rect_radius:, :] = 0
    rect_mask[:, :rect_radius] = 0
    rect_mask[:, -rect_radius:] = 0

    rect_mask = gaussian_filter(rect_mask, sigma=rect_sigma)
    rect_mask /= rect_mask.max()  # normalize

    # -----------------------
    # Circular mask (hard!)
    # -----------------------
    cx = (width - 1) / 2.0
    cy = (height - 1) / 2.0
    radius = min(width, height) * circle_scale / 2.0

    y, x = np.ogrid[:height, :width]
    dist = np.sqrt((x - cx) ** 2 + (y - cy) ** 2)

    circle_mask = (dist <= radius).astype(np.float32)

    circle_mask = gaussian_filter(circle_mask, sigma=circle_sigma)
    circle_mask /= circle_mask.max()  # CRITICAL

    # -----------------------
    # Combine masks safely
    # -----------------------
    combined_mask = rect_mask * circle_mask

    # -----------------------
    # Apply to alpha
    # -----------------------
    feathered_alpha = np.clip(alpha_np * combined_mask, 0, 255).astype(np.uint8)

    return Image.merge(
        "RGBA",
        (r, g, b, Image.fromarray(feathered_alpha, mode="L")),
    )

def main():
    # Input and output paths
    input_path = "assets/logo.png"
    output_path = "assets/logo_feathered.png"
    
    # Open the logo image
    print(f"Reading {input_path}...")
    logo = Image.open(input_path)
    
    # Convert to RGBA if not already
    if logo.mode != 'RGBA':
        logo = logo.convert('RGBA')
    
    print(f"Original size: {logo.size}, mode: {logo.mode}")
    
    # Apply feathering with 10% of image size
    print("Applying edge feathering...")
    feathered_logo = feather_edges(logo, int(logo.size[0]*0.06), 20, 20, 1.1)
    
    # Save the result
    print(f"Saving feathered logo to {output_path}...")
    feathered_logo.save(output_path)
    
    print("Done!")

if __name__ == "__main__":
    main()
