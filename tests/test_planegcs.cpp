// SPDX-License-Identifier: LGPL-2.1-or-later
// Дымовой тест PlaneGCS (извлечён из FreeCAD Sketcher): степени свободы, решение, диагностика.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "GCS.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

int main() {
    // Точка 1 закреплена в (0, 0); точка 2 свободна; расстояние 10 мм → 1 степень свободы (направление).
    double x1 = 0.0, y1 = 0.0, x2 = 7.0, y2 = 3.0;
    double zero = 0.0;
    double dist = 10.0;

    GCS::Point p1, p2;
    p1.x = &x1; p1.y = &y1;
    p2.x = &x2; p2.y = &y2;

    GCS::System sys;
    GCS::VEC_pD params = {&x1, &y1, &x2, &y2};
    sys.declareUnknowns(params);
    sys.addConstraintCoordinateX(p1, &zero, 1);
    sys.addConstraintCoordinateY(p1, &zero, 2);
    sys.addConstraintP2PDistance(p1, p2, &dist, 3);
    sys.initSolution();

    int dofs = sys.diagnose();
    check(dofs == 1, "dofs == 1 (distance fixes one of two free coordinates)");
    check(sys.dofsNumber() == 1, "dofsNumber() == 1");

    GCS::SolveStatus status = sys.solve();
    check(status == GCS::SolveStatus::Success || status == GCS::SolveStatus::Converged, "solve converges");
    sys.applySolution();

    double len = std::hypot(x2 - x1, y2 - y1);
    check(std::fabs(len - 10.0) < 1e-8, "solved distance is 10");

    // Избыточное ограничение: вторая фиксация X той же точки — должна попасть в конфликт/избыточные
    GCS::System sys2;
    double a = 0.0, b = 0.0;
    GCS::Point q;
    q.x = &a; q.y = &b;
    GCS::VEC_pD qp = {&a, &b};
    sys2.declareUnknowns(qp);
    sys2.addConstraintCoordinateX(q, &zero, 1);
    sys2.addConstraintCoordinateX(q, &zero, 2);
    sys2.initSolution();
    sys2.diagnose();
    GCS::VEC_I redundant;
    sys2.getRedundant(redundant);
    check(!redundant.empty(), "duplicate constraint is reported as redundant");

    if (failures == 0) {
        std::printf("OK planegcs smoke: dofs=%d, distance=%.10f\n", dofs, len);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
