# main.py
from __future__ import annotations

import io
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional

import httpx
import numpy as np
from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import Response
from PIL import Image


# ------------------------------
# config
# ------------------------------

_SHEET_W, _SHEET_H = 256, 128
_OUT_SIZE = 96

_ROOT = Path(__file__).resolve().parent
_CACHE_DIR = _ROOT / "skins"
_CACHE_DIR.mkdir(parents=True, exist_ok=True)
_DEFAULT_SHEET = _ROOT / "default.png"

_REMOTE_CANDIDATES: tuple[str, ...] = (
    "https://ddnet.org/skins/skin/community/{name}.png",
    "https://ddnet.org/skins/skin/{name}.png",
)

_TIMEOUT = httpx.Timeout(6.0, connect=3.0)
_LIMITS = httpx.Limits(max_keepalive_connections=10, max_connections=20)

# heuristic: feet region is in the right-most quarter, center half of height
# (matches the classic ddnet sheet layout)
_LEG_Y0_FRAC = 1 / 4
_LEG_Y1_FRAC = 3 / 4
_LEG_X0_FRAC = 6 / 8

_NAME_RX = re.compile(r"^[A-Za-z0-9._-]{1,64}$")


# ------------------------------
# sprite layout
# ------------------------------

@dataclass(frozen=True)
class _Piece:
    # crop rectangle on sheet, in pixels (x, y, w, h)
    crop: tuple[int, int, int, int]
    # paste anchors on output, in pixels
    anchors: tuple[tuple[int, int], ...]
    # scale of the cropped element relative to output size
    scale: tuple[float, float]
    # flip the second (and later) instance horizontally
    flip_after_first: bool = False


_PIECES: tuple[_Piece, ...] = (
    _Piece(crop=(96, 0, 96, 96), anchors=((0, 0),), scale=(1.0, 1.0)),                      # body_outline
    _Piece(crop=(192, 64, 64, 32), anchors=((-10, 45), (10, 45)), scale=(1.0, 0.5)),       # foot_outline (l/r)
    _Piece(crop=(192, 32, 64, 32), anchors=((-10, 45),), scale=(1.0, 0.5)),                # foot (l)
    _Piece(crop=(0, 0, 96, 96), anchors=((0, 0),), scale=(1.0, 1.0)),                       # body
    _Piece(crop=(192, 32, 64, 32), anchors=((10, 45),), scale=(1.0, 0.5)),                  # foot (r)
    _Piece(crop=(64, 96, 32, 32), anchors=((35, 23), (46, 23)), scale=(0.4, 0.4), flip_after_first=True),  # eye (l/r)
)


# ------------------------------
# helpers
# ------------------------------

def _bad_request(msg: str) -> HTTPException:
    return HTTPException(status_code=400, detail=msg)


def _server_error(msg: str) -> HTTPException:
    return HTTPException(status_code=500, detail=msg)


# sanitize inputs
def _skin_stem(raw: str) -> str:
    stem = os.path.splitext(os.path.basename(raw.strip()))[0]
    if not _NAME_RX.fullmatch(stem):
        raise _bad_request("некорректное имя.")
    return stem


# parse tw code
def _parse_twcode(raw: Optional[str]) -> Optional[int]:
    if raw is None or not raw.strip():
        return None

    s = raw.strip()
    try:
        if s.lower().startswith("0x"):
            return int(s, 16) & 0xFFFFFF
        if re.fullmatch(r"[0-9A-Fa-f]{6}", s):
            return int(s, 16) & 0xFFFFFF
        return int(s) & 0xFFFFFF
    except ValueError as e:
        raise _bad_request("неверный twcode.") from e


# tw code -> rgba (hsl-ish; ddnet style)
def _twcode_rgba(code: int) -> tuple[int, int, int, int]:
    h = ((code >> 16) & 255) / 255.0
    s = ((code >> 8) & 255) / 255.0
    l = (((code & 255) / 2) + 128) / 255.0

    if s == 0.0:
        v = int(np.clip(np.floor(l * 255.0 + 0.5), 0, 255))
        return (v, v, v, 255)

    t2 = l * (1 + s) if l < 0.5 else l + s - l * s
    t1 = 2 * l - t2

    def hue_to_rgb(t: float) -> int:
        t %= 1.0
        if t < 1 / 6:
            v = t1 + (t2 - t1) * 6 * t
        elif t < 1 / 2:
            v = t2
        elif t < 2 / 3:
            v = t1 + (t2 - t1) * (2 / 3 - t) * 6
        else:
            v = t1
        return int(np.clip(np.floor(v * 255.0 + 0.5), 0, 255))

    return (hue_to_rgb(h + 1 / 3), hue_to_rgb(h), hue_to_rgb(h - 1 / 3), 255)


# recolor sheet
def _recolor_sheet(sheet: Image.Image, body: Optional[int], foot: Optional[int]) -> Image.Image:
    if body is None and foot is None:
        return sheet

    # default codes: keep old behavior
    body_code = 0 if body is None else body
    foot_code = 0 if foot is None else foot

    br, bg, bb, ba = _twcode_rgba(body_code)
    fr, fg, fb, fa = _twcode_rgba(foot_code)

    rgba = np.array(sheet.convert("RGBA"), dtype=np.uint8)
    h, w = rgba.shape[:2]

    # legs mask
    y0 = int(h * _LEG_Y0_FRAC)
    y1 = int(h * _LEG_Y1_FRAC)
    x0 = int(w * _LEG_X0_FRAC)
    legs = np.zeros((h, w), dtype=bool)
    legs[y0:y1, x0:w] = True

    # grayscale (preserve shading) -> recolor via channel scaling
    gray = rgba[..., :3].astype(np.float32).mean(axis=2)[..., None]
    body_rgb = gray * (np.array([br, bg, bb], dtype=np.float32) / 255.0)
    foot_rgb = gray * (np.array([fr, fg, fb], dtype=np.float32) / 255.0)

    body_a = rgba[..., 3].astype(np.float32) * (ba / 255.0)
    foot_a = rgba[..., 3].astype(np.float32) * (fa / 255.0)

    out_rgb = np.where(legs[..., None], foot_rgb, body_rgb)
    out_a = np.where(legs, foot_a, body_a)

    rgba[..., :3] = np.clip(np.floor(out_rgb + 0.5), 0, 255).astype(np.uint8)
    rgba[..., 3] = np.clip(np.floor(out_a + 0.5), 0, 255).astype(np.uint8)

    return Image.fromarray(rgba, mode="RGBA")


# crop utility
def _crop_rgba(sheet: Image.Image, crop: tuple[int, int, int, int]) -> Image.Image:
    x, y, w, h = crop
    return sheet.crop((x, y, x + w, y + h)).convert("RGBA")


# assemble final tee
def _compose(sheet: Image.Image) -> Image.Image:
    try:
        resample = Image.Resampling.BILINEAR  # pillow >= 9
    except Exception:
        resample = Image.BILINEAR

    out = Image.new("RGBA", (_OUT_SIZE, _OUT_SIZE), (0, 0, 0, 0))

    for piece in _PIECES:
        part = _crop_rgba(sheet, piece.crop)
        target = (int(_OUT_SIZE * piece.scale[0]), int(_OUT_SIZE * piece.scale[1]))
        part = part.resize(target, resample)

        for idx, pos in enumerate(piece.anchors):
            drawn = part
            if piece.flip_after_first and idx > 0:
                drawn = part.transpose(Image.FLIP_LEFT_RIGHT)
            out.paste(drawn, pos, drawn)

    return out


# ------------------------------
# i/o and http
# ------------------------------

def _load_default_sheet() -> Image.Image:
    try:
        img = Image.open(_DEFAULT_SHEET).convert("RGBA")
    except Exception as e:
        raise _server_error("default.png отсутствует или повреждён.") from e

    if img.size != (_SHEET_W, _SHEET_H):
        raise _server_error(f"default.png должен быть {_SHEET_W}x{_SHEET_H}.")
    return img


def _open_cached_sheet(path: Path) -> Optional[Image.Image]:
    try:
        img = Image.open(path).convert("RGBA")
        if img.size == (_SHEET_W, _SHEET_H):
            return img
    except Exception:
        pass

    # drop broken cache entry
    try:
        path.unlink(missing_ok=True)
    except Exception:
        pass
    return None


async def _download_sheet(client: httpx.AsyncClient, stem: str) -> Optional[bytes]:
    for tpl in _REMOTE_CANDIDATES:
        url = tpl.format(name=stem)

        # small retry
        for _ in range(2):
            try:
                r = await client.get(url)
            except httpx.HTTPError:
                continue

            if r.status_code == 404:
                break
            if r.status_code != 200:
                continue

            data = r.content
            try:
                img = Image.open(io.BytesIO(data))
                if img.size == (_SHEET_W, _SHEET_H):
                    return data
            except Exception:
                # remote returned junk; try next candidate
                break

    return None


async def _get_sheet(client: httpx.AsyncClient, stem: str) -> Image.Image:
    cache_path = _CACHE_DIR / f"{stem}.png"

    # try cache first
    cached = _open_cached_sheet(cache_path)
    if cached is not None:
        return cached

    # then remote
    data = await _download_sheet(client, stem)
    if data:
        try:
            cache_path.write_bytes(data)
            img = Image.open(io.BytesIO(data)).convert("RGBA")
            if img.size == (_SHEET_W, _SHEET_H):
                return img
        except Exception:
            # ignore disk/image errors and fallback
            pass

    return _load_default_sheet()


# ------------------------------
# app
# ------------------------------

from contextlib import asynccontextmanager


@asynccontextmanager
async def _lifespan(app: FastAPI):
    # shared http client
    app.state.http = httpx.AsyncClient(
        timeout=_TIMEOUT,
        limits=_LIMITS,
        follow_redirects=True,
        headers={"User-Agent": "skins-api/1.0"},
    )
    try:
        yield
    finally:
        await app.state.http.aclose()


app = FastAPI(lifespan=_lifespan)


@app.get("/render")
async def render(
    name: str = Query(..., description="skin name, without extension"),
    body: Optional[str] = Query(None, description="body twcode (dec / 0xHEX / RRGGBB)"),
    foot: Optional[str] = Query(None, description="foot twcode (dec / 0xHEX / RRGGBB)"),
) -> Response:
    # parse and validate
    stem = _skin_stem(name)
    body_code = _parse_twcode(body)
    foot_code = _parse_twcode(foot)

    # load sheet
    client: httpx.AsyncClient = app.state.http
    sheet = await _get_sheet(client, stem)

    # build output
    colored = _recolor_sheet(sheet, body_code, foot_code)
    image = _compose(colored)

    buf = io.BytesIO()
    image.save(buf, "PNG")
    return Response(
        content=buf.getvalue(),
        media_type="image/png",
        headers={"Cache-Control": "public, max-age=3600"},
    )
