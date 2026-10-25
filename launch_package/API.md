# BendCore CAD API Documentation

## Overview

The BendCore API provides C++20 interfaces for creating and manipulating CAD documents.

## Core Components

### Phase A: Document Model

#### EntityId
```cpp
class EntityId {
  enum class Type { Part, Instance, Body, Sketch, Feature, ... };
  EntityId(Type type);
  uuid_t id() const;
  Type type() const;
  std::string to_string() const;
};
```

#### CadDocument
```cpp
class CadDocument {
  void add_entity(const Entity& entity);
  std::optional<Entity> get_entity(const EntityId& id);
  std::vector<Entity> get_children(const EntityId& parent_id);
  void remove_entity(const EntityId& id);
};
```

### Phase B: Commands & Transactions

#### CommandEngine
```cpp
class CommandEngine {
  void execute(std::shared_ptr<Command> cmd);
  void undo();
  void redo();
};
```

### Phase C: Parameters & Dependencies

#### ParameterStore
```cpp
class ParameterStore {
  void set(EntityId id, std::string name, ParameterValue value);
  std::optional<ParameterValue> get(EntityId id, std::string name);
  void link(EntityId source, std::string param, EntityId target);
};
```

### Phase D: Geometry & Topology

#### GeometryKernel
```cpp
class GeometryKernel {
  double point_distance(const Point3D& p1, const Point3D& p2);
  std::optional<Point3D> line_intersection(const Line& l1, const Line& l2);
  Vector3D compute_normal(const Point3D& p1, const Point3D& p2);
};
```

#### BRepBuilder
```cpp
class BRepBuilder {
  SolidId create_box(double width, double height, double depth);
  SolidId create_cylinder(double radius, double height);
  SolidId create_sphere(double radius);
  std::optional<SolidId> boolean_union(SolidId a, SolidId b);
  std::optional<SolidId> boolean_difference(SolidId a, SolidId b);
  std::optional<SolidId> boolean_intersection(SolidId a, SolidId b);
};
```

## Error Handling

The API uses C++20 `std::optional<T>` for optional results and exceptions for errors.

## Threading

Thread safety: Most classes are thread-safe for read operations. Write operations 
should be serialized through the CommandEngine.

