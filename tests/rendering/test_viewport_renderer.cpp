#include <gtest/gtest.h>
#include "bendcore/rendering/viewport_renderer.hpp"
#include <glm/glm.hpp>

using namespace bendcore::rendering;

class ViewportRendererTest : public ::testing::Test {
protected:
    ViewportSettings default_settings{};
};

// Basic initialization and lifecycle tests
TEST_F(ViewportRendererTest, Initialize) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();
    EXPECT_TRUE(renderer.supports_gpu_acceleration());
    renderer.shutdown();
}

TEST_F(ViewportRendererTest, DoubleInitialize) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();
    renderer.initialize(); // Should not crash
    renderer.shutdown();
}

TEST_F(ViewportRendererTest, Shutdown) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();
    renderer.shutdown();
    // Should be safe to call again
    renderer.shutdown();
}

// Object management tests
TEST_F(ViewportRendererTest, AddObject) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    RenderObject obj;
    obj.vertices = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    obj.indices = {0, 1, 2};

    uint32_t id = renderer.add_object(obj);
    EXPECT_GT(id, 0);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, AddMultipleObjects) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    std::vector<uint32_t> ids;
    for (int i = 0; i < 10; ++i) {
        RenderObject obj;
        obj.vertices = {{0.0f, 0.0f, 0.0f}};
        obj.indices = {0};
        ids.push_back(renderer.add_object(obj));
    }

    // IDs should be unique and sequential
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, UpdateObject) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    RenderObject obj;
    obj.vertices = {{0.0f, 0.0f, 0.0f}};
    obj.indices = {0};
    uint32_t id = renderer.add_object(obj);

    obj.vertices = {{1.0f, 1.0f, 1.0f}};
    renderer.update_object(id, obj);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, RemoveObject) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    RenderObject obj;
    obj.vertices = {{0.0f, 0.0f, 0.0f}};
    obj.indices = {0};
    uint32_t id = renderer.add_object(obj);

    renderer.remove_object(id);
    renderer.shutdown();
}

TEST_F(ViewportRendererTest, ClearObjects) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    for (int i = 0; i < 5; ++i) {
        RenderObject obj;
        obj.vertices = {{0.0f, 0.0f, 0.0f}};
        obj.indices = {0};
        renderer.add_object(obj);
    }

    renderer.clear_objects();
    renderer.shutdown();
}

// Camera control tests
TEST_F(ViewportRendererTest, SetCamera) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    CameraState camera;
    camera.position = {5.0f, 5.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};

    renderer.set_camera(camera);
    CameraState retrieved = renderer.get_camera();

    EXPECT_EQ(retrieved.position, camera.position);
    EXPECT_EQ(retrieved.target, camera.target);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, ResetView) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    CameraState modified;
    modified.position = {100.0f, 100.0f, 100.0f};
    renderer.set_camera(modified);

    renderer.reset_view();
    CameraState reset = renderer.get_camera();

    EXPECT_EQ(reset.position.z, 10.0f); // Default z position

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, FitViewToObjects) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    RenderObject obj;
    obj.vertices = {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
    obj.indices = {0, 1};
    renderer.add_object(obj);

    renderer.fit_view_to_objects();
    CameraState camera = renderer.get_camera();

    EXPECT_EQ(camera.target, glm::vec3(0.0f, 0.0f, 0.0f));

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, FitViewWithEmptyObjects) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    renderer.fit_view_to_objects();
    CameraState camera = renderer.get_camera();

    // Should reset to default view
    EXPECT_EQ(camera.position.z, 10.0f);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, CameraRotation) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    CameraState initial = renderer.get_camera();

    ViewportEvent event;
    event.type = ViewportEvent::MOUSE_MOVE;
    event.delta_x = 10.0f;
    event.delta_y = 5.0f;
    renderer.handle_event(event);

    CameraState rotated = renderer.get_camera();
    EXPECT_NE(rotated.rotation_y, initial.rotation_y);
    EXPECT_NE(rotated.rotation_x, initial.rotation_x);

    renderer.shutdown();
}

// Event handling tests
TEST_F(ViewportRendererTest, HandleMouseWheel) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    CameraState initial = renderer.get_camera();
    float initial_zoom = initial.zoom;

    ViewportEvent event;
    event.type = ViewportEvent::MOUSE_WHEEL;
    event.delta_y = 1.0f;
    renderer.handle_event(event);

    CameraState zoomed = renderer.get_camera();
    EXPECT_NE(zoomed.zoom, initial_zoom);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, HandleKeyPress) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    CameraState initial = renderer.get_camera();

    ViewportEvent event;
    event.type = ViewportEvent::KEY_PRESS;
    event.key_code = 'H'; // Home/reset view
    renderer.handle_event(event);

    CameraState reset = renderer.get_camera();
    EXPECT_EQ(reset.position.z, 10.0f);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, CustomEventHandler) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    bool handler_called = false;
    renderer.set_event_handler([&](const ViewportEvent&) {
        handler_called = true;
    });

    ViewportEvent event;
    event.type = ViewportEvent::MOUSE_CLICK;
    renderer.handle_event(event);

    EXPECT_TRUE(handler_called);

    renderer.shutdown();
}

// Performance tests
TEST_F(ViewportRendererTest, RenderCycle) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    for (int i = 0; i < 100; ++i) {
        renderer.render();
    }

    float fps = renderer.get_fps();
    EXPECT_GT(fps, 0.0f);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, SetTargetFPS) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    renderer.set_target_fps(120.0f);
    renderer.render();

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, LODControl) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    renderer.set_lod_enabled(false);
    EXPECT_FALSE(renderer.get_lod_enabled());

    renderer.set_lod_enabled(true);
    EXPECT_TRUE(renderer.get_lod_enabled());

    renderer.shutdown();
}

// Viewport configuration tests
TEST_F(ViewportRendererTest, SetViewportSize) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    renderer.set_viewport_size(1280, 720);
    // Size change should be recorded internally

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, SetClearColor) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    glm::vec4 color(1.0f, 0.5f, 0.25f, 1.0f);
    renderer.set_clear_color(color);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, WireframeMode) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    renderer.set_wireframe_mode(true);
    renderer.set_wireframe_mode(false);

    renderer.shutdown();
}

// GPU capability tests
TEST_F(ViewportRendererTest, GPUSupport) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    bool supports = renderer.supports_gpu_acceleration();
    EXPECT_TRUE(supports);

    std::string gpu_info = renderer.get_gpu_info();
    EXPECT_FALSE(gpu_info.empty());

    renderer.shutdown();
}

// Integration tests
TEST_F(ViewportRendererTest, ComplexSceneRender) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    // Create complex scene
    for (int i = 0; i < 20; ++i) {
        RenderObject obj;
        obj.vertices = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        obj.indices = {0, 1, 2};
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(i, 0, 0));
        renderer.add_object(obj);
    }

    // Render multiple frames
    for (int frame = 0; frame < 30; ++frame) {
        renderer.render();
    }

    EXPECT_GT(renderer.get_fps(), 0.0f);

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, DynamicObjectUpdate) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    RenderObject obj;
    obj.vertices = {{0.0f, 0.0f, 0.0f}};
    obj.indices = {0};
    uint32_t id = renderer.add_object(obj);

    // Update object over time
    for (int i = 0; i < 10; ++i) {
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(i * 0.1f, 0, 0));
        renderer.update_object(id, obj);
        renderer.render();
    }

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, CameraAnimation) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    CameraState camera = renderer.get_camera();

    // Animate camera
    for (int i = 0; i < 60; ++i) {
        camera.rotation_y += 0.05f;
        renderer.set_camera(camera);
        renderer.render();
    }

    renderer.shutdown();
}

// Memory and resource tests
TEST_F(ViewportRendererTest, LargeObjectCount) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    for (int i = 0; i < 1000; ++i) {
        RenderObject obj;
        obj.vertices = {{0.0f, 0.0f, 0.0f}};
        obj.indices = {0};
        renderer.add_object(obj);
    }

    renderer.render();
    renderer.clear_objects();

    renderer.shutdown();
}

TEST_F(ViewportRendererTest, LargeVertexBuffers) {
    ViewportRenderer renderer(default_settings);
    renderer.initialize();

    RenderObject obj;
    // Create large vertex buffer
    for (int i = 0; i < 10000; ++i) {
        obj.vertices.push_back({float(i % 100), float(i / 100), 0.0f});
        if (obj.vertices.size() >= 3) {
            obj.indices.push_back(obj.vertices.size() - 1);
        }
    }

    renderer.add_object(obj);
    renderer.render();

    renderer.shutdown();
}
