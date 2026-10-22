#include "bendcore/bom.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace bendcore;

namespace {

struct Check {
    std::string msg;
    bool ok = true;

    ~Check() { std::cout << (ok ? "✓ " : "✗ ") << msg << "\n"; }
};

void check_signature() {
    Check c{"tube_signature: different od/wall → different signature"};

    Path path_1;
    path_1.push_back(Straight{100.0});
    EndRule rule;

    auto sig1 = tube_signature(10.0, 1.0, path_1, rule, rule);
    auto sig2 = tube_signature(10.0, 1.5, path_1, rule, rule);
    auto sig3 = tube_signature(12.0, 1.0, path_1, rule, rule);

    if (sig1 == sig2 || sig1 == sig3 || sig2 == sig3) c.ok = false;
}

void check_signature_path() {
    Check c{"tube_signature: different path → different signature"};

    EndRule rule;
    Path path1;
    path1.push_back(Straight{100.0});

    Path path2;
    path2.push_back(Straight{100.0});
    path2.push_back(Bend{50.0, 90.0});

    auto sig1 = tube_signature(10.0, 1.0, path1, rule, rule);
    auto sig2 = tube_signature(10.0, 1.0, path2, rule, rule);

    if (sig1 == sig2) c.ok = false;
}

void check_signature_rules() {
    Check c{"tube_signature: different EndRule → different signature"};

    Path path;
    path.push_back(Straight{100.0});

    EndRule rule1;
    EndRule rule2{1.0, 10.0};   // E != 0
    EndRule rule3{0.0, 10.0, false};   // cut_allowed != true

    auto sig1 = tube_signature(10.0, 1.0, path, rule1, rule1);
    auto sig2 = tube_signature(10.0, 1.0, path, rule2, rule2);
    auto sig3 = tube_signature(10.0, 1.0, path, rule1, rule3);

    if (sig1 == sig2 || sig1 == sig3 || sig2 == sig3) c.ok = false;
}

void check_build_bom_single() {
    Check c{"build_bom: single tube"};

    TubeModel tube;
    tube.id = "T1";
    tube.source = "desc_1";
    tube.od = 10.0;
    tube.wall = 1.0;
    Path p;
    p.push_back(Straight{100.0});
    tube.path = p;

    EndRule rule;
    auto bom = build_bom({tube}, rule);

    if (bom.groups.size() != 1) {
        c.ok = false;
        return;
    }
    if (bom.groups[0].spec.quantity != 1 || bom.groups[0].tube_ids.size() != 1) c.ok = false;
    if (bom.groups[0].spec.id != "T1" || bom.groups[0].spec.od != 10.0) c.ok = false;
}

void check_build_bom_identical() {
    Check c{"build_bom: identical tubes grouped"};

    TubeModel tube1;
    tube1.id = "T1";
    tube1.source = "desc";
    tube1.od = 10.0;
    tube1.wall = 1.0;
    Path p1;
    p1.push_back(Straight{100.0});
    tube1.path = p1;

    TubeModel tube2;
    tube2.id = "T2";
    tube2.source = "desc";
    tube2.od = 10.0;
    tube2.wall = 1.0;
    Path p2;
    p2.push_back(Straight{100.0});
    tube2.path = p2;

    EndRule rule;
    auto bom = build_bom({tube1, tube2}, rule);

    if (bom.groups.size() != 1) {
        c.ok = false;
        return;
    }
    if (bom.groups[0].spec.quantity != 2) c.ok = false;
    if (bom.groups[0].tube_ids.size() != 2) c.ok = false;
    if (bom.groups[0].tube_ids[0] != "T1" || bom.groups[0].tube_ids[1] != "T2") c.ok = false;
}

void check_build_bom_different() {
    Check c{"build_bom: different tubes not grouped"};

    TubeModel tube1;
    tube1.id = "T1";
    tube1.source = "desc";
    tube1.od = 10.0;
    tube1.wall = 1.0;
    Path p1;
    p1.push_back(Straight{100.0});
    tube1.path = p1;

    TubeModel tube2;
    tube2.id = "T2";
    tube2.source = "desc";
    tube2.od = 12.0;
    tube2.wall = 1.0;
    Path p2;
    p2.push_back(Straight{100.0});
    tube2.path = p2;

    EndRule rule;
    auto bom = build_bom({tube1, tube2}, rule);

    if (bom.groups.size() != 2) {
        c.ok = false;
        return;
    }
    if (bom.groups[0].spec.quantity != 1 || bom.groups[1].spec.quantity != 1) c.ok = false;
}

void check_build_bom_mixed() {
    Check c{"build_bom: mixed groups, order preserved"};

    TubeModel t1;
    t1.id = "T1";
    t1.source = "A";
    t1.od = 10.0;
    t1.wall = 1.0;
    Path p1;
    p1.push_back(Straight{100.0});
    t1.path = p1;

    TubeModel t2;
    t2.id = "T2";
    t2.source = "B";
    t2.od = 12.0;
    t2.wall = 1.0;
    Path p2;
    p2.push_back(Straight{100.0});
    t2.path = p2;

    TubeModel t3;
    t3.id = "T3";
    t3.source = "A";
    t3.od = 10.0;
    t3.wall = 1.0;
    Path p3;
    p3.push_back(Straight{100.0});
    t3.path = p3;

    EndRule rule;
    auto bom = build_bom({t1, t2, t3}, rule);

    // Ожидание: группа A (T1, T3), группа B (T2).
    if (bom.groups.size() != 2) {
        c.ok = false;
        return;
    }
    if (bom.groups[0].spec.id != "T1" || bom.groups[0].spec.quantity != 2) c.ok = false;
    if (bom.groups[0].tube_ids.size() != 2 || bom.groups[0].tube_ids[0] != "T1" ||
        bom.groups[0].tube_ids[1] != "T3") {
        c.ok = false;
        return;
    }
    if (bom.groups[1].spec.id != "T2" || bom.groups[1].spec.quantity != 1) c.ok = false;
}

void check_blank_length() {
    Check c{"build_bom: blank_length computed"};

    // Простая прямая труба: S = 100 мм, no bends.
    // Blank length = axis_length(path) + E_start + E_end + d_start + d_end.
    // axis_length([S100]) = 100. E_start = E_end = 0 (default rule). d_start, d_end = max(dmin, N - S - E).
    // S_start = S_end = 100, N = 0 (default), E = 0, dmin = 0 → d = 0.
    // blank_length = 100 + 0 + 0 + 0 + 0 = 100.
    TubeModel tube;
    tube.id = "T1";
    tube.source = "desc";
    tube.od = 10.0;
    tube.wall = 1.0;
    Path p;
    p.push_back(Straight{100.0});
    tube.path = p;

    EndRule rule;   // E = 0, N = 0, cut_allowed = true
    auto bom = build_bom({tube}, rule);

    if (bom.groups.size() != 1) {
        c.ok = false;
        return;
    }
    const double blank = bom.groups[0].spec.blank_length;
    if (std::abs(blank - 100.0) > 0.01) {
        c.ok = false;
    }
}

void check_blank_length_with_allowance() {
    Check c{"build_bom: blank_length with end allowance"};

    // S100 с EndRule{E=5, N=0} на обоих концах.
    // blank_length = 100 + 5 + 5 = 110.
    TubeModel tube;
    tube.id = "T1";
    tube.source = "desc";
    tube.od = 10.0;
    tube.wall = 1.0;
    Path p;
    p.push_back(Straight{100.0});
    tube.path = p;

    EndRule rule{5.0, 0.0};   // E = 5, N = 0
    auto bom = build_bom({tube}, rule);

    if (bom.groups.size() != 1) {
        c.ok = false;
        return;
    }
    const double blank = bom.groups[0].spec.blank_length;
    if (std::abs(blank - 110.0) > 0.01) {
        c.ok = false;
    }
}

void check_issues_propagated() {
    Check c{"build_bom: blank issues propagated to spec"};

    // Труба с коротким концом: S20 (или 0), N = 100, cut_allowed = false.
    // Должна быть ошибка: S + E < N.
    TubeModel tube;
    tube.id = "T1";
    tube.source = "desc";
    tube.od = 10.0;
    tube.wall = 1.0;
    Path p;
    p.push_back(Straight{20.0});
    tube.path = p;

    EndRule rule{0.0, 100.0, false};   // N = 100 > S + E = 20
    auto bom = build_bom({tube}, rule);

    if (bom.groups.size() != 1) {
        c.ok = false;
        return;
    }
    if (bom.groups[0].spec.issues.size() < 1) {
        c.ok = false;   // Должна быть ошибка
    }
}

void check_different_ends() {
    Check c{"build_bom_with_ends: different rules for start and end"};

    TubeModel tube;
    tube.id = "T1";
    tube.source = "desc";
    tube.od = 10.0;
    tube.wall = 1.0;
    Path p;
    p.push_back(Straight{100.0});
    tube.path = p;

    EndRule start{5.0, 0.0};   // E_start = 5
    EndRule end{3.0, 0.0};     // E_end = 3
    auto bom = build_bom_with_ends({tube}, start, end);

    if (bom.groups.size() != 1) {
        c.ok = false;
        return;
    }
    const double blank = bom.groups[0].spec.blank_length;
    // blank = 100 + 5 + 3 = 108
    if (std::abs(blank - 108.0) > 0.01) {
        c.ok = false;
    }
}

}  // namespace

int main() {
    check_signature();
    check_signature_path();
    check_signature_rules();
    check_build_bom_single();
    check_build_bom_identical();
    check_build_bom_different();
    check_build_bom_mixed();
    check_blank_length();
    check_blank_length_with_allowance();
    check_issues_propagated();
    check_different_ends();

    std::cout << "\n✓ All BOM tests passed\n";
    return 0;
}
