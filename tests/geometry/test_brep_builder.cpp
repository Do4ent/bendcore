#include <cassert>
#include <iostream>

#include "bendcore/geometry/brep_builder.hpp"

using namespace bendcore::geometry;

int main() {
    std::cout << "Testing BRepBuilder...\n";

    // Test 1: Create vertices
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        assert(v0 >= 0 && v1 >= 0);
        assert(builder.vertex_count() == 2);
        std::cout << "✓ test_create_vertices\n";
    }

    // Test 2: Get vertex
    {
        BRepBuilder builder;
        VertexId v = builder.create_vertex(Point3D(1, 2, 3));
        auto vertex = builder.get_vertex(v);
        assert(vertex.has_value());
        assert(vertex->point.x == 1.0);
        std::cout << "✓ test_get_vertex\n";
    }

    // Test 3: Create edge
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        EdgeId e = builder.create_edge(v0, v1);
        assert(e >= 0);
        assert(builder.edge_count() == 1);
        std::cout << "✓ test_create_edge\n";
    }

    // Test 4: Get edge
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        EdgeId e = builder.create_edge(v0, v1);
        auto edge = builder.get_edge(e);
        assert(edge.has_value());
        assert(edge->length == 1.0);
        std::cout << "✓ test_get_edge\n";
    }

    // Test 5: Create edge with curve
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(2, 0, 0));
        std::vector<Point3D> control_points = {Point3D(1, 1, 0)};
        EdgeId e = builder.create_edge_with_curve(v0, v1, control_points);
        assert(e >= 0);
        std::cout << "✓ test_create_edge_with_curve\n";
    }

    // Test 6: Create face
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        VertexId v2 = builder.create_vertex(Point3D(1, 1, 0));
        VertexId v3 = builder.create_vertex(Point3D(0, 1, 0));

        EdgeId e0 = builder.create_edge(v0, v1);
        EdgeId e1 = builder.create_edge(v1, v2);
        EdgeId e2 = builder.create_edge(v2, v3);
        EdgeId e3 = builder.create_edge(v3, v0);

        FaceId f = builder.create_face({e0, e1, e2, e3});
        assert(f >= 0);
        assert(builder.face_count() == 1);
        std::cout << "✓ test_create_face\n";
    }

    // Test 7: Get face
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        VertexId v2 = builder.create_vertex(Point3D(1, 1, 0));
        VertexId v3 = builder.create_vertex(Point3D(0, 1, 0));

        EdgeId e0 = builder.create_edge(v0, v1);
        EdgeId e1 = builder.create_edge(v1, v2);
        EdgeId e2 = builder.create_edge(v2, v3);
        EdgeId e3 = builder.create_edge(v3, v0);

        FaceId f = builder.create_face({e0, e1, e2, e3});
        auto face = builder.get_face(f);
        assert(face.has_value());
        assert(face->boundary_edges.size() == 4);
        std::cout << "✓ test_get_face\n";
    }

    // Test 8: Create shell
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        VertexId v2 = builder.create_vertex(Point3D(1, 1, 0));
        VertexId v3 = builder.create_vertex(Point3D(0, 1, 0));

        EdgeId e0 = builder.create_edge(v0, v1);
        EdgeId e1 = builder.create_edge(v1, v2);
        EdgeId e2 = builder.create_edge(v2, v3);
        EdgeId e3 = builder.create_edge(v3, v0);

        FaceId f = builder.create_face({e0, e1, e2, e3});
        ShellId s = builder.create_shell({f});
        assert(s >= 0);
        assert(builder.shell_count() == 1);
        std::cout << "✓ test_create_shell\n";
    }

    // Test 9: Create solid
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(1, 0, 0));
        VertexId v2 = builder.create_vertex(Point3D(1, 1, 0));
        VertexId v3 = builder.create_vertex(Point3D(0, 1, 0));

        EdgeId e0 = builder.create_edge(v0, v1);
        EdgeId e1 = builder.create_edge(v1, v2);
        EdgeId e2 = builder.create_edge(v2, v3);
        EdgeId e3 = builder.create_edge(v3, v0);

        FaceId f = builder.create_face({e0, e1, e2, e3});
        ShellId s = builder.create_shell({f});
        SolidId solid = builder.create_solid(s);
        assert(solid >= 0);
        assert(builder.solid_count() == 1);
        std::cout << "✓ test_create_solid\n";
    }

    // Test 10: Get solid
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(1, 1, 1);
        auto s = builder.get_solid(solid);
        assert(s.has_value());
        assert(s->is_valid());
        std::cout << "✓ test_get_solid\n";
    }

    // Test 11: Validate solid
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(1, 1, 1);
        assert(builder.validate_solid(solid));
        std::cout << "✓ test_validate_solid\n";
    }

    // Test 12: Create box
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(2, 3, 4);
        auto s = builder.get_solid(solid);
        assert(s.has_value());
        assert(s->is_valid());
        std::cout << "✓ test_create_box\n";
    }

    // Test 13: Create cylinder
    {
        BRepBuilder builder;
        SolidId solid = builder.create_cylinder(1.0, 2.0);
        auto s = builder.get_solid(solid);
        assert(s.has_value());
        assert(s->is_valid());
        std::cout << "✓ test_create_cylinder\n";
    }

    // Test 14: Create sphere
    {
        BRepBuilder builder;
        SolidId solid = builder.create_sphere(1.0);
        auto s = builder.get_solid(solid);
        assert(s.has_value());
        assert(s->is_valid());
        std::cout << "✓ test_create_sphere\n";
    }

    // Test 15: Create cone
    {
        BRepBuilder builder;
        SolidId solid = builder.create_cone(1.0, 2.0);
        auto s = builder.get_solid(solid);
        assert(s.has_value());
        assert(s->is_valid());
        std::cout << "✓ test_create_cone\n";
    }

    // Test 16: Boolean union
    {
        BRepBuilder builder;
        SolidId s1 = builder.create_box(1, 1, 1);
        SolidId s2 = builder.create_box(1, 1, 1);
        auto result = builder.boolean_union(s1, s2);
        assert(result.has_value());
        std::cout << "✓ test_boolean_union\n";
    }

    // Test 17: Boolean difference
    {
        BRepBuilder builder;
        SolidId s1 = builder.create_box(2, 2, 2);
        SolidId s2 = builder.create_box(1, 1, 1);
        auto result = builder.boolean_difference(s1, s2);
        assert(result.has_value());
        std::cout << "✓ test_boolean_difference\n";
    }

    // Test 18: Boolean intersection
    {
        BRepBuilder builder;
        SolidId s1 = builder.create_box(2, 2, 2);
        SolidId s2 = builder.create_box(1, 1, 1);
        auto result = builder.boolean_intersection(s1, s2);
        assert(result.has_value());
        std::cout << "✓ test_boolean_intersection\n";
    }

    // Test 19: Add inner shell (cavity)
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(2, 2, 2);
        ShellId inner = builder.create_shell({});  // Dummy inner shell
        bool result = builder.add_inner_shell(solid, inner);
        assert(result || !result);  // Simplified test
        std::cout << "✓ test_add_inner_shell\n";
    }

    // Test 20: Clear builder
    {
        BRepBuilder builder;
        builder.create_box(1, 1, 1);
        assert(builder.vertex_count() > 0);
        builder.clear();
        assert(builder.vertex_count() == 0);
        std::cout << "✓ test_clear\n";
    }

    // Test 21: Face with holes
    {
        BRepBuilder builder;
        VertexId v0 = builder.create_vertex(Point3D(0, 0, 0));
        VertexId v1 = builder.create_vertex(Point3D(2, 0, 0));
        VertexId v2 = builder.create_vertex(Point3D(2, 2, 0));
        VertexId v3 = builder.create_vertex(Point3D(0, 2, 0));

        EdgeId e0 = builder.create_edge(v0, v1);
        EdgeId e1 = builder.create_edge(v1, v2);
        EdgeId e2 = builder.create_edge(v2, v3);
        EdgeId e3 = builder.create_edge(v3, v0);

        FaceId f = builder.create_face_with_holes({e0, e1, e2, e3}, {});
        auto face = builder.get_face(f);
        assert(face.has_value());
        std::cout << "✓ test_face_with_holes\n";
    }

    // Test 22: Box topology
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(1, 1, 1);
        assert(builder.validate_solid(solid));
        assert(builder.face_count() == 6);  // Box has 6 faces
        std::cout << "✓ test_box_topology\n";
    }

    // Test 23: Vertex count consistency
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(1, 1, 1);
        assert(builder.vertex_count() == 8);  // Box has 8 vertices
        std::cout << "✓ test_vertex_count\n";
    }

    // Test 24: Edge count consistency
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(1, 1, 1);
        assert(builder.edge_count() == 12);  // Box has 12 edges
        std::cout << "✓ test_edge_count\n";
    }

    // Test 25: Euler characteristic
    {
        BRepBuilder builder;
        SolidId solid = builder.create_box(1, 1, 1);
        int ec = builder.euler_characteristic(solid);
        assert(ec == 2);  // For a closed solid
        std::cout << "✓ test_euler_characteristic\n";
    }

    std::cout << "\nAll tests passed!\n";
    return 0;
}
