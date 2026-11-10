# vitrio — logo kit

A cryo-EM grid drawn as an HDD platter. The tracks are Thon rings (radii ∝ √k), the spindle is a 6-screw platter clamp, and a head stack with its voice coil reads the sample. The "o" of the wordmark repeats the platter.

Everything is plain SVG with no font dependency (the wordmark is built from strokes and the text is outlined). Holes are real knock-outs, so the transparent marks work on any background.

## What's inside

| Folder | Files | Use for |
|---|---|---|
| `isotype/` | `vitrio-icon.svg` (Frost tile), `vitrio-icon-dark.svg` | App icon, docs sidebar, PyPI/conda avatar |
| | `vitrio-mark.svg`, `vitrio-mark-on-dark.svg` | Transparent mark on light / dark |
| | `vitrio-mark-mono-black.svg`, `-mono-white.svg` | One-colour: print, stickers, engraving |
| `wordmark/` | `vitrio-wordmark*.svg` | Name only (light, on-dark, mono) |
| `lockup/` | `vitrio-logo-horizontal*.svg` | **Primary logo.** README, docs header, slides |
| | `vitrio-logo-stacked*.svg` | Square-ish spaces, title slides, posters |
| `favicon/` | `favicon.svg` (follows OS light/dark), `favicon.ico` (16/32/48), `favicon-light/dark.svg`, `mask-icon.svg` | Browser tabs, docs site |
| `github/` | `readme-banner-light/dark.svg` (1280×320) | README header |
| | `social-preview.svg` (1280×640), `avatar.svg` | Repo social card, org avatar |
| `png/` | Social preview, avatar, touch/PWA icons, 600 px lockups | Places that refuse SVG |
| `source/generate.py` | Generator | Re-run to tweak anything |

## Snippets

**README header (switches with GitHub's theme)**

```html
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/_static/readme-banner-dark.svg">
    <img alt="vitrio — High-throughput cryo-EM image I/O for C++ & Python" src="docs/_static/readme-banner-light.svg" width="800">
  </picture>
</p>
```

**Sphinx + pydata-sphinx-theme / furo**

```python
html_favicon = "_static/favicon.svg"
# pydata-sphinx-theme
html_theme_options = {"logo": {"image_light": "_static/vitrio-logo-horizontal.svg",
                               "image_dark": "_static/vitrio-logo-horizontal-on-dark.svg"}}
# furo
html_theme_options = {"light_logo": "vitrio-logo-horizontal.svg",
                      "dark_logo": "vitrio-logo-horizontal-on-dark.svg"}
```

**MkDocs Material**

```yaml
theme:
  logo: assets/vitrio-mark-on-dark.svg   # header bar is coloured
  favicon: assets/favicon.svg
```

**Plain HTML `<head>`**

```html
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="icon" href="/favicon.svg" type="image/svg+xml">
<link rel="apple-touch-icon" href="/apple-touch-icon-180.png">
```

**GitHub social preview:** go to Settings → General → Social preview → upload `png/social-preview-1280x640.png`. GitHub only accepts PNG/JPG here.

## Palette — Frost

| Name | Hex | Role |
|---|---|---|
| Frost | `#E9F5F9` | Light tile, ink on dark |
| Navy | `#0E1A22` | Ink, dark tile |
| Teal | `#1786AA` | Tracks on light |
| Deep | `#0E3A4C` | Clamp on light |
| Ice | `#8FE3F5` | Tracks on dark |
| Graphite | `#1E2327` | Dark backgrounds (repo theme colour) |

## Usage rules

- Keep clear space of at least the clamp's diameter on every side.
- Minimum sizes: isotype 20 px; horizontal lockup 24 px tall. Below 32 px, use the arm-less favicon mark.
- Don't recolour individual parts, add shadows or effects, rotate the arm, or re-set the wordmark in a typeface.

## Regenerating

`python3 source/generate.py` (needs `fonttools`, `rsvg-convert`, ImageMagick, and Inter at `FONT_DIR`). It overwrites the kit files in place.
