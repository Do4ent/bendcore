// Сохранение и загрузка пользовательских настроек импорта DWFx. Раздел «Импорт → DWFx» в настройках
// программы: значения хранятся по тем же идентификаторам, что и описание панели свойств
// (import_properties_json). Отсутствующие ключи получают значения по умолчанию из заметок; неизвестные
// идентификаторы и значения вне диапазона отвергаются.
#pragma once

#include "bendcore/dwfx/import_settings.hpp"

#include <string>

namespace bendcore::dwfx {

// Текст JSON: {"import_kind":"dwfx","version":1,"values":{"<id>":"<значение>", ...}}.
// Все значения пишутся текстом в простом виде (как get_import_property).
std::string import_settings_to_json(const ImportSettings& settings);

// Разбор JSON, сделанного import_settings_to_json. Исключение std::invalid_argument при синтаксисе,
// неизвестном виде импорта или версии, неизвестном идентификаторе, недопустимом значении или
// нарушении согласованности (validate_import_settings).
ImportSettings import_settings_from_json(const std::string& json);

// Чтение из файла. Если файла нет — значения по умолчанию (пользователь ещё ничего не менял).
// Исключение std::invalid_argument, если файл повреждён.
ImportSettings load_import_settings(const std::string& path);

// Запись в файл через временный файл и переименование. Исключение std::runtime_error при ошибке записи.
void save_import_settings(const std::string& path, const ImportSettings& settings);

}  // namespace bendcore::dwfx
