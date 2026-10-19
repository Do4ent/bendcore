"""Личные настройки и профиль предприятия. Спецификация 6.10, 6.13, 6.14 («Профили и сохранение»).

Уровни: стандартные значения (6.13) → профиль предприятия (администратор задаёт значения и может закрепить
любую личную настройку) → профиль пользователя. Закреплённая настройка показывается с замком и подсказкой
«Задано предприятием»; пользователь её не меняет. Сброс раздела или всего — к профилю предприятия,
при его отсутствии — к стандартному. Экспорт и импорт — всех или выбранных разделов.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, Iterable, Optional, Set, Tuple

LOCKED_HINT = "Задано предприятием"

# Стандартные значения интерфейса (6.13). Ключ «раздел.имя».
STANDARD: Dict[str, Any] = {
    "keyboard.keyset": "SolidWorks",
    "keyboard.snaps_on": True,                    # «Привязки: включены (F3)»
    "navigation.style": None,                     # 6.13 не задаёт; по умолчанию — как у набора клавиш (README)
    "icons.theme": "Цветная",
    "tree.labels": "name+sortament",              # «имя + сортамент»
    "tree.tube_order": "by_systems",
    "tree.collapse_identical": True,
    "view.camera_history": 20,
    "files.stale_lock_minutes": 10,
}


@dataclass
class Settings:
    enterprise: Dict[str, Any] = field(default_factory=dict)
    locked: Set[str] = field(default_factory=set)
    user: Dict[str, Any] = field(default_factory=dict)

    def get(self, key: str) -> Tuple[Any, str, bool]:
        """(значение, источник, закреплено)."""
        if key in self.locked:
            return self.enterprise.get(key, STANDARD.get(key)), "enterprise", True
        if key in self.user:
            return self.user[key], "user", False
        if key in self.enterprise:
            return self.enterprise[key], "enterprise", False
        return STANDARD.get(key), "standard", False

    def value(self, key: str) -> Any:
        return self.get(key)[0]

    def set_user(self, key: str, value: Any) -> None:
        if key in self.locked:
            raise PermissionError(LOCKED_HINT)
        self.user[key] = value

    def lock(self, key: str, value: Any) -> None:
        """Администратор закрепляет настройку в профиле предприятия."""
        self.enterprise[key] = value
        self.locked.add(key)

    def reset(self, section: Optional[str] = None) -> None:
        """Сброс раздела или всего интерфейса к профилю предприятия (или к стандартному)."""
        for k in list(self.user):
            if section is None or k.startswith(section + "."):
                del self.user[k]

    def export(self, sections: Optional[Iterable[str]] = None) -> Dict[str, Any]:
        secs = set(sections) if sections else None
        return {k: v for k, v in self.user.items() if secs is None or k.split(".")[0] in secs}

    def import_(self, data: Dict[str, Any]) -> Dict[str, str]:
        """Импорт личных настроек; закреплённые предприятием пропускаются с причиной."""
        skipped = {}
        for k, v in data.items():
            if k in self.locked:
                skipped[k] = LOCKED_HINT
            else:
                self.user[k] = v
        return skipped
