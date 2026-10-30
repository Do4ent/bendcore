// Геометрический бэкенд GeometryAnalyzer на Open CASCADE (собирается в bendcore_occt).
#pragma once

#include "bendcore/geometry_analyzer.hpp"

#include <string>

namespace bendcore::occt {

// Признаки трубы из B-rep (текст BRepTools): цилиндрические грани — прямые участки, тороидальные — гибы.
// Наружный радиус — наибольший радиус этих граней, внутренний — меньший (если есть). Длины и углы берутся
// по параметрам поверхностей с учётом того, что грань может быть разрезана швом на части.
// Порядок участков и гибов в результате не обязан совпадать с порядком по пути.
// Неподдерживаемые поверхности (например, спиральные участки) — ok = false с причиной.
geometry::BRepTubeFeatures analyze_brep(const std::string& brep_text);

// Подключить analyze_brep как бэкенд для всех GeometryAnalyzer.
void install_geometry_backend();

}  // namespace bendcore::occt
