import math
import os
from PIL import Image, ImageDraw

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "templates")
os.makedirs(OUTPUT_DIR, exist_ok=True)

# ---------------------------------------------------------------------------
# 1. SPIDER-VERSE (Chibi Spider-Man Action Animation with 4x Supersampling)
# ---------------------------------------------------------------------------
def make_spiderverse_gif():
    total_frames = 24
    scale = 4
    w, h = 64 * scale, 32 * scale  # 256 x 128 supersampled
    raw_frames = []

    for frame_idx in range(total_frames):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        t = frame_idx / total_frames
        
        # Heroic breathing bob (gentle vertical float)
        bob_y = int(math.sin(t * 2 * math.pi) * 3.5)
        
        # Web shooting action dynamics
        is_shooting = 9 <= frame_idx <= 17
        shoot_progress = (frame_idx - 9) / 8.0 if is_shooting else 0.0
        
        # Dynamic hop when pulling the web
        hop_x = 0
        hop_y = 0
        if 12 <= frame_idx <= 15:
            hop_x = int(math.sin((frame_idx - 12) / 3.0 * math.pi) * 5)
            hop_y = -int(math.sin((frame_idx - 12) / 3.0 * math.pi) * 4)

        cx = 92 + hop_x
        cy = 48 + bob_y + hop_y
        
        # Blink/Squint state
        blink = 0.0
        lens_wide = 0.0
        if 5 <= frame_idx <= 6:
            blink = (frame_idx - 4) / 2.0
        elif 7 <= frame_idx <= 8:
            blink = 1.0 - (frame_idx - 6) / 2.0
        elif 11 <= frame_idx <= 15:
            lens_wide = math.sin((frame_idx - 11) / 4.0 * math.pi) * 0.25

        # Spider-Verse chromatic aberration on shoot burst
        is_glitch = frame_idx in [10, 11, 12]
        
        def draw_character(draw_ctx, offset_x=0, offset_y=0, tint=None):
            ocx = cx + offset_x
            ocy = cy + offset_y
            
            red_base = (235, 25, 45, 255)
            red_dark = (165, 12, 28, 255)
            blue_suit = (16, 75, 195, 255)
            blue_dark = (10, 48, 140, 255)
            black_col = (14, 14, 18, 255)
            white_col = (255, 255, 255, 255)
            web_line_col = (45, 8, 14, 220)
            
            if tint == "cyan":
                red_base = (0, 235, 255, 180)
                red_dark = (0, 180, 220, 180)
                blue_suit = (0, 160, 255, 180)
                black_col = (0, 210, 250, 180)
                web_line_col = (0, 180, 220, 140)
            elif tint == "magenta":
                red_base = (255, 0, 120, 180)
                red_dark = (200, 0, 90, 180)
                blue_suit = (180, 0, 160, 180)
                black_col = (150, 0, 90, 180)
                web_line_col = (180, 0, 80, 140)

            # 1. Legs / Boots (Sitting/Crouching Hero Stance)
            torso_top = ocy + 22
            torso_bottom = ocy + 64
            draw_ctx.ellipse([ocx - 22, torso_bottom - 10, ocx - 6, torso_bottom + 12], fill=blue_dark)
            draw_ctx.ellipse([ocx + 6, torso_bottom - 10, ocx + 22, torso_bottom + 12], fill=blue_dark)
            draw_ctx.ellipse([ocx - 24, torso_bottom + 2, ocx - 4, torso_bottom + 14], fill=red_dark)
            draw_ctx.ellipse([ocx + 4, torso_bottom + 2, ocx + 24, torso_bottom + 14], fill=red_dark)

            # 2. Torso (Blue sides + Red central vest)
            draw_ctx.ellipse([ocx - 26, torso_top + 4, ocx + 26, torso_bottom + 4], fill=blue_suit)
            draw_ctx.polygon([
                (ocx - 13, torso_top),
                (ocx + 13, torso_top),
                (ocx + 9, torso_bottom),
                (ocx - 9, torso_bottom)
            ], fill=red_base, outline=red_dark, width=1)
            
            # Black Spider Emblem on chest
            if tint is None:
                draw_ctx.ellipse([ocx - 3, torso_top + 13, ocx + 3, torso_top + 23], fill=black_col)
                draw_ctx.ellipse([ocx - 2, torso_top + 9, ocx + 2, torso_top + 13], fill=black_col)
                leg_y = torso_top + 16
                draw_ctx.line([(ocx - 2, leg_y - 2), (ocx - 8, leg_y - 8), (ocx - 14, leg_y - 5)], fill=black_col, width=2)
                draw_ctx.line([(ocx + 2, leg_y - 2), (ocx + 8, leg_y - 8), (ocx + 14, leg_y - 5)], fill=black_col, width=2)
                draw_ctx.line([(ocx - 2, leg_y), (ocx - 10, leg_y - 2), (ocx - 16, leg_y + 2)], fill=black_col, width=2)
                draw_ctx.line([(ocx + 2, leg_y), (ocx + 10, leg_y - 2), (ocx + 16, leg_y + 2)], fill=black_col, width=2)
                draw_ctx.line([(ocx - 2, leg_y + 2), (ocx - 10, leg_y + 5), (ocx - 14, leg_y + 11)], fill=black_col, width=2)
                draw_ctx.line([(ocx + 2, leg_y + 2), (ocx + 10, leg_y + 5), (ocx + 14, leg_y + 11)], fill=black_col, width=2)
                draw_ctx.line([(ocx - 1, leg_y + 4), (ocx - 7, leg_y + 10), (ocx - 11, leg_y + 15)], fill=black_col, width=2)
                draw_ctx.line([(ocx + 1, leg_y + 4), (ocx + 7, leg_y + 10), (ocx + 11, leg_y + 15)], fill=black_col, width=2)

            # 3. Left Arm (resting heroically)
            draw_ctx.line([(ocx - 18, torso_top + 6), (ocx - 28, torso_top + 22)], fill=red_base, width=8)
            draw_ctx.ellipse([ocx - 32, torso_top + 18, ocx - 24, torso_top + 26], fill=red_dark)

            # 4. Right Arm (Animated web-shooter)
            if is_shooting:
                reach = math.sin(shoot_progress * math.pi)
                hand_x = ocx + 36 + int(reach * 18)
                hand_y = torso_top + 8 - int(reach * 8)
                draw_ctx.line([(ocx + 16, torso_top + 6), (hand_x, hand_y)], fill=red_base, width=8)
                draw_ctx.ellipse([hand_x - 4, hand_y - 4, hand_x + 6, hand_y + 6], fill=red_dark)
                draw_ctx.line([(hand_x + 4, hand_y - 3), (hand_x + 12, hand_y - 6)], fill=red_base, width=3)
                draw_ctx.line([(hand_x + 4, hand_y + 3), (hand_x + 12, hand_y + 6)], fill=red_base, width=3)
            else:
                draw_ctx.line([(ocx + 18, torso_top + 6), (ocx + 28, torso_top + 22)], fill=red_base, width=8)
                draw_ctx.ellipse([ocx + 24, torso_top + 18, ocx + 32, torso_top + 26], fill=red_dark)

            # 5. Spider-Man Head (Mask)
            head_pts = []
            head_w = 29
            head_h_top = 28
            head_h_bot = 32
            for deg in range(0, 360, 15):
                rad = math.radians(deg)
                y_mult = head_h_top if math.sin(rad) < 0 else head_h_bot
                taper_x = (0.84 if math.sin(rad) > 0.4 else 1.0)
                hx = ocx + int(head_w * taper_x * math.cos(rad))
                hy = ocy + int(y_mult * math.sin(rad))
                head_pts.append((hx, hy))

            draw_ctx.polygon(head_pts, fill=red_base, outline=red_dark, width=2)

            # 6. Radial Web Pattern on Mask
            web_center = (ocx, ocy - 4)
            angles = [0, 35, 75, 105, 145, 180, 215, 255, 285, 325]
            radial_ends = []
            for ang in angles:
                rad = math.radians(ang)
                y_mult = head_h_top if math.sin(rad) < 0 else head_h_bot
                taper_x = (0.84 if math.sin(rad) > 0.4 else 1.0)
                ex = ocx + int(head_w * 0.95 * taper_x * math.cos(rad))
                ey = ocy + int(y_mult * 0.95 * math.sin(rad))
                radial_ends.append((ex, ey))
                draw_ctx.line([web_center, (ex, ey)], fill=web_line_col, width=1)

            # Concentric web loop rings
            for ring_frac in [0.38, 0.72]:
                for i in range(len(angles)):
                    p1 = (
                        int(web_center[0] + (radial_ends[i][0] - web_center[0]) * ring_frac),
                        int(web_center[1] + (radial_ends[i][1] - web_center[1]) * ring_frac)
                    )
                    nxt = (i + 1) % len(angles)
                    p2 = (
                        int(web_center[0] + (radial_ends[nxt][0] - web_center[0]) * ring_frac),
                        int(web_center[1] + (radial_ends[nxt][1] - web_center[1]) * ring_frac)
                    )
                    mid_x = (p1[0] + p2[0]) // 2
                    mid_y = (p1[1] + p2[1]) // 2
                    inward_x = int(web_center[0] + (mid_x - web_center[0]) * 0.86)
                    inward_y = int(web_center[1] + (mid_y - web_center[1]) * 0.86)
                    draw_ctx.line([p1, (inward_x, inward_y)], fill=web_line_col, width=1)
                    draw_ctx.line([(inward_x, inward_y), p2], fill=web_line_col, width=1)

            # 7. Iconic Expressive Spider-Man Eyes (Lenses)
            effective_blink = min(1.0, blink)
            lens_scale = 1.0 + lens_wide
            
            l_top_outer = (ocx - int(25 * lens_scale), ocy - int(13 * lens_scale))
            l_top_inner = (ocx - 6, ocy - 3)
            l_bot_inner = (ocx - 7, ocy + int(13 * (1.0 - effective_blink * 0.85) * lens_scale))
            l_bot_outer = (ocx - int(23 * lens_scale), ocy + int(5 * (1.0 - effective_blink * 0.5) * lens_scale))
            left_lens = [l_top_outer, l_top_inner, l_bot_inner, l_bot_outer]
            
            r_top_outer = (ocx + int(25 * lens_scale), ocy - int(13 * lens_scale))
            r_top_inner = (ocx + 6, ocy - 3)
            r_bot_inner = (ocx + 7, ocy + int(13 * (1.0 - effective_blink * 0.85) * lens_scale))
            r_bot_outer = (ocx + int(23 * lens_scale), ocy + int(5 * (1.0 - effective_blink * 0.5) * lens_scale))
            right_lens = [r_top_outer, r_top_inner, r_bot_inner, r_bot_outer]

            draw_ctx.polygon(left_lens, fill=white_col, outline=black_col, width=4)
            draw_ctx.polygon(right_lens, fill=white_col, outline=black_col, width=4)

            if tint is None and effective_blink < 0.6:
                draw_ctx.ellipse([ocx - 19, ocy - 9, ocx - 12, ocy - 4], fill=(225, 250, 255, 220))
                draw_ctx.ellipse([ocx + 12, ocy - 9, ocx + 19, ocy - 4], fill=(225, 250, 255, 220))

        if is_glitch:
            draw_character(d, offset_x=-5, offset_y=0, tint="cyan")
            draw_character(d, offset_x=5, offset_y=0, tint="magenta")

        draw_character(d, offset_x=0, offset_y=0, tint=None)

        # 8. Dynamic Web Shooting Effect ("THWIP!")
        if is_shooting:
            reach = math.sin(shoot_progress * math.pi)
            web_start_x = cx + 44 + int(reach * 18)
            web_start_y = cy + 22 + 8
            web_target_x = min(w - 12, web_start_x + int(shoot_progress * 135) + 24)
            web_target_y = cy + 16 - int(math.sin(shoot_progress * math.pi) * 8)
            
            d.line([(web_start_x, web_start_y), (web_target_x, web_target_y)], fill=(0, 240, 255, 180), width=5)
            d.line([(web_start_x, web_start_y), (web_target_x, web_target_y)], fill=(255, 255, 255, 255), width=3)
            
            mid_x = (web_start_x + web_target_x) // 2
            mid_y = (web_start_y + web_target_y) // 2
            spiral_r = int(5 * math.sin(shoot_progress * math.pi))
            if spiral_r > 1:
                d.ellipse([mid_x - spiral_r, mid_y - spiral_r, mid_x + spiral_r, mid_y + spiral_r],
                          outline=(255, 255, 255, 200), width=2)

            burst_r = int(6 + math.sin(shoot_progress * math.pi) * 9)
            d.ellipse([web_target_x - burst_r, web_target_y - burst_r, web_target_x + burst_r, web_target_y + burst_r], 
                      outline=(255, 255, 255, 255), width=2)
            for ray_ang in [0, 45, 90, 135, 180, 225, 270, 315]:
                rr = math.radians(ray_ang)
                sx = web_target_x + int(burst_r * math.cos(rr))
                sy = web_target_y + int(burst_r * math.sin(rr))
                ex = web_target_x + int((burst_r + 6) * math.cos(rr))
                ey = web_target_y + int((burst_r + 6) * math.sin(rr))
                d.line([(sx, sy), (ex, ey)], fill=(0, 255, 240, 230), width=2)

        final_img = img.resize((64, 32), Image.Resampling.LANCZOS)
        raw_frames.append(final_img)

    frames = []
    for f in raw_frames:
        alpha = f.split()[3]
        p_img = f.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
        p_img.paste(255, Image.eval(alpha, lambda a: 255 if a == 0 else 0))
        p_img.info['transparency'] = 255
        frames.append(p_img)

    out_path = os.path.join(OUTPUT_DIR, "spiderverse.gif")
    frames[0].save(
        out_path,
        save_all=True,
        append_images=frames[1:],
        duration=70,
        loop=0,
        transparency=255,
        disposal=2
    )
    print(f"Generated Upgraded Spider-Verse GIF -> {out_path}")

# ---------------------------------------------------------------------------
# 2. CUTE CAT (Chibi Kawaii Cat Animation)
# ---------------------------------------------------------------------------
def make_cat_gif():
    w, h = 48, 32
    frames = []
    total_frames = 18

    for f in range(total_frames):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        # Breathing / gentle vertical bobbing
        bob = int(math.sin(f * (2 * math.pi / total_frames)) * 1.5)
        cx, cy = w // 2, h // 2 + bob

        # Ear twitch on frames 8-11
        ear_twitch_left = -2 if (8 <= f <= 10) else 0
        ear_twitch_right = 2 if (11 <= f <= 13) else 0

        # Blinking state
        is_blinking = (5 <= f <= 7) or (15 <= f <= 16)

        # Paws tapping
        left_paw_y = cy + 9 + (1 if f % 4 < 2 else 0)
        right_paw_y = cy + 9 + (0 if f % 4 < 2 else 1)

        # 1. Ears
        # Left ear
        d.polygon([(cx - 13 + ear_twitch_left, cy - 5), (cx - 10, cy - 14), (cx - 4, cy - 7)], fill=(250, 248, 245, 255), outline=(220, 215, 205, 255))
        d.polygon([(cx - 11 + ear_twitch_left, cy - 6), (cx - 9, cy - 12), (cx - 6, cy - 7)], fill=(255, 175, 190, 255))
        # Right ear
        d.polygon([(cx + 4, cy - 7), (cx + 10, cy - 14 + ear_twitch_right), (cx + 13, cy - 5)], fill=(250, 248, 245, 255), outline=(220, 215, 205, 255))
        d.polygon([(cx + 6, cy - 7), (cx + 9, cy - 12 + ear_twitch_right), (cx + 11, cy - 6)], fill=(255, 175, 190, 255))

        # 2. Cat Head (Chibi cute oval)
        d.ellipse([cx - 14, cy - 8, cx + 14, cy + 9], fill=(252, 250, 246, 255), outline=(220, 215, 205, 255), width=1)

        # 3. Cute Cheeks (Blush)
        d.ellipse([cx - 12, cy + 2, cx - 7, cy + 5], fill=(255, 160, 180, 160))
        d.ellipse([cx + 7, cy + 2, cx + 12, cy + 5], fill=(255, 160, 180, 160))

        # 4. Eyes
        if is_blinking:
            # Happy curved closed eyes ^ ^
            d.line([(cx - 8, cy - 1), (cx - 6, cy - 3), (cx - 4, cy - 1)], fill=(40, 30, 25, 255), width=1)
            d.line([(cx + 4, cy - 1), (cx + 6, cy - 3), (cx + 8, cy - 1)], fill=(40, 30, 25, 255), width=1)
        else:
            # Big dark glossy eyes
            d.ellipse([cx - 8, cy - 3, cx - 4, cy + 2], fill=(35, 25, 20, 255))
            d.ellipse([cx + 4, cy - 3, cx + 8, cy + 2], fill=(35, 25, 20, 255))
            # Catchlight shine (white dot)
            d.rectangle([cx - 7, cy - 2, cx - 6, cy - 1], fill=(255, 255, 255, 255))
            d.rectangle([cx + 5, cy - 2, cx + 6, cy - 1], fill=(255, 255, 255, 255))

        # 5. Nose & Mouth
        # Tiny pink nose
        d.polygon([(cx - 1, cy + 1), (cx + 1, cy + 1), (cx, cy + 2)], fill=(255, 140, 160, 255))
        # W mouth ( :3 )
        d.arc([cx - 3, cy + 2, cx, cy + 4], start=0, end=180, fill=(80, 60, 50, 255), width=1)
        d.arc([cx, cy + 2, cx + 3, cy + 4], start=0, end=180, fill=(80, 60, 50, 255), width=1)

        # 6. Whiskers
        d.line([(cx - 16, cy + 1), (cx - 11, cy + 1)], fill=(180, 175, 170, 200), width=1)
        d.line([(cx - 15, cy + 3), (cx - 11, cy + 4)], fill=(180, 175, 170, 200), width=1)
        d.line([(cx + 11, cy + 1), (cx + 16, cy + 1)], fill=(180, 175, 170, 200), width=1)
        d.line([(cx + 11, cy + 4), (cx + 15, cy + 3)], fill=(180, 175, 170, 200), width=1)

        # 7. Front Paws (Bongo / tapping paws)
        d.ellipse([cx - 8, left_paw_y - 2, cx - 3, left_paw_y + 2], fill=(252, 250, 246, 255), outline=(215, 210, 200, 255), width=1)
        d.ellipse([cx + 3, right_paw_y - 2, cx + 8, right_paw_y + 2], fill=(252, 250, 246, 255), outline=(215, 210, 200, 255), width=1)

        # Convert to paletted with transparency
        p_img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        p_img.paste(img, (0, 0))
        alpha = p_img.split()[3]
        mask_p = p_img.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
        mask_p.paste(255, Image.eval(alpha, lambda a: 255 if a == 0 else 0))
        mask_p.info['transparency'] = 255
        frames.append(mask_p)

    out_path = os.path.join(OUTPUT_DIR, "cat.gif")
    frames[0].save(
        out_path,
        save_all=True,
        append_images=frames[1:],
        duration=90,
        loop=0,
        transparency=255,
        disposal=2
    )
    print(f"Generated Cute Cat GIF -> {out_path}")

# ---------------------------------------------------------------------------
# 3. CUTE STITCH (Experiment 626 Small Cute Animation)
# ---------------------------------------------------------------------------
def make_stitch_gif():
    w, h = 54, 34
    frames = []
    total_frames = 18

    for f in range(total_frames):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        # Ear bounce angle and head tilt
        ear_wave = math.sin(f * (2 * math.pi / total_frames)) * 2.5
        head_bob = int(math.sin(f * (2 * math.pi / total_frames)) * 1.2)
        cx, cy = w // 2, h // 2 + head_bob

        # Waving paw on frames 6-12
        is_waving = 6 <= f <= 12
        wave_dx = int(math.sin(f * 2) * 2) if is_waving else 0
        is_blinking = (4 <= f <= 6) or (14 <= f <= 15)

        # Colors
        stitch_blue = (35, 100, 220, 255)       # Main body
        stitch_dark = (22, 65, 160, 255)        # Outline
        stitch_belly = (145, 195, 245, 255)     # Eye surrounds & light belly
        stitch_ear_pink = (210, 85, 145, 255)   # Inner ear
        stitch_nose = (20, 25, 70, 255)         # Navy dark nose

        # 1. Big Iconic Stitch Ears
        # Left Ear (large floppy alien ear with notch)
        left_ear_pts = [
            (cx - 10, cy - 2),
            (cx - 24, cy - 10 + int(ear_wave)),
            (cx - 21, cy + 2 + int(ear_wave)),
            (cx - 8, cy + 4)
        ]
        d.polygon(left_ear_pts, fill=stitch_blue, outline=stitch_dark)
        # Inner pink left
        left_inner_ear = [
            (cx - 11, cy - 1),
            (cx - 21, cy - 7 + int(ear_wave)),
            (cx - 19, cy + 1 + int(ear_wave)),
            (cx - 9, cy + 3)
        ]
        d.polygon(left_inner_ear, fill=stitch_ear_pink)

        # Right Ear
        right_ear_pts = [
            (cx + 10, cy - 2),
            (cx + 24, cy - 10 - int(ear_wave)),
            (cx + 21, cy + 2 - int(ear_wave)),
            (cx + 8, cy + 4)
        ]
        d.polygon(right_ear_pts, fill=stitch_blue, outline=stitch_dark)
        # Inner pink right
        right_inner_ear = [
            (cx + 11, cy - 1),
            (cx + 21, cy - 7 - int(ear_wave)),
            (cx + 19, cy + 1 - int(ear_wave)),
            (cx + 9, cy + 3)
        ]
        d.polygon(right_inner_ear, fill=stitch_ear_pink)

        # 2. Stitch Head
        d.ellipse([cx - 13, cy - 9, cx + 13, cy + 8], fill=stitch_blue, outline=stitch_dark, width=1)

        # Top hair tuft
        d.polygon([(cx - 2, cy - 9), (cx, cy - 13), (cx + 2, cy - 9)], fill=stitch_blue)

        # 3. Eye Patches (lighter blue surround)
        d.ellipse([cx - 11, cy - 5, cx - 2, cy + 4], fill=stitch_belly)
        d.ellipse([cx + 2, cy - 5, cx + 11, cy + 4], fill=stitch_belly)

        # 4. Big Dark Expressive Eyes
        if is_blinking:
            d.arc([cx - 9, cy - 2, cx - 4, cy + 2], start=10, end=170, fill=(15, 20, 50, 255), width=2)
            d.arc([cx + 4, cy - 2, cx + 9, cy + 2], start=10, end=170, fill=(15, 20, 50, 255), width=2)
        else:
            d.ellipse([cx - 9, cy - 4, cx - 4, cy + 2], fill=(12, 16, 45, 255))
            d.ellipse([cx + 4, cy - 4, cx + 9, cy + 2], fill=(12, 16, 45, 255))
            # Catchlight
            d.rectangle([cx - 8, cy - 3, cx - 7, cy - 2], fill=(255, 255, 255, 255))
            d.rectangle([cx + 5, cy - 3, cx + 6, cy - 2], fill=(255, 255, 255, 255))

        # 5. Big Stitch Blue/Navy Nose
        d.ellipse([cx - 4, cy - 1, cx + 4, cy + 3], fill=stitch_nose)

        # 6. Cute Smile / Mouth
        d.arc([cx - 6, cy + 2, cx + 6, cy + 6], start=0, end=180, fill=(15, 20, 60, 255), width=1)

        # 7. Tiny Little Hand Waving (if waving)
        if is_waving:
            paw_x = cx + 12 + wave_dx
            paw_y = cy + 5
            d.ellipse([paw_x - 3, paw_y - 3, paw_x + 3, paw_y + 3], fill=stitch_blue, outline=stitch_dark)
            # tiny claws
            d.point([(paw_x - 2, paw_y - 4), (paw_x, paw_y - 4), (paw_x + 2, paw_y - 4)], fill=(180, 210, 255, 255))

        # Convert to paletted with transparency
        p_img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        p_img.paste(img, (0, 0))
        alpha = p_img.split()[3]
        mask_p = p_img.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
        mask_p.paste(255, Image.eval(alpha, lambda a: 255 if a == 0 else 0))
        mask_p.info['transparency'] = 255
        frames.append(mask_p)

    out_path = os.path.join(OUTPUT_DIR, "stitch.gif")
    frames[0].save(
        out_path,
        save_all=True,
        append_images=frames[1:],
        duration=85,
        loop=0,
        transparency=255,
        disposal=2
    )
    print(f"Generated Cute Stitch GIF -> {out_path}")

# ---------------------------------------------------------------------------
# 4. VEGETA ULTRA EGO (Dragon Ball Super Hakai Transformation - Redesigned v3)
# ---------------------------------------------------------------------------
def make_vegeta_ultra_ego_gif():
    total_frames = 20
    scale = 4
    w, h = 64 * scale, 32 * scale
    raw_frames = []

    for frame_idx in range(total_frames):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        t = frame_idx / total_frames
        power_bob = int(math.sin(t * 2 * math.pi) * 3)
        cx = 88
        cy = 60 + power_bob

        purple_dark = (36, 2, 58, 255)
        purple_base = (145, 12, 200, 255)
        purple_bright = (220, 50, 255, 255)
        purple_glow = (250, 130, 255, 220)
        hakai_magenta = (255, 0, 135, 255)
        skin_base = (248, 204, 174, 255)
        skin_shadow = (200, 142, 116, 255)
        armor_white = (240, 242, 250, 255)
        armor_gold = (238, 178, 28, 255)
        suit_navy = (16, 24, 52, 255)
        black_col = (10, 8, 14, 255)

        # 1. Roaring Hakai Aura (Flames billowing up around Vegeta)
        for ang in range(-80, 85, 12):
            rad = math.radians(ang - 90)
            dist = 56 + int(math.sin(t * 6 * math.pi + ang * 2) * 10)
            fx = cx + int(dist * math.cos(rad))
            fy = cy - 20 + int(dist * 0.85 * math.sin(rad))
            d.ellipse([fx - 16, fy - 18, fx + 16, fy + 18], fill=(145, 12, 215, 75))
            d.ellipse([fx - 8, fy - 10, fx + 8, fy + 10], fill=(225, 60, 255, 110))

        # Inner core aura halo
        d.ellipse([cx - 48, cy - 60, cx + 48, cy + 30], fill=(105, 6, 165, 70))
        d.ellipse([cx - 36, cy - 50, cx + 36, cy + 22], fill=(180, 22, 235, 85))

        # 2. Hakai Lightning Bolts
        if frame_idx in [1, 2, 3, 8, 9, 10, 11]:
            seed = frame_idx * 47
            for _ in range(3):
                lx = cx + int(math.sin(seed) * 42)
                ly = cy - 10 + int(math.cos(seed) * 36)
                d.line([(lx, ly), (lx + 8, ly - 8), (lx + 3, ly - 16), (lx + 14, ly - 24)], 
                       fill=purple_glow, width=2)
                seed += 31

        # 3. Torso & Saiyan Armor (Chest & Shoulders close-up)
        torso_top = cy + 18
        torso_bottom = cy + 66
        d.ellipse([cx - 42, torso_top + 2, cx + 42, torso_bottom], fill=suit_navy)
        
        # White armor plate
        d.polygon([
            (cx - 26, torso_top + 4),
            (cx + 26, torso_top + 4),
            (cx + 20, torso_bottom),
            (cx - 20, torso_bottom)
        ], fill=armor_white, outline=black_col, width=2)
        d.rectangle([cx - 15, torso_bottom - 24, cx + 15, torso_bottom - 18], fill=armor_gold, outline=black_col, width=1)
        d.rectangle([cx - 13, torso_bottom - 14, cx + 13, torso_bottom - 8], fill=armor_gold, outline=black_col, width=1)
        d.line([(cx, torso_top + 4), (cx, torso_bottom - 26)], fill=(190, 195, 210, 255), width=2)

        # 4. Giant Hakai Destruction Sphere (Charging on right hand at x=180)
        charge_pulse = (math.sin(t * 4 * math.pi) + 1) / 2
        sphere_x = cx + 58
        sphere_y = cy + 8
        sphere_r = int(14 + charge_pulse * 6)

        d.line([(cx + 22, torso_top + 10), (sphere_x - 14, sphere_y + 8)], fill=suit_navy, width=12)
        d.ellipse([sphere_x - 18, sphere_y - 2, sphere_x - 6, sphere_y + 16], fill=armor_white, outline=black_col, width=2)

        d.ellipse([sphere_x - sphere_r - 10, sphere_y - sphere_r - 10, sphere_x + sphere_r + 10, sphere_y + sphere_r + 10], 
                  fill=(160, 0, 140, 80))
        d.ellipse([sphere_x - sphere_r - 4, sphere_y - sphere_r - 4, sphere_x + sphere_r + 4, sphere_y + sphere_r + 4], 
                  fill=(220, 20, 180, 130))
        d.ellipse([sphere_x - sphere_r, sphere_y - sphere_r, sphere_x + sphere_r, sphere_y + sphere_r], 
                  fill=purple_dark, outline=hakai_magenta, width=3)
        d.ellipse([sphere_x - sphere_r // 2, sphere_y - sphere_r // 2, sphere_x + sphere_r // 2, sphere_y + sphere_r // 2], 
                  fill=(255, 60, 180, 240))
        d.ellipse([sphere_x - 3, sphere_y - 3, sphere_x + 3, sphere_y + 3], fill=(255, 255, 255, 255))

        for ray_deg in range(0, 360, 45):
            ray_r = math.radians(ray_deg + int(t * 180))
            rx1 = sphere_x + int((sphere_r + 2) * math.cos(ray_r))
            ry1 = sphere_y + int((sphere_r + 2) * math.sin(ray_r))
            rx2 = sphere_x + int((sphere_r + 10) * math.cos(ray_r))
            ry2 = sphere_y + int((sphere_r + 10) * math.sin(ray_r))
            d.line([(rx1, ry1), (rx2, ry2)], fill=purple_glow, width=2)

        # 5. Muscular Neck
        d.polygon([(cx - 14, cy + 12), (cx + 14, cy + 12), (cx + 11, cy + 26), (cx - 11, cy + 26)], fill=skin_shadow)
        d.line([(cx - 5, cy + 14), (cx - 7, cy + 24)], fill=black_col, width=1)
        d.line([(cx + 5, cy + 14), (cx + 7, cy + 24)], fill=black_col, width=1)

        # 6. Face & Jaw
        face_pts = [
            (cx - 22, cy - 14),
            (cx + 22, cy - 14),
            (cx + 24, cy + 4),
            (cx + 17, cy + 18),
            (cx, cy + 25),
            (cx - 17, cy + 18),
            (cx - 24, cy + 4)
        ]
        d.polygon(face_pts, fill=skin_base, outline=black_col, width=2)

        # Left Ear & Hakai Earring
        ear_x = cx - 25
        ear_y = cy + 2
        d.ellipse([ear_x - 4, ear_y - 7, ear_x + 4, ear_y + 7], fill=skin_base, outline=black_col, width=1)
        d.ellipse([ear_x - 3, ear_y + 5, ear_x + 3, ear_y + 9], outline=armor_gold, width=2)
        d.ellipse([ear_x - 3, ear_y + 9, ear_x + 3, ear_y + 14], fill=purple_dark)

        # 7. Ultra Ego Brow Ridge (NO EYEBROWS)
        d.line([(cx - 19, cy - 4), (cx - 3, cy - 1)], fill=skin_shadow, width=6)
        d.line([(cx + 3, cy - 1), (cx + 19, cy - 4)], fill=skin_shadow, width=6)
        d.line([(cx - 18, cy - 4), (cx - 4, cy - 1)], fill=black_col, width=2)
        d.line([(cx + 4, cy - 1), (cx + 18, cy - 4)], fill=black_col, width=2)
        d.line([(cx - 3, cy - 8), (cx - 3, cy - 2)], fill=black_col, width=1)
        d.line([(cx + 3, cy - 8), (cx + 3, cy - 2)], fill=black_col, width=1)

        # 8. Eyes - Bold, Fierce Glowing Magenta Saiyan Glare
        left_eye = [(cx - 18, cy - 2), (cx - 5, cy), (cx - 7, cy + 7), (cx - 16, cy + 5)]
        d.polygon(left_eye, fill=(255, 255, 255, 255), outline=black_col, width=2)
        right_eye = [(cx + 18, cy - 2), (cx + 5, cy), (cx + 7, cy + 7), (cx + 16, cy + 5)]
        d.polygon(right_eye, fill=(255, 255, 255, 255), outline=black_col, width=2)

        d.ellipse([cx - 14, cy, cx - 7, cy + 6], fill=hakai_magenta)
        d.ellipse([cx + 7, cy, cx + 14, cy + 6], fill=hakai_magenta)
        d.ellipse([cx - 12, cy + 1, cx - 9, cy + 5], fill=black_col)
        d.ellipse([cx + 9, cy + 1, cx + 12, cy + 5], fill=black_col)

        # 9. Nose & Gritted Teeth Smirk
        d.line([(cx, cy + 5), (cx - 2, cy + 10), (cx + 2, cy + 10)], fill=black_col, width=1)
        d.polygon([(cx - 9, cy + 15), (cx + 7, cy + 13), (cx + 11, cy + 11), (cx + 4, cy + 16)], fill=(255, 255, 255, 255), outline=black_col, width=1)
        d.line([(cx - 3, cy + 15), (cx + 5, cy + 14)], fill=black_col, width=1)
        d.line([(cx - 2, cy + 19), (cx + 3, cy + 19)], fill=skin_shadow, width=2)

        # 10. Massive Flaming Spiky Saiyan Hair
        hair_outline_pts = [
            (cx, cy - 6),
            (cx - 13, cy - 14),
            (cx - 20, cy - 12),
            (cx - 25, cy - 16),
            (cx - 50, cy - 38),
            (cx - 28, cy - 26),
            (cx - 42, cy - 54),
            (cx - 20, cy - 32),
            (cx - 22, cy - 68),
            (cx - 4, cy - 38),
            (cx - 6, cy - 72),
            (cx + 6, cy - 38),
            (cx + 20, cy - 68),
            (cx + 20, cy - 32),
            (cx + 42, cy - 54),
            (cx + 28, cy - 26),
            (cx + 50, cy - 38),
            (cx + 25, cy - 16),
            (cx + 20, cy - 12),
            (cx + 13, cy - 14),
            (cx, cy - 6)
        ]
        d.polygon(hair_outline_pts, fill=purple_base, outline=black_col, width=2)

        inner_shadows = [
            [(cx - 4, cy - 70), (cx - 16, cy - 66), (cx - 4, cy - 38)],
            [(cx + 4, cy - 66), (cx + 16, cy - 64), (cx + 6, cy - 38)],
            [(cx - 24, cy - 26), (cx - 38, cy - 50), (cx - 20, cy - 32)],
            [(cx + 24, cy - 26), (cx + 38, cy - 50), (cx + 20, cy - 32)],
        ]
        for spk in inner_shadows:
            d.polygon(spk, fill=purple_dark)

        d.line([(cx - 3, cy - 66), (cx - 12, cy - 30)], fill=purple_bright, width=3)
        d.line([(cx + 4, cy - 62), (cx + 12, cy - 28)], fill=purple_bright, width=3)
        d.line([(cx - 34, cy - 48), (cx - 18, cy - 22)], fill=purple_bright, width=2)
        d.line([(cx + 34, cy - 48), (cx + 18, cy - 22)], fill=purple_bright, width=2)
        d.line([(cx - 6, cy - 70), (cx, cy - 72), (cx + 6, cy - 68)], fill=purple_glow, width=2)

        final_img = img.resize((64, 32), Image.Resampling.LANCZOS)
        raw_frames.append(final_img)

    frames = []
    for f in raw_frames:
        alpha = f.split()[3]
        p_img = f.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
        p_img.paste(255, Image.eval(alpha, lambda a: 255 if a == 0 else 0))
        p_img.info['transparency'] = 255
        frames.append(p_img)

    out_path = os.path.join(OUTPUT_DIR, "vegeta.gif")
    frames[0].save(
        out_path,
        save_all=True,
        append_images=frames[1:],
        duration=75,
        loop=0,
        transparency=255,
        disposal=2
    )
    print(f"Generated Upgraded Vegeta Ultra Ego GIF -> {out_path}")

# ---------------------------------------------------------------------------
# 5. SAKURA FOREST (Minecraft Cherry Blossom Grove Biome)
# ---------------------------------------------------------------------------
def make_sakura_minecraft_gif():
    w, h = 64, 32
    frames = []
    total_frames = 24

    grass_top = (118, 178, 64, 255)
    grass_side = (98, 150, 52, 255)
    dirt_col = (134, 96, 67, 255)
    wood_trunk = (58, 28, 38, 255)
    wood_bark = (42, 18, 26, 255)
    
    leaf_deep = (219, 78, 138, 255)
    leaf_mid = (244, 122, 176, 255)
    leaf_light = (252, 182, 214, 255)
    leaf_highlight = (255, 218, 235, 255)
    petal_pink = (244, 114, 182, 255)
    petal_soft = (251, 207, 232, 255)

    for frame_idx in range(total_frames):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        t = frame_idx / total_frames

        # 1. Ground - Blocky Grass & Dirt (Bottom y=27 to 31)
        d.rectangle([0, 29, 63, 31], fill=dirt_col)
        d.rectangle([0, 27, 63, 28], fill=grass_top)
        for gx in range(0, 64, 3):
            d.rectangle([gx, 29, gx + 1, 29], fill=grass_side)

        # Petal carpets on grass
        carpets = [(4, 26), (12, 26), (22, 26), (42, 26), (54, 26)]
        for cx, cy in carpets:
            d.rectangle([cx, cy, cx + 4, cy], fill=petal_pink)
            d.rectangle([cx + 1, cy, cx + 2, cy], fill=petal_soft)

        # 2. Minecraft Cherry Tree Trunk
        tx, ty = 22, 14
        d.rectangle([tx, ty + 4, tx + 4, 27], fill=wood_trunk, outline=wood_bark)
        d.rectangle([tx - 2, 26, tx, 27], fill=wood_trunk)
        d.rectangle([tx + 4, 26, tx + 5, 27], fill=wood_trunk)
        d.rectangle([tx - 4, ty + 2, tx, ty + 6], fill=wood_trunk)
        d.rectangle([tx + 4, ty + 1, tx + 8, ty + 5], fill=wood_trunk)

        # 3. Minecraft Cherry Leaf Blocks (Cubic clusters with wind sway)
        sway = int(math.sin(t * 2 * math.pi) * 1.0)
        leaf_blocks = [
            (tx - 12 + sway, ty - 12, 28, 16),
            (tx - 8 + sway, ty - 14, 20, 8),
            (tx - 15 + sway, ty - 6, 12, 12),
            (tx + 6 + sway, ty - 8, 14, 13),
            (tx - 6 + sway, ty - 2, 16, 10),
        ]

        for lx, ly, lw, lh in leaf_blocks:
            d.rectangle([lx, ly, lx + lw, ly + lh], fill=leaf_deep)
            d.rectangle([lx + 1, ly + 1, lx + lw - 1, ly + lh - 2], fill=leaf_mid)
            d.rectangle([lx + 1, ly + 1, lx + lw - 1, ly + 3], fill=leaf_light)
            for px in range(lx + 2, lx + lw - 2, 4):
                for py in range(ly + 3, ly + lh - 3, 4):
                    d.rectangle([px, py, px + 1, py + 1], fill=leaf_highlight)
                    d.rectangle([px + 2, py + 2, px + 3, py + 3], fill=leaf_deep)

        # 4. Animated Falling Sakura Petals (Minecraft 1.20 particle drift)
        petals = [
            (55, -2, 1.2, 1.4, 2),
            (38, -4, 1.0, 1.1, 2),
            (24, 0, 1.3, 1.5, 1),
            (48, 8, 0.9, 1.2, 2),
            (15, 6, 1.1, 1.3, 1),
            (32, 12, 1.4, 1.6, 2),
            (58, 16, 1.0, 1.2, 1),
        ]

        for px0, py0, sx, sy, psize in petals:
            dist_y = 36
            curr_y = (py0 + t * dist_y * sy) % 32
            curr_x = (px0 - t * 24 * sx + math.sin(t * 4 * math.pi + px0) * 3) % 64
            col = petal_pink if psize > 1 else petal_soft
            d.rectangle([int(curr_x), int(curr_y), int(curr_x) + psize, int(curr_y) + 1], fill=col)

        # 5. Cute Miniature Minecraft Bee (Hovering on the right)
        bee_hover_y = math.sin(t * 6 * math.pi) * 2.0
        bx = 48
        by = 13 + int(bee_hover_y)

        wing_up = (frame_idx % 4 < 2)
        wing_col = (255, 255, 255, 190)
        if wing_up:
            d.rectangle([bx + 1, by - 3, bx + 4, by - 1], fill=wing_col)
        else:
            d.rectangle([bx + 1, by - 1, bx + 5, by], fill=wing_col)

        d.rectangle([bx, by, bx + 6, by + 4], fill=(245, 180, 30, 255))
        d.rectangle([bx + 2, by, bx + 3, by + 4], fill=(60, 25, 15, 255))
        d.rectangle([bx + 5, by, bx + 6, by + 4], fill=(60, 25, 15, 255))
        d.rectangle([bx, by + 1, bx + 1, by + 2], fill=(60, 190, 230, 255))
        d.rectangle([bx + 7, by + 2, bx + 7, by + 2], fill=(40, 15, 10, 255))

        # Convert to paletted with transparency
        alpha = img.split()[3]
        mask_p = img.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
        mask_p.paste(255, Image.eval(alpha, lambda a: 255 if a == 0 else 0))
        mask_p.info['transparency'] = 255
        frames.append(mask_p)

    out_path = os.path.join(OUTPUT_DIR, "sakura.gif")
    frames[0].save(
        out_path,
        save_all=True,
        append_images=frames[1:],
        duration=80,
        loop=0,
        transparency=255,
        disposal=2
    )
    print(f"Generated Sakura Forest (Minecraft) GIF -> {out_path}")

if __name__ == "__main__":
    make_spiderverse_gif()
    make_cat_gif()
    make_stitch_gif()
    make_vegeta_ultra_ego_gif()
    make_sakura_minecraft_gif()
    print("All 5 Dynamic Island GIF templates generated successfully!")
