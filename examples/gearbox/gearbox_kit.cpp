// Запись комплекта редуктора: документы bendcore (деталь, сборка, чертёж, спецификация), STEP
// сборки (AP214, имена и обозначения) и деталей, один многостраничный PDF всех листов (рабочие
// чертежи, сборочный чертёж, спецификация) и DXF каждого листа.

#include "gearbox.hpp"

#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/drawing_v2/truetype_font.hpp>
#include <bendcore/kernel/step_io.hpp>

#include <filesystem>
#include <memory>

namespace bendcore::examples::gearbox {

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::document;

namespace {

Status save(const std::filesystem::path& path, const std::string& bytes, std::vector<std::string>* list = nullptr) {
    auto n = saveToFile(path.string(), bytes);
    if (!n.ok()) return n.error();
    if (list) list->push_back(path.string());
    return std::monostate{};
}

// Шрифт невидимого текстового слоя PDF (поиск, pdftotext); листы выводятся шрифтом ГОСТ 2.304.
const TrueTypeFont* textLayerFont() {
    static std::unique_ptr<TrueTypeFont> font = [] {
        auto path = findSystemFont();
        if (!path.ok()) return std::unique_ptr<TrueTypeFont>();
        auto f = TrueTypeFont::loadFile(path.value());
        return f.ok() ? std::make_unique<TrueTypeFont>(std::move(f).value()) : std::unique_ptr<TrueTypeFont>();
    }();
    return font.get();
}

}  // namespace

Result<KitFiles> writeKit(const std::string& directory, const std::vector<PartEntry>& parts,
                          const std::vector<StandardPart>& standard, const Gearbox& g, const DrawingSet& set,
                          const KitOptions& options) {
    namespace fs = std::filesystem;
    const fs::path dir(directory);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) return Error{ErrorCode::IoError, "cannot create '" + directory + "': " + ec.message()};
    KitFiles out;

    for (const auto& p : parts) {
        auto j = p.doc->toJson();
        if (!j.ok()) return Error{j.error().code, p.file + ": " + j.error().message};
        if (auto st = save(dir / p.file, j.value(), &out.partDocuments); !st.ok()) return st.error();
    }
    for (const auto& s : standard) {
        auto j = s.doc->toJson();
        if (!j.ok()) return Error{j.error().code, s.key + ": " + j.error().message};
        if (auto st = save(dir / (s.key + ".bcpart.json"), j.value(), &out.partDocuments); !st.ok()) return st.error();
    }
    assembly_v2::SaveOptions so;
    so.embedSourcedGeometry = false;  // детали — ссылками на свои документы, крепёж встроен
    out.assemblyDocument = (dir / "gearbox.bcasm.json").string();
    if (auto st = g.doc->saveFile(out.assemblyDocument, so); !st.ok()) return st.error();

    for (const auto& pd : set.parts) {
        out.drawingDocuments.push_back((dir / pd.file).string());
        if (auto st = pd.doc.saveFile(out.drawingDocuments.back()); !st.ok()) return Error{st.error().code, pd.file + ": " + st.error().message};
    }
    out.drawingDocuments.push_back((dir / "gearbox.bcdrw.json").string());
    if (auto st = set.assembly.saveFile(out.drawingDocuments.back()); !st.ok()) return st.error();
    auto spec = set.specification.toJson();
    if (!spec.ok()) return spec.error();
    out.specificationDocument = (dir / "gearbox.bcspec.json").string();
    if (auto st = save(out.specificationDocument, spec.value()); !st.ok()) return st.error();

    auto step = g.doc->exportStep();
    if (!step.ok()) return Error{step.error().code, "assembly STEP: " + step.error().message};
    out.assemblyStep = (dir / "gearbox.step").string();
    if (auto st = save(out.assemblyStep, step.value()); !st.ok()) return st.error();
    if (options.partSteps) {
        for (const auto& p : parts) {
            auto s = kernel::writeStep(kernel::Shape(p.doc->finalShape().shape()));
            if (!s.ok()) return Error{s.error().code, p.key + " STEP: " + s.error().message};
            if (auto st = save(dir / (p.key + ".step"), s.value(), &out.partSteps); !st.ok()) return st.error();
        }
    }

    // Листы: рабочие чертежи, сборочный чертёж, спецификация.
    const TrueTypeFont* font = textLayerFont();
    std::vector<std::pair<std::string, std::vector<Drawing>>> rendered;
    for (const auto& pd : set.parts) {
        auto r = pd.doc.render(font);
        if (!r.ok()) return Error{r.error().code, pd.file + ": " + r.error().message};
        rendered.emplace_back(pd.key, std::move(r).value());
    }
    auto ar = set.assembly.render(font);
    if (!ar.ok()) return Error{ar.error().code, "assembly drawing: " + ar.error().message};
    rendered.emplace_back("gearbox", std::move(ar).value());
    auto sr = set.specification.render(font);
    if (!sr.ok()) return Error{sr.error().code, "specification: " + sr.error().message};
    rendered.emplace_back("gearbox_SP", std::move(sr).value().sheets);

    std::vector<const Drawing*> pages;
    for (const auto& [base, sheets] : rendered) {
        for (std::size_t i = 0; i < sheets.size(); ++i) {
            pages.push_back(&sheets[i]);
            if (!options.dxf) continue;
            auto dxf = writeDxf(sheets[i]);
            if (!dxf.ok()) return Error{dxf.error().code, base + " DXF: " + dxf.error().message};
            const std::string name = base == "gearbox_SP" ? "gearbox_SP" + std::to_string(i + 1) + ".dxf"
                                                          : base + "_S" + std::to_string(i + 1) + ".dxf";
            if (auto st = save(dir / name, dxf.value(), &out.dxf); !st.ok()) return st.error();
        }
    }
    out.pdfPages = int(pages.size());
    if (options.pdf) {
        PdfOptions po;
        po.font = font;
        po.title = "РЦ.080.00.000 Редуктор цилиндрический — комплект документации";
        auto pdf = writePdfPages(pages, po);
        if (!pdf.ok()) return Error{pdf.error().code, "PDF: " + pdf.error().message};
        out.pdf = (dir / "gearbox_kit.pdf").string();
        if (auto st = save(out.pdf, pdf.value()); !st.ok()) return st.error();
    }
    return out;
}

}  // namespace bendcore::examples::gearbox
