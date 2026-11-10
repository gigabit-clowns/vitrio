# SPDX-License-Identifier: LGPL-2.1-or-later

"""vitrio — final logo system (F2 · S2 clamp · H2 head stack · P6 Frost).

Everything is generated from code: real knock-out holes (evenodd / masks) so transparent
variants work on any background; text outlined via fontTools so no SVG depends on fonts.
Run:  python3 source/generate.py  -> rewrites the kit in place
"""
import math, pathlib, itertools, subprocess, shutil

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

# Writes into the kit folder this script lives in (…/source/../), overwriting files in place.
DIST = pathlib.Path(__file__).resolve().parent.parent
FONT_DIR = pathlib.Path("/usr/share/fonts/opentype/inter")

# ------------------------------------------------------------------ palette (P6 Frost)
FROST = "#e9f5f9"   # light tile / light-on-dark
NAVY = "#0e1a22"    # ink, dark tile
TEAL = "#1786aa"    # tracks on light
DEEP = "#0e3a4c"    # clamp on light
ICE = "#8fe3f5"     # tracks on dark
GRAPHITE = "#1e2327"  # repo theme, dark backgrounds
MUTED_L = "#4f6672"   # secondary text on light
MUTED_D = "#9fb4bf"   # secondary text on dark

CTX = {
    "light": dict(track=TEAL, hub=DEEP, arm=NAVY, word=NAVY, mono=False),
    "dark": dict(track=ICE, hub=FROST, arm=FROST, word=FROST, mono=False),
    "black": dict(track="#000000", hub="#000000", arm="#000000", word="#000000", mono=True),
    "white": dict(track="#ffffff", hub="#ffffff", arm="#ffffff", word="#ffffff", mono=True),
}

_ids = itertools.count()


def nid(p):
    return f"vt-{p}{next(_ids)}"


def circ(cx, cy, r, ccw=False):
    """Circle as a path sub-path (for evenodd holes)."""
    sweep = 0 if ccw else 1
    return (f"M{cx - r:.3f} {cy:.3f}a{r:.3f} {r:.3f} 0 1 {sweep} {2*r:.3f} 0"
            f"a{r:.3f} {r:.3f} 0 1 {sweep} {-2*r:.3f} 0Z")


# ------------------------------------------------------------------ parts
def tracks(cx, cy, R, C, n, outer_w, widths):
    rs = [R * math.sqrt(k / n) for k in range(1, n)]
    ops = (0.5, 0.62, 0.74, 0.86) if not C["mono"] else (1, 1, 1, 1)
    g = "".join(f'<circle cx="{cx}" cy="{cy}" r="{r:.3f}" fill="none" stroke="{C["track"]}" '
                f'stroke-width="{w}"' + (f' opacity="{o}"' if o != 1 else "") + "/>"
                for r, w, o in zip(rs, widths, ops))
    return g + f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" stroke="{C["track"]}" stroke-width="{outer_w}"/>'


def clamp(cx, cy, C, s=1.0, screws=True):
    """6-screw HDD platter clamp, holes knocked out (evenodd)."""
    d = circ(cx, cy, 10.5 * s)
    if screws:
        for k in range(6):
            a = math.radians(k * 60 + 30)
            d += circ(cx + 6.6 * s * math.cos(a), cy + 6.6 * s * math.sin(a), 1.45 * s)
    d += circ(cx, cy, 2.4 * s)
    return f'<path d="{d}" fill="{C["hub"]}" fill-rule="evenodd"/>'


PIVOT, HEAD = (93, 91), (25, 57)


def _arm_geometry():
    px, py = PIVOT; hx, hy = HEAD
    dx, dy = hx - px, hy - py
    L = math.hypot(dx, dy); ux, uy = dx / L, dy / L; nx, ny = -uy, ux
    w0, w1 = 14, 3
    poly = " ".join(f"{x:.3f},{y:.3f}" for x, y in [
        (px + nx*w0/2, py + ny*w0/2), (hx + nx*w1/2, hy + ny*w1/2),
        (hx - nx*w1/2, hy - ny*w1/2), (px - nx*w0/2, py - ny*w0/2)])
    ang = math.degrees(math.atan2(dy, dx))
    back = math.atan2(dy, dx) + math.pi
    r = 20
    a1, a2 = back - math.radians(40), back + math.radians(40)
    coil = (f"M{px} {py}L{px + r*math.cos(a1):.3f} {py + r*math.sin(a1):.3f}"
            f"A{r} {r} 0 0 1 {px + r*math.cos(a2):.3f} {py + r*math.sin(a2):.3f}Z")
    slot = f"M{px + ux*16:.3f} {py + uy*16:.3f}L{px + ux*34:.3f} {py + uy*34:.3f}"
    return dict(px=px, py=py, hx=hx, hy=hy, poly=poly, ang=ang, coil=coil, slot=slot)


def arm_shapes(fill, halo=False):
    G = _arm_geometry()
    hs = ' stroke="#000" stroke-width="7" stroke-linejoin="round"' if halo else ""
    cw = 11 if halo else 4
    return (f'<path d="{G["coil"]}" fill="none" stroke="{"#000" if halo else fill}" stroke-width="{cw}" stroke-linejoin="round"/>'
            f'<polygon points="{G["poly"]}" fill="{fill}"{hs}/>'
            f'<rect x="{G["hx"]-5}" y="{G["hy"]-3.5}" width="10" height="7" rx="1.5" fill="{fill}"{hs} '
            f'transform="rotate({G["ang"]:.2f} {G["hx"]} {G["hy"]})"/>'
            f'<circle cx="{G["px"]}" cy="{G["py"]}" r="8.5" fill="{fill}"{hs}/>')


def arm(C):
    """Head stack with knocked-out lightening slot and bearing ring."""
    G = _arm_geometry()
    m = nid("arm")
    mask = (f'<mask id="{m}" maskUnits="userSpaceOnUse" x="-10" y="-10" width="140" height="140">'
            f'<rect x="-10" y="-10" width="140" height="140" fill="#fff"/>'
            f'<path d="{G["slot"]}" stroke="#000" stroke-width="3.2" stroke-linecap="round"/>'
            f'<circle cx="{G["px"]}" cy="{G["py"]}" r="3.6" fill="none" stroke="#000" stroke-width="1.8"/></mask>')
    return f'<defs>{mask}</defs><g mask="url(#{m})">{arm_shapes(C["arm"])}</g>'


def mark(C, tile=None):
    """Full isotype in a 120×120 box. tile = fill colour or None (transparent)."""
    m = nid("halo")
    halo = (f'<mask id="{m}" maskUnits="userSpaceOnUse" x="-10" y="-10" width="140" height="140">'
            f'<rect x="-10" y="-10" width="140" height="140" fill="#fff"/>{arm_shapes("#000", halo=True)}</mask>')
    g = f'<defs>{halo}</defs>'
    if tile:
        g += f'<rect width="120" height="120" rx="26" fill="{tile}"/>'
    g += (f'<g mask="url(#{m})">{tracks(50, 50, 38, C, 5, 6, (2.2, 2.8, 3.4, 4.0))}{clamp(50, 50, C)}</g>'
          + arm(C))
    return g


def mark_small(C, tile=None):
    """Arm-less platter for ≤ 32 px (favicons)."""
    g = f'<rect width="120" height="120" rx="26" fill="{tile}"/>' if tile else ""
    return g + tracks(60, 60, 40, C, 4, 10, (4, 4.6, 5.2)) + clamp(60, 60, C, s=1.25)


# transparent mark visual bounds inside its 120 box (measured, see measure())
MARK_BOX = None

# ------------------------------------------------------------------ wordmark
SW = 8


def o_motif(C, cx=168, cy=40):
    ops = (0.75, 0.9) if not C["mono"] else (1, 1)
    g = "".join(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{C["track"]}" stroke-width="{w}"'
                + (f' opacity="{o}"' if o != 1 else "") + "/>"
                for r, w, o in [(11.2, 2.4, ops[0]), (14.6, 2.6, ops[1])])
    g += f'<circle cx="{cx}" cy="{cy}" r="20" fill="none" stroke="{C["word"]}" stroke-width="{SW}"/>'
    g += f'<path d="{circ(cx, cy, 6.4)}{circ(cx, cy, 1.9)}" fill="{C["hub"]}" fill-rule="evenodd"/>'
    return g


def wordmark(C):
    w = C["word"]
    p = f'stroke="{w}" stroke-width="{SW}" stroke-linecap="round" stroke-linejoin="round" fill="none"'
    return (f'<path d="M2 20L15 60L28 20" {p}/>'
            f'<path d="M46 20V60" {p}/><circle cx="46" cy="6" r="5.2" fill="{w}"/>'
            f'<path d="M66 6V48Q66 60 78 60M58 22H78" {p}/>'
            f'<path d="M96 60V20M96 40Q96 20 116 20" {p}/>'
            f'<path d="M132 20V60" {p}/><circle cx="132" cy="6" r="5.2" fill="{w}"/>'
            + o_motif(C))


WORD_BOX = (-2, 0.8, 194, 63.2)  # x, y, w, h (stroke-inclusive)

# ------------------------------------------------------------------ text → paths
_fonts = {}


def text_path(txt, size, weight="Regular", x=0, y=0, fill="#000", anchor="start", tracking=0):
    """Outline text with Inter via fontTools. Returns (svg, width)."""
    if weight not in _fonts:
        _fonts[weight] = TTFont(FONT_DIR / f"Inter-{weight}.otf")
    f = _fonts[weight]
    gs, cmap = f.getGlyphSet(), f.getBestCmap()
    upm = f["head"].unitsPerEm
    s = size / upm
    hmtx = f["hmtx"]
    adv = [hmtx[cmap.get(ord(ch), ".notdef")][0] * s + tracking for ch in txt]
    width = sum(adv) - (tracking if txt else 0)
    x0 = x - (width if anchor == "end" else width / 2 if anchor == "middle" else 0)
    ds, cx = [], x0
    for ch, a in zip(txt, adv):
        gname = cmap.get(ord(ch), ".notdef")
        pen = SVGPathPen(gs)
        gs[gname].draw(TransformPen(pen, (s, 0, 0, -s, cx, y)))
        if pen.getCommands():
            ds.append(pen.getCommands())
        cx += a
    return f'<path d="{" ".join(ds)}" fill="{fill}"/>', width


# ------------------------------------------------------------------ files
def svg(w, h, body, title="vitrio", vb=None):
    vb = vb or f"0 0 {w} {h}"
    title = title.replace("&", "&amp;")
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w:g}" height="{h:g}" viewBox="{vb}" '
            f'role="img" aria-label="{title}"><title>{title}</title>{body}</svg>\n')


def write(rel, content):
    p = DIST / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(content)
    return p


def measure_mark():
    """Measure the transparent mark's ink bounds (render & trim) to build tight viewBoxes."""
    tmp = pathlib.Path("/tmp/vt-measure.svg")
    tmp.write_text(svg(1200, 1200, mark(CTX["black"]), vb="0 0 120 120"))
    png = "/tmp/vt-measure.png"
    subprocess.run(["rsvg-convert", str(tmp), "-o", png], check=True)
    out = subprocess.run(["convert", png, "-trim", "-format", "%w %h %X %Y", "info:"],
                         capture_output=True, text=True, check=True).stdout.split()
    w, h, X, Y = (int(v.strip("+")) for v in out)
    return X / 10, Y / 10, w / 10, h / 10


def lockup_h(C, pad=0):
    """Horizontal lockup: mark + wordmark. Returns (body, w, h)."""
    mx, my, mw, mh = MARK_BOX
    H = 100.0                                  # mark height in output units
    ms = H / mh
    gap = 20
    ws = 1.12                                  # wordmark scale -> x-height 44.8
    wx = pad + mw * ms + gap
    ww, wh = WORD_BOX[2] * ws, WORD_BOX[3] * ws
    wy = pad + (H - wh) / 2 + 4                # optical: sit slightly low (ascenders)
    body = (f'<g transform="translate({pad - mx*ms:.3f} {pad - my*ms:.3f}) scale({ms:.4f})">{mark(C)}</g>'
            f'<g transform="translate({wx - WORD_BOX[0]*ws:.3f} {wy - WORD_BOX[1]*ws:.3f}) scale({ws})">{wordmark(C)}</g>')
    return body, wx + ww + pad, H + 2 * pad


def lockup_v(C, pad=0):
    """Stacked lockup: mark above wordmark."""
    mx, my, mw, mh = MARK_BOX
    ws = 1.0
    ww, wh = WORD_BOX[2] * ws, WORD_BOX[3] * ws
    H = 130.0
    ms = H / mh
    W = max(ww, mw * ms)
    gap = 26
    body = (f'<g transform="translate({pad + (W - mw*ms)/2 - mx*ms:.3f} {pad - my*ms:.3f}) scale({ms:.4f})">{mark(C)}</g>'
            f'<g transform="translate({pad + (W - ww)/2 - WORD_BOX[0]*ws:.3f} {pad + H + gap - WORD_BOX[1]*ws:.3f}) scale({ws})">{wordmark(C)}</g>')
    return body, W + 2 * pad, H + gap + wh + 2 * pad


TAGLINE = "High-throughput cryo-EM image I/O for C++ & Python"
FORMATS = "MRC  ·  TIFF  ·  Zarr  ·  EER  ·  DM3/DM4"


def banner(mode):
    """README header, 1280×320, light or dark."""
    C = CTX[mode]
    bg = "#ffffff" if mode == "light" else GRAPHITE
    sub = MUTED_L if mode == "light" else MUTED_D
    W, H = 1280, 320
    body, lw, lh = lockup_h(C)
    s = 1.25
    lx = (W - lw * s) / 2
    ly = 52
    t1, _ = text_path(TAGLINE, 30, "Medium", W / 2, ly + lh * s + 60, sub, "middle")
    t2, _ = text_path(FORMATS, 18, "SemiBold", W / 2, ly + lh * s + 98, C["track"], "middle", tracking=1.2)
    return svg(W, H, f'<rect width="{W}" height="{H}" rx="0" fill="{bg}"/>'
               f'<g transform="translate({lx:.2f} {ly}) scale({s})">{body}</g>{t1}{t2}',
               title="vitrio — " + TAGLINE)


def social():
    """GitHub social preview 1280×640 (upload the PNG). Dark, with grid-like Thon backdrop."""
    W, H = 1280, 640
    C = CTX["dark"]
    # big faint platter bleeding off the right edge
    rings = "".join(f'<circle cx="1120" cy="560" r="{620*math.sqrt(k/9):.1f}" fill="none" stroke="{ICE}" '
                    f'stroke-width="{3+k*0.8:.1f}" opacity="{0.05+0.012*k:.3f}"/>' for k in range(1, 10))
    body, lw, lh = lockup_h(C)
    s = 1.9
    lx, ly = 110, 170
    t1, _ = text_path("High-throughput cryo-EM image I/O", 44, "SemiBold", lx, ly + lh*s + 80, FROST)
    t2, _ = text_path("for C++ & Python", 44, "SemiBold", lx, ly + lh*s + 136, MUTED_D)
    t3, _ = text_path("github.com/gigabit-clowns/vitrio", 22, "Medium", lx, H - 56, ICE)
    return svg(W, H, f'<rect width="{W}" height="{H}" fill="{GRAPHITE}"/>{rings}'
               f'<g transform="translate({lx} {ly}) scale({s})">{body}</g>{t1}{t2}{t3}',
               title="vitrio social preview")


def favicon_auto():
    """SVG favicon that follows the OS colour scheme."""
    light = mark_small(CTX["light"], tile=FROST)
    dark = mark_small(CTX["dark"], tile=NAVY)
    css = ("<style>.d{display:none}@media (prefers-color-scheme:dark){.l{display:none}.d{display:inline}}</style>")
    return svg(32, 32, f'{css}<g class="l">{light}</g><g class="d">{dark}</g>', vb="0 0 120 120",
               title="vitrio")


def main():
    global MARK_BOX
    MARK_BOX = measure_mark()
    mx, my, mw, mh = MARK_BOX
    pad = 4
    mvb = f"{mx-pad:.1f} {my-pad:.1f} {mw+2*pad:.1f} {mh+2*pad:.1f}"

    # --- isotype ---
    write("isotype/vitrio-icon.svg", svg(512, 512, mark(CTX["light"], tile=FROST), vb="0 0 120 120"))
    write("isotype/vitrio-icon-dark.svg", svg(512, 512, mark(CTX["dark"], tile=NAVY), vb="0 0 120 120"))
    for k in ("light", "dark", "black", "white"):
        name = {"light": "vitrio-mark", "dark": "vitrio-mark-on-dark",
                "black": "vitrio-mark-mono-black", "white": "vitrio-mark-mono-white"}[k]
        write(f"isotype/{name}.svg", svg(round((mw+2*pad)*4), round((mh+2*pad)*4), mark(CTX[k]), vb=mvb))

    # --- favicon / small ---
    write("favicon/favicon.svg", favicon_auto())
    write("favicon/favicon-light.svg", svg(32, 32, mark_small(CTX["light"], tile=FROST), vb="0 0 120 120"))
    write("favicon/favicon-dark.svg", svg(32, 32, mark_small(CTX["dark"], tile=NAVY), vb="0 0 120 120"))
    write("favicon/mask-icon.svg", svg(16, 16, mark_small(CTX["black"]), vb="0 0 120 120"))

    # --- wordmark ---
    x, y, w, h = WORD_BOX
    for k, name in [("light", "vitrio-wordmark"), ("dark", "vitrio-wordmark-on-dark"),
                    ("black", "vitrio-wordmark-mono-black"), ("white", "vitrio-wordmark-mono-white")]:
        write(f"wordmark/{name}.svg", svg(round(w*2), round(h*2), wordmark(CTX[k]), vb=f"{x} {y} {w} {h}"))

    # --- lockups ---
    for k, suf in [("light", ""), ("dark", "-on-dark"), ("black", "-mono-black"), ("white", "-mono-white")]:
        body, W, H = lockup_h(CTX[k], pad=6)
        write(f"lockup/vitrio-logo-horizontal{suf}.svg", svg(round(W*2), round(H*2), body, vb=f"0 0 {W:.2f} {H:.2f}"))
        body, W, H = lockup_v(CTX[k], pad=6)
        write(f"lockup/vitrio-logo-stacked{suf}.svg", svg(round(W*2), round(H*2), body, vb=f"0 0 {W:.2f} {H:.2f}"))

    # --- GitHub / docs ---
    write("github/readme-banner-light.svg", banner("light"))
    write("github/readme-banner-dark.svg", banner("dark"))
    write("github/social-preview.svg", social())
    write("github/avatar.svg", svg(500, 500, f'<rect width="120" height="120" fill="{FROST}"/>'
                                   f'<g transform="translate(12 12) scale(0.8)">{mark(CTX["light"])}</g>',
                                   vb="0 0 120 120"))
    print("mark box", MARK_BOX)


if __name__ == "__main__":
    main()


# ------------------------------------------------------------------ extras: brand sheet, rasters
def brand_sheet():
    W, H = 1400, 980
    T = lambda s, sz, x, y, fill=NAVY, wt="Regular", a="start": text_path(s, sz, wt, x, y, fill, a)[0]
    out = [f'<rect width="{W}" height="{H}" fill="#f6f9fa"/>',
           T("vitrio — logo system", 34, 60, 78, NAVY, "Bold"),
           T("Cryo-EM grid as an HDD platter: Thon-ring tracks (√k spacing), 6-screw spindle clamp, head stack reading the sample.", 16, 60, 108, MUTED_L)]
    # hero lockups
    body, lw, lh = lockup_h(CTX["light"])
    out.append(f'<rect x="60" y="140" width="620" height="230" rx="18" fill="#fff" stroke="#dde6ea"/>'
               f'<g transform="translate({60 + (620 - lw*1.15)/2:.1f} {255 - lh*1.15/2:.1f}) scale(1.15)">{body}</g>')
    body, lw, lh = lockup_h(CTX["dark"])
    out.append(f'<rect x="720" y="140" width="620" height="230" rx="18" fill="{GRAPHITE}"/>'
               f'<g transform="translate({720 + (620 - lw*1.15)/2:.1f} {255 - lh*1.15/2:.1f}) scale(1.15)">{body}</g>')
    # icons row
    y0 = 410
    out.append(T("Isotype", 18, 60, y0, NAVY, "SemiBold"))
    items = [(mark(CTX["light"], tile=FROST), None), (mark(CTX["dark"], tile=NAVY), None),
             (mark(CTX["light"]), "#ffffff"), (mark(CTX["dark"]), GRAPHITE),
             (mark(CTX["black"]), "#ffffff"), (mark(CTX["white"]), "#000000")]
    for i, (m, bg) in enumerate(items):
        x = 60 + i * 150
        if bg:
            out.append(f'<rect x="{x}" y="{y0+20}" width="130" height="130" rx="14" fill="{bg}" stroke="#dde6ea"/>')
            out.append(f'<g transform="translate({x+11} {y0+31}) scale(0.9)">{m}</g>')
        else:
            out.append(f'<g transform="translate({x} {y0+20}) scale(1.0833)">{m}</g>')
    # favicon sizes
    fx = 60 + 6 * 150 + 20
    out.append(T("Favicon (arm-less ≤ 32 px)", 18, fx, y0, NAVY, "SemiBold"))
    for i, s in enumerate((64, 32, 16)):
        out.append(f'<g transform="translate({fx + [0, 80, 126][i]} {y0 + 20 + (64 - s)}) scale({s/120:.4f})">{mark_small(CTX["light"], tile=FROST)}</g>')
        out.append(f'<g transform="translate({fx + 200 + [0, 80, 126][i]} {y0 + 20 + (64 - s)}) scale({s/120:.4f})">{mark_small(CTX["dark"], tile=NAVY)}</g>')
    # palette
    y1 = 620
    out.append(T("Palette — Frost", 18, 60, y1, NAVY, "SemiBold"))
    sw = [(FROST, "Frost", "tile · on-dark ink"), (NAVY, "Navy", "ink · dark tile"), (TEAL, "Teal", "tracks on light"),
          (DEEP, "Deep", "clamp on light"), (ICE, "Ice", "tracks on dark"), (GRAPHITE, "Graphite", "dark backgrounds")]
    for i, (c, n, u) in enumerate(sw):
        x = 60 + i * 215
        out.append(f'<rect x="{x}" y="{y1+20}" width="195" height="90" rx="12" fill="{c}" stroke="#d3dde2"/>')
        out.append(T(n, 15, x, y1 + 134, NAVY, "SemiBold"))
        out.append(T(c.upper() + "  ·  " + u, 12.5, x, y1 + 154, MUTED_L))
    # rules
    y2 = 830
    out.append(T("Usage", 18, 60, y2, NAVY, "SemiBold"))
    rules = ["Clear space ≥ the clamp diameter on every side.  Minimum size: isotype 20 px, horizontal lockup 24 px tall.",
             "Below 32 px use the favicon (arm-less) mark.  On photos or busy backgrounds use the tile icon.",
             "Don't recolour individual parts, add effects, rotate the arm, or re-set the wordmark in a font."]
    for i, r in enumerate(rules):
        out.append(T("•  " + r, 14.5, 60, y2 + 32 + i * 26, "#33454f"))
    return svg(W, H, "".join(out), title="vitrio logo system")


def rasters():
    """PNG/ICO exports where platforms refuse SVG (GitHub social/avatar, ICO, touch icons)."""
    png = DIST / "png"; png.mkdir(exist_ok=True)
    jobs = [("github/social-preview.svg", "social-preview-1280x640.png", ["-w", "1280"]),
            ("github/avatar.svg", "avatar-500.png", ["-w", "500"]),
            ("isotype/vitrio-icon.svg", "apple-touch-icon-180.png", ["-w", "180"]),
            ("isotype/vitrio-icon.svg", "icon-192.png", ["-w", "192"]),
            ("isotype/vitrio-icon.svg", "icon-512.png", ["-w", "512"]),
            ("lockup/vitrio-logo-horizontal.svg", "vitrio-logo-horizontal-600.png", ["-w", "600"]),
            ("lockup/vitrio-logo-horizontal-on-dark.svg", "vitrio-logo-horizontal-on-dark-600.png", ["-w", "600"])]
    for src, dst, args in jobs:
        subprocess.run(["rsvg-convert", *args, str(DIST / src), "-o", str(png / dst)], check=True)
    tmp = []
    for s in (16, 32, 48):
        t = f"/tmp/vt-fav{s}.png"
        subprocess.run(["rsvg-convert", "-w", str(s), str(DIST / "favicon/favicon-light.svg"), "-o", t], check=True)
        tmp.append(t)
    subprocess.run(["convert", *tmp, str(DIST / "favicon/favicon.ico")], check=True)


if __name__ == "__main__":
    write("vitrio-brand-sheet.svg", brand_sheet())
    rasters()
