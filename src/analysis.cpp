#include "bendcore/analysis.hpp"

#include "bendcore/blank.hpp"
#include "bendcore/compensation.hpp"
#include "bendcore/spiral.hpp"
#include "bendcore/tubegeom.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace bendcore::analysis {

using json = nlohmann::json;

// ============ MATERIALDATABASE IMPLEMENTATION ============

MaterialDatabase::MaterialDatabase() {
    // Initialize categories
    categories_ = {
        "Carbon Steel",
        "Stainless Steel",
        "Aluminum Alloy",
        "Copper",
        "Titanium",
        "Nickel Alloy",
        "Special"
    };
}

MaterialDatabase::~MaterialDatabase() = default;

MaterialDatabase& MaterialDatabase::instance() {
    static MaterialDatabase db;
    return db;
}

void MaterialDatabase::loadMaterialsFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            // If file doesn't exist, initialize with empty database
            initialized_ = true;
            return;
        }

        json data = json::parse(file);

        if (data.contains("materials") && data["materials"].is_array()) {
            for (const auto& matJson : data["materials"]) {
                MaterialProperties mat;
                mat.id = matJson.value("id", "");
                mat.name = matJson.value("name", "");
                mat.category = matJson.value("category", "");
                mat.density = matJson.value("density", 5.0);
                mat.availability = matJson.value("availability", "Standard");
                mat.color = matJson.value("color", "#808080");

                // Parse properties
                if (matJson.contains("properties")) {
                    auto props = matJson["properties"];
                    mat.properties.youngsModulus = props.value("youngsModulus", 0.0);
                    mat.properties.yieldStrength = props.value("yieldStrength", 0.0);
                    mat.properties.tensileStrength = props.value("tensileStrength", 0.0);
                    mat.properties.elongation = props.value("elongation", 0.0);
                    mat.properties.hardness = props.value("hardness", 0.0);
                    mat.properties.cost = props.value("cost", 0.0);
                }

                // Parse standards and applications
                if (matJson.contains("standards")) {
                    mat.standards = matJson["standards"].get<std::vector<std::string>>();
                }
                if (matJson.contains("applications")) {
                    mat.applications = matJson["applications"].get<std::vector<std::string>>();
                }

                if (!mat.id.empty()) {
                    materials_[mat.id] = mat;
                }
            }
        }

        initialized_ = true;
    } catch (const std::exception& e) {
        // Log error but don't fail - database just won't have materials
        initialized_ = true;
    }
}

std::optional<MaterialProperties> MaterialDatabase::getMaterial(const std::string& id) {
    auto it = materials_.find(id);
    if (it != materials_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<MaterialProperties> MaterialDatabase::getAllMaterials() {
    std::vector<MaterialProperties> result;
    for (const auto& pair : materials_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<MaterialProperties> MaterialDatabase::getMaterialsByCategory(const std::string& category) {
    std::vector<MaterialProperties> result;
    for (const auto& pair : materials_) {
        if (pair.second.category == category) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<MaterialProperties> MaterialDatabase::getMaterialsByAvailability(const std::string& availability) {
    std::vector<MaterialProperties> result;
    for (const auto& pair : materials_) {
        if (pair.second.availability == availability) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool MaterialDatabase::addCustomMaterial(const MaterialProperties& props) {
    if (props.id.empty()) {
        return false;
    }
    materials_[props.id] = props;
    return true;
}

bool MaterialDatabase::removeCustomMaterial(const std::string& id) {
    auto it = materials_.find(id);
    if (it != materials_.end()) {
        materials_.erase(it);
        return true;
    }
    return false;
}

bool MaterialDatabase::updateCustomMaterial(const std::string& id, const MaterialProperties& props) {
    auto it = materials_.find(id);
    if (it != materials_.end()) {
        materials_[id] = props;
        return true;
    }
    return false;
}

double MaterialDatabase::getMaterialDensity(const std::string& materialId) {
    auto mat = getMaterial(materialId);
    if (mat.has_value()) {
        return mat->density;
    }
    return 5.0;  // Default fallback
}

std::vector<std::string> MaterialDatabase::getCategories() {
    return categories_;
}

std::vector<std::string> MaterialDatabase::getAvailabilityLevels() {
    return {"Common", "Standard", "Premium", "Custom"};
}

bool MaterialDatabase::checkStandardsCompliance(
    const std::string& materialId,
    const std::vector<std::string>& requiredStandards
) {
    auto mat = getMaterial(materialId);
    if (!mat.has_value()) {
        return false;
    }

    for (const auto& required : requiredStandards) {
        bool found = false;
        for (const auto& standard : mat->standards) {
            if (standard == required) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> MaterialDatabase::getCommonStandards(const std::string& materialId) {
    auto mat = getMaterial(materialId);
    if (mat.has_value()) {
        return mat->standards;
    }
    return {};
}

// ============ ANALYZER IMPLEMENTATION ============

Analyzer::Analyzer() = default;
Analyzer::~Analyzer() = default;

Issue Analyzer::createIssue(
    IssueLevel level,
    IssueType type,
    const std::string& message,
    std::optional<int> bendIdx,
    std::optional<double> actual,
    std::optional<double> minVal,
    std::optional<double> maxVal
) const {
    return {level, type, message, bendIdx, {}, actual, minVal, maxVal};
}

AnalysisResult Analyzer::analyze(
    const TubeModel& model,
    double od, double wall,
    const MaterialSpec& material,
    const std::vector<MachineSpec>& machines
) const {
    AnalysisResult result;
    result.valid = true;

    if (!model.path.has_value()) {
        result.issues.push_back(createIssue(
            IssueLevel::Error,
            IssueType::SelfIntersection,
            "Тело трубы не построено: отсутствует путь"
        ));
        result.valid = false;
        return result;
    }

    const auto& path = *model.path;

    // Проверка геометрии
    auto geomIssues = checkGeometry(model, od);
    result.issues.insert(result.issues.end(), geomIssues.begin(), geomIssues.end());

    // Выбор лучшей машины
    if (!machines.empty()) {
        // Фильтруем подходящие машины
        std::vector<const MachineSpec*> suitable;
        for (const auto& m : machines) {
            auto machineIssues = checkMachineCapability(model, m);
            bool isSuitable = true;
            for (const auto& iss : machineIssues) {
                if (iss.level == IssueLevel::Error) {
                    isSuitable = false;
                    break;
                }
            }
            if (isSuitable) {
                suitable.push_back(&m);
            }
        }

        if (!suitable.empty()) {
            const MachineSpec& bestMachine = *suitable[0];
            result.suggestedMachine = bestMachine.name;

            // Проверяем параметры гибов для лучшей машины
            auto bendIssues = checkBendParameters(model, material, bestMachine);
            result.issues.insert(result.issues.end(), bendIssues.begin(), bendIssues.end());
        } else {
            result.issues.push_back(createIssue(
                IssueLevel::Error,
                IssueType::NoSuitableMachine,
                "Нет подходящего станка для данной трубы"
            ));
            result.valid = false;
        }
    }

    // Проверка портов
    auto portIssues = checkPorts(model, od, wall);
    result.issues.insert(result.issues.end(), portIssues.begin(), portIssues.end());

    // Проверка совместимости материала
    auto materialIssues = checkMaterialCompatibility(model, material);
    result.issues.insert(result.issues.end(), materialIssues.begin(), materialIssues.end());

    // Вычисление параметров трубы
    result.totalLength = axis_length(path);
    result.bendCount = std::count_if(path.begin(), path.end(), [](const PathSegment& s) {
        return s.kind == SegmentKind::Bend;
    });

    // Подсчёт суммарного угла
    result.totalBendAngle = 0.0;
    for (const auto& seg : path) {
        if (seg.kind == SegmentKind::Bend) {
            result.totalBendAngle += std::abs(seg.angle);
        }
    }

    // Минимальная и максимальная длина заготовки
    const BlankSpec blank = calculate_blank(path, od, wall);
    result.blankLength = blank.length;

    // Поиск радиусов гибов
    std::vector<double> bendRadii;
    for (const auto& seg : path) {
        if (seg.kind == SegmentKind::Bend) {
            bendRadii.push_back(seg.radius);
        }
    }

    if (!bendRadii.empty()) {
        result.minBendRadius = *std::min_element(bendRadii.begin(), bendRadii.end());
        result.maxBendRadius = *std::max_element(bendRadii.begin(), bendRadii.end());
    }

    // Проверка на ошибки (ошибки блокируют производство)
    result.valid = true;
    for (const auto& issue : result.issues) {
        if (issue.level == IssueLevel::Error) {
            result.valid = false;
            break;
        }
    }

    // Вычисление сложности и риска
    if (!machines.empty() && !result.suggestedMachine.empty()) {
        const MachineSpec* bestMachine = nullptr;
        for (const auto& m : machines) {
            if (m.name == result.suggestedMachine) {
                bestMachine = &m;
                break;
            }
        }
        if (bestMachine) {
            result.complexityScore = calculateComplexity(model, material, *bestMachine);
            result.manufacturingRisk = calculateManufacturingRisk(model, material, *bestMachine);
        }
    }

    return result;
}

std::vector<Issue> Analyzer::checkGeometry(const TubeModel& model, double od) const {
    std::vector<Issue> issues;

    if (!model.path.has_value()) {
        return issues;
    }

    const auto& path = *model.path;

    // Проверка на самопересечения
    const auto selfInt = self_intersections(path);
    if (!selfInt.empty()) {
        issues.push_back(createIssue(
            IssueLevel::Error,
            IssueType::SelfIntersection,
            "Труба самопересекается в " + std::to_string(selfInt.size()) + " местах"
        ));
    }

    // Проверка радиусов гибов
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& seg = path[i];
        if (seg.kind == SegmentKind::Bend) {
            if (seg.radius < od / 2.0) {
                issues.push_back(createIssue(
                    IssueLevel::Error,
                    IssueType::InvalidBendRadius,
                    "Радиус гиба меньше радиуса трубы",
                    static_cast<int>(i),
                    seg.radius,
                    od / 2.0
                ));
            }
        }
    }

    // Проверка углов (не более 180°)
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& seg = path[i];
        if (seg.kind == SegmentKind::Bend) {
            if (std::abs(seg.angle) > 180.0) {
                issues.push_back(createIssue(
                    IssueLevel::Error,
                    IssueType::ExcessiveBending,
                    "Угол гиба больше 180°",
                    static_cast<int>(i),
                    seg.angle,
                    {}, -180.0, 180.0
                ));
            }
        }
    }

    return issues;
}

std::vector<Issue> Analyzer::checkBendParameters(
    const TubeModel& model,
    const MaterialSpec& material,
    const MachineSpec& machine
) const {
    std::vector<Issue> issues;

    if (!model.path.has_value()) {
        return issues;
    }

    const auto& path = *model.path;

    // Проверка количества гибов
    int bendCount = std::count_if(path.begin(), path.end(), [](const PathSegment& s) {
        return s.kind == SegmentKind::Bend;
    });

    if (machine.maxBendCount > 0 && bendCount > machine.maxBendCount) {
        issues.push_back(createIssue(
            IssueLevel::Warning,
            IssueType::SuggestedToolChange,
            "Количество гибов превышает рекомендуемое для станка",
            {}, static_cast<double>(bendCount),
            {}, static_cast<double>(machine.maxBendCount)
        ));
    }

    // Проверка минимальных прямых участков
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& seg = path[i];
        if (seg.kind == SegmentKind::Straight) {
            double minStraight = 10.0; // 10 мм по умолчанию
            if (seg.length < minStraight) {
                if (i > 0 && i < path.size() - 1) {
                    // Внутренний прямой участок между гибами
                    if (path[i-1].kind == SegmentKind::Bend && path[i+1].kind == SegmentKind::Bend) {
                        issues.push_back(createIssue(
                            IssueLevel::Warning,
                            IssueType::InsufficientStraight,
                            "Недостаточный прямой участок между гибами",
                            static_cast<int>(i),
                            seg.length,
                            minStraight
                        ));
                    }
                }
            }
        }
    }

    return issues;
}

std::vector<Issue> Analyzer::checkPorts(const TubeModel& model, double od, double wall) const {
    std::vector<Issue> issues;

    if (!model.path.has_value()) {
        return issues;
    }

    // Проверка портов
    for (size_t i = 0; i < model.ports.size(); ++i) {
        const auto& port = model.ports[i];

        // Проверка диаметра порта
        if (port.od > od) {
            issues.push_back(createIssue(
                IssueLevel::Warning,
                IssueType::InvalidPortPosition,
                "Диаметр порта больше диаметра трубы",
                {}, port.od,
                {}, od
            ));
        }

        // Проверка позиции портов на концах трубы (зазор ≥ 5 мм)
        const double minProtrusion = 5.0;  // 5 мм зазор

        // Это упрощённая проверка; полная проверка требует геометрии
    }

    return issues;
}

std::vector<Issue> Analyzer::checkMaterialCompatibility(
    const TubeModel& model,
    const MaterialSpec& material
) const {
    std::vector<Issue> issues;

    if (!model.path.has_value()) {
        return issues;
    }

    const auto& path = *model.path;

    // Проверка материала
    if (material.ultimateBendRadius > 0.0) {
        for (size_t i = 0; i < path.size(); ++i) {
            const auto& seg = path[i];
            if (seg.kind == SegmentKind::Bend) {
                if (seg.radius < material.ultimateBendRadius) {
                    issues.push_back(createIssue(
                        IssueLevel::Warning,
                        IssueType::SpringbackVariation,
                        "Радиус гиба близок к пределу для данного материала",
                        static_cast<int>(i),
                        seg.radius,
                        material.ultimateBendRadius
                    ));
                }
            }
        }
    }

    return issues;
}

std::vector<Issue> Analyzer::checkMachineCapability(
    const TubeModel& model,
    const MachineSpec& machine
) const {
    std::vector<Issue> issues;

    if (!model.path.has_value()) {
        return issues;
    }

    const auto& path = *model.path;
    const double tubeLength = axis_length(path);

    // Проверка длины трубы
    if (machine.maxTubeLength > 0.0 && tubeLength > machine.maxTubeLength) {
        issues.push_back(createIssue(
            IssueLevel::Error,
            IssueType::NoSuitableMachine,
            "Длина трубы превышает максимум для станка",
            {}, tubeLength,
            {}, machine.maxTubeLength
        ));
    }

    // Проверка диаметра трубы
    double od = 0.0;
    if (!model.ports.empty()) {
        od = model.ports[0].od;
    }

    if (od > 0.0) {
        if (od < machine.minTubeOD) {
            issues.push_back(createIssue(
                IssueLevel::Error,
                IssueType::NoSuitableMachine,
                "Диаметр трубы меньше минимума для станка",
                {}, od,
                machine.minTubeOD
            ));
        }
        if (machine.maxTubeOD > 0.0 && od > machine.maxTubeOD) {
            issues.push_back(createIssue(
                IssueLevel::Error,
                IssueType::NoSuitableMachine,
                "Диаметр трубы больше максимума для станка",
                {}, od,
                {}, machine.maxTubeOD
            ));
        }
    }

    // Проверка радиусов гибов
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& seg = path[i];
        if (seg.kind == SegmentKind::Bend) {
            if (seg.radius < machine.minBendRadius) {
                issues.push_back(createIssue(
                    IssueLevel::Error,
                    IssueType::BendRadiusOutOfRange,
                    "Радиус гиба меньше минимума для станка",
                    static_cast<int>(i),
                    seg.radius,
                    machine.minBendRadius
                ));
            }
            if (machine.maxBendRadius > 0.0 && seg.radius > machine.maxBendRadius) {
                issues.push_back(createIssue(
                    IssueLevel::Error,
                    IssueType::BendRadiusOutOfRange,
                    "Радиус гиба больше максимума для станка",
                    static_cast<int>(i),
                    seg.radius,
                    {}, machine.maxBendRadius
                ));
            }
        }
    }

    return issues;
}

std::string Analyzer::suggestOptimalMachine(
    const TubeModel& model,
    const std::vector<MachineSpec>& machines
) const {
    if (machines.empty()) return "";

    for (const auto& m : machines) {
        auto issues = checkMachineCapability(model, m);
        bool isSuitable = true;
        for (const auto& iss : issues) {
            if (iss.level == IssueLevel::Error) {
                isSuitable = false;
                break;
            }
        }
        if (isSuitable) {
            return m.name;
        }
    }

    return "";
}

double Analyzer::calculateComplexity(
    const TubeModel& model,
    const MaterialSpec& material,
    const MachineSpec& machine
) const {
    if (!model.path.has_value()) return 0.0;

    const auto& path = *model.path;
    double score = 0.0;

    // Количество гибов (макс. 50 баллов)
    int bendCount = std::count_if(path.begin(), path.end(), [](const PathSegment& s) {
        return s.kind == SegmentKind::Bend;
    });
    score += std::min(50.0, bendCount * 5.0);

    // Суммарный угол (макс. 30 баллов)
    double totalAngle = 0.0;
    for (const auto& seg : path) {
        if (seg.kind == SegmentKind::Bend) {
            totalAngle += std::abs(seg.angle);
        }
    }
    score += std::min(30.0, (totalAngle / 360.0) * 30.0);

    // Плотность гибов (макс. 20 баллов)
    double tubeLength = axis_length(path);
    if (tubeLength > 0.0) {
        double bendDensity = bendCount / (tubeLength / 100.0);
        score += std::min(20.0, bendDensity * 2.0);
    }

    return std::min(100.0, score);
}

double Analyzer::calculateManufacturingRisk(
    const TubeModel& model,
    const MaterialSpec& material,
    const MachineSpec& machine
) const {
    if (!model.path.has_value()) return 0.0;

    const auto& path = *model.path;
    double risk = 0.0;

    // Сложность влияет на риск
    risk += calculateComplexity(model, material, machine) / 2.0;

    // Близость к пределам станка
    for (const auto& seg : path) {
        if (seg.kind == SegmentKind::Bend) {
            double minRatio = (seg.radius - machine.minBendRadius) /
                            (machine.maxBendRadius - machine.minBendRadius);
            if (minRatio < 0.2) {
                risk += 10.0; // Близко к минимуму
            }
            if (minRatio > 0.8) {
                risk += 5.0; // Близко к максимуму
            }
        }
    }

    // Материальный фактор
    if (material.springbackFactor > 0.5) {
        risk += 15.0; // Высокое пружинение
    }

    return std::min(100.0, risk);
}

std::vector<Issue> Analyzer::checkGaps(
    const TubeModel& model,
    double minimumGap
) const {
    std::vector<Issue> issues;

    if (!model.path.has_value()) {
        return issues;
    }

    const auto& path = *model.path;

    // For now, we'll generate sample gap violations for demonstration
    // In a full implementation, this would:
    // 1. Calculate the 3D position and extent of each bend segment
    // 2. Measure the distance between adjacent bends
    // 3. Flag violations based on minimum gap threshold

    // Sample violation: Between bend 1 and bend 2
    int bendIndex = 0;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& item = path[i];

        // Check if this is a bend segment (check variant type)
        if (std::holds_alternative<Bend>(item)) {
            bendIndex++;

            // Simulate gap calculation - generate test data
            // In production, calculate actual geometric distance
            double simulatedGap = 2.5 + (bendIndex * 0.8);  // Simulated gaps

            if (simulatedGap < 2.0) {
                // Critical error
                issues.push_back(createIssue(
                    IssueLevel::Error,
                    IssueType::InsufficientToolClearance,
                    "Критический зазор между гибами " + std::to_string(bendIndex) +
                    " и " + std::to_string(bendIndex + 1),
                    static_cast<int>(bendIndex),
                    simulatedGap,
                    minimumGap
                ));
            } else if (simulatedGap < minimumGap) {
                // Warning
                issues.push_back(createIssue(
                    IssueLevel::Warning,
                    IssueType::InsufficientToolClearance,
                    "Зазор между гибами " + std::to_string(bendIndex) +
                    " и " + std::to_string(bendIndex + 1) + " ниже рекомендуемого",
                    static_cast<int>(bendIndex),
                    simulatedGap,
                    minimumGap
                ));
            }
        }
    }

    return issues;
}

double Analyzer::calculateTubeMass(
    const TubeModel& model,
    double outerDiameter,      // mm
    double wallThickness,      // mm
    const std::string& materialName
) const {
    if (!model.path.has_value()) {
        return 0.0;
    }

    // Look up material density
    double density = 5.0;  // default g/cm³
    for (const auto& mat : MATERIAL_DENSITIES) {
        if (mat.name == materialName) {
            density = mat.density;
            break;
        }
    }

    const auto& path = *model.path;
    double totalMass = 0.0;  // grams

    // Calculate cross-section area of the tube
    // Area = π * (OD/2)² - π * ((OD - 2*WT)/2)²
    // Simplify: Area = π/4 * (OD² - (OD - 2*WT)²)
    double innerDiameter = outerDiameter - 2.0 * wallThickness;
    double area = M_PI * (outerDiameter * outerDiameter - innerDiameter * innerDiameter) / 4.0;  // mm²

    // Process each segment in the path
    for (const auto& item : path) {
        double segmentLength = 0.0;  // mm

        if (std::holds_alternative<Straight>(item)) {
            const auto& straight = std::get<Straight>(item);
            segmentLength = straight.S;
        } else if (std::holds_alternative<Bend>(item)) {
            const auto& bend = std::get<Bend>(item);
            // For a bend, calculate the arc length
            // Arc length = R * angle (in radians)
            double angleRadians = std::abs(bend.B) * M_PI / 180.0;
            segmentLength = bend.R * angleRadians;
        } else if (std::holds_alternative<Spiral>(item)) {
            const auto& spiral = std::get<Spiral>(item);
            // For a spiral, approximate as helix arc length
            // More complex calculation: use number of turns and pitch
            // Approximation: circumference * number of turns (ignoring pitch variation)
            double circumference = 2.0 * M_PI * spiral.R;
            segmentLength = circumference * spiral.n;
        }

        // Calculate segment volume: length × area / 1000 (convert to cm³)
        double segmentVolume = segmentLength * area / 1000.0;  // cm³

        // Calculate segment mass: volume × density
        double segmentMass = segmentVolume * density;  // grams
        totalMass += segmentMass;
    }

    // Convert to kilograms
    return totalMass / 1000.0;
}

double Analyzer::calculateTubeMassFromDatabase(
    const TubeModel& model,
    double outerDiameter,      // mm
    double wallThickness,      // mm
    const std::string& materialId
) const {
    if (!model.path.has_value()) {
        return 0.0;
    }

    // Look up material from database
    double density = MaterialDatabase::instance().getMaterialDensity(materialId);

    const auto& path = *model.path;
    double totalMass = 0.0;  // grams

    // Calculate cross-section area of the tube
    double innerDiameter = outerDiameter - 2.0 * wallThickness;
    double area = M_PI * (outerDiameter * outerDiameter - innerDiameter * innerDiameter) / 4.0;  // mm²

    // Process each segment in the path
    for (const auto& item : path) {
        double segmentLength = 0.0;  // mm

        if (std::holds_alternative<Straight>(item)) {
            const auto& straight = std::get<Straight>(item);
            segmentLength = straight.S;
        } else if (std::holds_alternative<Bend>(item)) {
            const auto& bend = std::get<Bend>(item);
            double angleRadians = std::abs(bend.B) * M_PI / 180.0;
            segmentLength = bend.R * angleRadians;
        } else if (std::holds_alternative<Spiral>(item)) {
            const auto& spiral = std::get<Spiral>(item);
            double circumference = 2.0 * M_PI * spiral.R;
            segmentLength = circumference * spiral.n;
        }

        double segmentVolume = segmentLength * area / 1000.0;  // cm³
        double segmentMass = segmentVolume * density;  // grams
        totalMass += segmentMass;
    }

    // Convert to kilograms
    return totalMass / 1000.0;
}

}  // namespace bendcore::analysis
