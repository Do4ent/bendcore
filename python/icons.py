"""Этап 5, темы значков. Спецификация 6.14 («Значки и темы», «Темы значков 18.10.2026»).

Значок — SVG с цветовыми ролями: контур, заливка категории (11 категорий легенды ленты), акцент, фон.
Тема задаёт значения ролей для светлого и тёмного интерфейса.
Три слоя: базовый набор рисунков (на все команды) → набор рисунков темы (заменяет часть значков,
недостающие берутся из базового) → цвета темы.
В версии 1 поставляются «Цветная» (по умолчанию) и «Монохромная». Пакет темы — ZIP с описанием и SVG.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

ROLES = ("outline", "category", "accent", "background")
CATEGORIES = 11
MAX_SVG_BYTES = 64 * 1024          # ограничение размера одного значка

_FORBIDDEN = [
    (re.compile(r"<\s*script", re.I), "скрипт в SVG"),
    (re.compile(r"\son[a-z]+\s*=", re.I), "обработчик событий on*"),
    (re.compile(r"(href|src)\s*=\s*['\"]\s*(https?:|//|javascript:|data:)", re.I), "внешняя ссылка или javascript:"),
    (re.compile(r"<\s*foreignObject", re.I), "foreignObject"),
    (re.compile(r"@import|url\(\s*['\"]?https?:", re.I), "внешний ресурс в стиле"),
]


@dataclass
class Icon:
    command: str
    svg: str


@dataclass
class IconSet:
    name: str
    icons: Dict[str, Icon] = field(default_factory=dict)


@dataclass
class Theme:
    name: str
    colors: Dict[str, Dict[str, str]]          # {"light": {role: hex}, "dark": {...}}
    icons: Optional[IconSet] = None            # набор рисунков темы (может отсутствовать — только цвета)


def check_svg(svg: str) -> List[str]:
    """Причины отказа; пустой список — значок допустим."""
    problems = []
    if len(svg.encode("utf-8")) > MAX_SVG_BYTES:
        problems.append("слишком большой SVG")
    if "<svg" not in svg.lower():
        problems.append("не SVG")
    for rx, why in _FORBIDDEN:
        if rx.search(svg):
            problems.append(why)
    return problems


def check_theme_colors(colors: Dict[str, Dict[str, str]]) -> List[str]:
    problems = []
    for mode in ("light", "dark"):
        roles = colors.get(mode)
        if roles is None:
            problems.append(f"нет цветов для {mode}")
            continue
        for r in ROLES:
            v = roles.get(r)
            if not v or not re.fullmatch(r"#[0-9a-fA-F]{6}", v):
                problems.append(f"{mode}.{r}: нужен цвет #RRGGBB")
    return problems


def resolve_icon(command: str, base: IconSet, theme: Optional[Theme]) -> Optional[Icon]:
    """Слой 2 поверх слоя 1: значок темы, иначе базовый; None — значка нет ни там, ни там."""
    if theme and theme.icons and command in theme.icons.icons:
        return theme.icons.icons[command]
    return base.icons.get(command)


def apply_colors(svg: str, roles: Dict[str, str]) -> str:
    """Подставляет цвета ролей в SVG через плейсхолдеры {{outline}} и т. п."""
    out = svg
    for r in ROLES:
        out = out.replace("{{" + r + "}}", roles[r])
    return out


def load_package(name: str, description: Dict, svgs: Dict[str, str], known_commands: List[str]
                 ) -> Tuple[Optional[Theme], List[str]]:
    """Проверка пакета темы. Неизвестные команды пропускаются с отчётом; ошибки описания прерывают загрузку."""
    report: List[str] = []
    colors = description.get("colors")
    if not description.get("name") or (colors is None and not svgs):
        return None, ["описание пакета неполное (нужны name и colors или SVG)"]
    if colors is not None:
        cp = check_theme_colors(colors)
        if cp:
            return None, cp
    icons = None
    if svgs:
        icons = IconSet(name=name)
        for cmd, svg in svgs.items():
            if cmd not in known_commands:
                report.append(f"пропущен значок неизвестной команды: {cmd}")
                continue
            bad = check_svg(svg)
            if bad:
                report.append(f"значок {cmd} отклонён: " + "; ".join(bad))
                continue
            icons.icons[cmd] = Icon(cmd, svg)
    return Theme(name=name, colors=colors or {}, icons=icons), report


DEFAULT_THEME = "Цветная"
MONO_THEME = "Монохромная"


# ---------- Свой значок кнопки (6.14, «Свой значок кнопки») ----------
# Слои по возрастанию приоритета: базовый набор → набор рисунков темы → значки предприятия «для всех»
# (администратор, 6.10) → личные значки пользователя. «Сбросить» убирает личный значок.
PNG_MIN_PX = 32          # «PNG не меньше 32 и 64 px» — трактовка: сторона ≥ 32 px, для крупных размеров ≥ 64 px


@dataclass
class CustomIcon:
    command: str
    kind: str                       # "svg" | "png"
    data: str                       # SVG-текст или путь к PNG
    width: int = 0                  # для PNG
    height: int = 0


def check_custom(icon: CustomIcon) -> Tuple[List[str], List[str]]:
    """(ошибки, предупреждения) для своего значка."""
    if icon.kind == "svg":
        return check_svg(icon.data), []
    if icon.kind == "png":
        side = min(icon.width, icon.height)
        if side < PNG_MIN_PX:
            return [f"PNG меньше {PNG_MIN_PX} px"], []
        if side < 64:
            return [], ["PNG меньше 64 px: в крупных размерах ленты будет увеличен с потерей чёткости"]
        return [], []
    return [f"неподдерживаемый формат: {icon.kind}"], []


def resolve_layers(command: str, base: IconSet, theme: Optional[Theme],
                   enterprise: Optional[Dict[str, CustomIcon]] = None,
                   personal: Optional[Dict[str, CustomIcon]] = None):
    """Действующий значок: личный → предприятия → темы → базовый."""
    for layer in (personal, enterprise):
        if layer and command in layer:
            return layer[command]
    return resolve_icon(command, base, theme)


def recolors_with_theme(icon) -> bool:
    """SVG с цветовыми ролями перекрашивается вместе с темой, остальные показываются как есть (6.14)."""
    svg = icon.svg if isinstance(icon, Icon) else (icon.data if icon.kind == "svg" else "")
    return any("{{" + r + "}}" in svg for r in ROLES)


def suggest_theme(keyset_name: str, packages: Dict[str, Dict]) -> Optional[str]:
    """Пакет, указавший подходящий набор клавиш, предлагается при выборе этого набора (6.14)."""
    for name, description in packages.items():
        if description.get("keyset") == keyset_name:
            return name
    return None
