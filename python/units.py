"""Единицы измерения. Спецификация 6.15.

Хранение — всегда миллиметры и градусы. Единицы документа (мм или дюймы) — только показ и ввод.
Ввод с суффиксом работает в любом документе: «1in», «25,4mm», «3/8in», «1 1/2in», «90°30'»;
оба разделителя дробной части. Без суффикса число понимается в единицах документа.
"""
from __future__ import annotations

import math
import re
from fractions import Fraction

MM_PER_IN = 25.4
_LEN_UNITS = {"mm": 1.0, "мм": 1.0, "cm": 10.0, "см": 10.0, "m": 1000.0, "м": 1000.0,
              "in": MM_PER_IN, "\"": MM_PER_IN, "дюйм": MM_PER_IN}
_NUM = r"\d+(?:[.,]\d+)?"


def _number(s: str) -> float:
    """«1 1/2» → 1.5, «3/8» → 0.375, «25,4» → 25.4."""
    s = s.strip().replace(",", ".")
    m = re.fullmatch(r"(?:(\d+)\s+)?(\d+)/(\d+)", s)
    if m:
        whole = int(m.group(1)) if m.group(1) else 0
        den = int(m.group(3))
        if den == 0:
            raise ValueError("деление на ноль")
        return whole + int(m.group(2)) / den
    if re.fullmatch(_NUM, s):
        return float(s)
    raise ValueError(f"не число: {s!r}")


def parse_length(text: str, doc_unit: str = "mm") -> float:
    """Длина в мм. «1in» → 25.4; «3/8in» → 9.525; «1 1/2in» → 38.1; «25,4mm» → 25.4;
    «12» в дюймовом документе → 304.8."""
    s = text.strip().lower()
    neg = s.startswith("-")
    if neg:
        s = s[1:].strip()
    m = re.fullmatch(r"(.+?)\s*(mm|мм|cm|см|m|м|in|\"|дюйм)?", s)
    if not m:
        raise ValueError(f"не длина: {text!r}")
    unit = m.group(2) or doc_unit
    if unit not in _LEN_UNITS:
        raise ValueError(f"неизвестная единица: {unit!r}")
    v = _number(m.group(1)) * _LEN_UNITS[unit]
    return -v if neg else v


def parse_angle(text: str) -> float:
    """Угол в градусах: «90», «90,5», «90.5°», «90°30'», «90°30′»."""
    s = text.strip().replace(",", ".").replace("′", "'").replace("’", "'")
    neg = s.startswith("-")
    if neg:
        s = s[1:].strip()
    m = re.fullmatch(r"(\d+(?:\.\d+)?)\s*°?\s*(?:(\d+(?:\.\d+)?)\s*')?", s)
    if not m:
        raise ValueError(f"не угол: {text!r}")
    v = float(m.group(1)) + (float(m.group(2)) / 60 if m.group(2) else 0.0)
    return -v if neg else v


def format_length(mm: float, doc_unit: str = "mm", decimals: int = 1, inch_fraction: int = 0,
                  sep: str = ",") -> str:
    """Показ длины: мм с числом знаков стандарта (по умолчанию 0,1 мм); дюймы десятичные или дроби
    1/16, 1/32, 1/64 («12 3/8\"»). Разделитель дробной части — из стандарта."""
    if doc_unit == "mm":
        return f"{mm:.{decimals}f}".replace(".", sep)
    inches = mm / MM_PER_IN
    if inch_fraction:
        f = Fraction(round(inches * inch_fraction), inch_fraction)
        whole, rest = divmod(f.numerator, f.denominator)
        if rest == 0:
            return f"{whole}\""
        frac = Fraction(rest, f.denominator)
        return (f"{whole} " if whole else "") + f"{frac.numerator}/{frac.denominator}\""
    return f"{inches:.{decimals}f}\"".replace(".", sep)


def format_angle(deg: float, mode: str = "decimal", decimals: int = 1, sep: str = ",") -> str:
    """Углы — десятичные градусы (по умолчанию 0,1°) или градусы и минуты (90°30′)."""
    if mode == "dms":
        total = round(abs(deg) * 60)
        d, m = divmod(total, 60)
        return ("-" if deg < 0 else "") + f"{d}°{m:02d}′"
    return f"{deg:.{decimals}f}°".replace(".", sep)


def format_tube_size(od_mm: float, wall_mm: float, inch_label: str = "", mode: str = "in_mm",
                     show_wall: bool = True, sep: str = ",") -> str:
    """Формат размера трубы (6.15). Режимы для дюймовых труб: «in_mm» — 3/8\" (9,52); «mm_in» — 9,52 (3/8\");
    «in» — 3/8\"; «mm» — 9,52. Метрические трубы (без inch_label) — всегда в мм.
    Стенка — один раз в конце, в мм (в режимах in_mm и in — в единицах стандарта трубы; здесь мм)."""
    mm = f"{od_mm:.2f}".replace(".", sep)
    if not inch_label:
        body = mm
    else:
        body = {"in_mm": f"{inch_label} ({mm})", "mm_in": f"{mm} ({inch_label})",
                "in": inch_label, "mm": mm}[mode]
    if show_wall:
        body += " × " + f"{wall_mm:.2f}".replace(".", sep)
    return body
