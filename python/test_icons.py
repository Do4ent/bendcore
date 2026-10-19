from icons import (Icon, IconSet, Theme, check_svg, check_theme_colors, resolve_icon, apply_colors,
                   load_package, DEFAULT_THEME, MONO_THEME)

GOOD = '<svg xmlns="http://www.w3.org/2000/svg"><path fill="{{category}}" stroke="{{outline}}"/></svg>'
COLORS = {"light": {"outline": "#222222", "category": "#3366cc", "accent": "#ff8800", "background": "#ffffff"},
          "dark": {"outline": "#eeeeee", "category": "#88aaff", "accent": "#ffaa33", "background": "#111111"}}


def test_svg_rules():
    assert check_svg(GOOD) == []
    assert check_svg('<svg><script>x</script></svg>')
    assert check_svg('<svg onload="x()"></svg>')
    assert check_svg('<svg><image href="https://evil/x.png"/></svg>')
    assert check_svg('<svg><foreignObject/></svg>')
    assert check_svg('<svg>' + 'x' * 70000 + '</svg>')
    assert check_svg('<div/>')


def test_three_layers_fallback():
    base = IconSet("базовый", {"line": Icon("line", "base-line"), "arc": Icon("arc", "base-arc")})
    theme = Theme("Монохромная", COLORS, IconSet("тема", {"arc": Icon("arc", "theme-arc")}))
    assert resolve_icon("arc", base, theme).svg == "theme-arc"
    assert resolve_icon("line", base, theme).svg == "base-line"
    assert resolve_icon("missing", base, theme) is None
    assert resolve_icon("line", base, None).svg == "base-line"


def test_colors_applied_to_roles():
    out = apply_colors(GOOD, COLORS["dark"])
    assert "#88aaff" in out and "{{" not in out


def test_colors_must_be_hex_for_both_modes():
    assert any("dark" in p for p in check_theme_colors({"light": COLORS["light"]}))
    assert check_theme_colors(COLORS) == []


def test_package_skips_unknown_and_rejects_bad_svg():
    theme, report = load_package("Пакет", {"name": "Пакет", "colors": COLORS},
                                 {"known": GOOD, "unknown": GOOD, "evil": "<svg><script/></svg>"},
                                 known_commands=["known", "evil"])
    assert theme is not None
    assert "known" in theme.icons.icons and "evil" not in theme.icons.icons
    assert any("неизвестной" in r for r in report)
    assert any("отклонён" in r for r in report)


def test_package_without_description_rejected():
    theme, report = load_package("x", {}, {}, [])
    assert theme is None and report


def test_default_names():
    assert DEFAULT_THEME == "Цветная" and MONO_THEME == "Монохромная"
