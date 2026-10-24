import * as THREE from 'three';

/**
 * Сервис для 3D визуализации трубопровода с гибками
 * Использует Three.js для отрисовки в WebGL
 */

interface Point3D {
  x: number;
  y: number;
  z: number;
}

interface BendDetail {
  position: Point3D;
  angle_xy: number;
  angle_z: number;
  radius: number;
}

export class Three3DRenderer {
  private container: HTMLDivElement;
  private scene: THREE.Scene;
  private camera: THREE.PerspectiveCamera;
  private renderer: THREE.WebGLRenderer;
  private controls: any; // Для простоты используем встроенный контроль
  private tubeGroup: THREE.Group;
  private gridHelper: THREE.GridHelper;
  private animationFrameId: number | null = null;

  constructor(container: HTMLDivElement) {
    this.container = container;

    // Инициализация сцены
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0xf3f4f6);
    this.scene.fog = new THREE.Fog(0xf3f4f6, 1000, 10000);

    // Инициализация камеры
    const width = container.clientWidth || 800;
    const height = container.clientHeight || 600;
    this.camera = new THREE.PerspectiveCamera(75, width / height, 0.1, 10000);
    this.camera.position.set(200, 200, 150);
    this.camera.lookAt(0, 0, 0);

    // Инициализация рендерера
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    this.renderer.setSize(width, height);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.renderer.shadowMap.enabled = true;
    container.appendChild(this.renderer.domElement);

    // Создание группы для трубы
    this.tubeGroup = new THREE.Group();
    this.scene.add(this.tubeGroup);

    // Добавление сетки координат
    this.gridHelper = new THREE.GridHelper(500, 50, 0xcccccc, 0xeeeeee);
    this.scene.add(this.gridHelper);

    // Добавление освещения
    this.setupLights();

    // Запуск анимации
    this.animate();

    // Обработка изменения размера окна
    window.addEventListener('resize', () => this.onWindowResize());

    // Включение контроля мышью
    this.enableMouseControls();
  }

  /**
   * Установка освещения сцены
   */
  private setupLights(): void {
    // Основной свет (directional light)
    const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
    directionalLight.position.set(300, 300, 200);
    directionalLight.castShadow = true;
    directionalLight.shadow.mapSize.width = 2048;
    directionalLight.shadow.mapSize.height = 2048;
    this.scene.add(directionalLight);

    // Заполняющий свет (ambient light)
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.4);
    this.scene.add(ambientLight);

    // Точечный свет для дополнительной глубины
    const pointLight = new THREE.PointLight(0xffffff, 0.4);
    pointLight.position.set(-200, 100, 300);
    this.scene.add(pointLight);
  }

  /**
   * Отрисовка трубопровода (основной метод)
   */
  public renderTube(
    startPoint: Point3D,
    endPoint: Point3D,
    bends: BendDetail[],
    outerDiameter: number,
    wallThickness: number
  ): void {
    // Очистка предыдущей трубы
    this.tubeGroup.clear();

    // Радиус трубы
    const tubeRadius = outerDiameter / 2;

    // Создание сегментов трубы между точками
    const allPoints = [startPoint, ...bends.map(b => b.position), endPoint];

    // Отрисовка прямых участков
    for (let i = 0; i < allPoints.length - 1; ++i) {
      const start = allPoints[i];
      const end = allPoints[i + 1];
      this.drawTubeSegment(start, end, tubeRadius, 0x3b82f6);
    }

    // Отрисовка гибок (как тор)
    bends.forEach((bend, index) => {
      this.drawBend(bend, tubeRadius, 0x10b981);

      // Добавление метки гибки
      this.addBendLabel(bend.position, index + 1);
    });

    // Отрисовка стартовой и конечной точек
    this.addPointMarker(startPoint, 'START', 0xef4444);
    this.addPointMarker(endPoint, 'END', 0xf59e0b);

    // Добавление оси координат
    this.addCoordinateAxes();
  }

  /**
   * Отрисовка прямого участка трубы как цилиндра
   */
  private drawTubeSegment(
    start: Point3D,
    end: Point3D,
    radius: number,
    color: number
  ): void {
    // Вычисление длины и направления
    const dx = end.x - start.x;
    const dy = end.y - start.y;
    const dz = end.z - start.z;
    const length = Math.sqrt(dx * dx + dy * dy + dz * dz);

    // Создание цилиндра (трубы)
    const geometry = new THREE.CylinderGeometry(radius, radius, length, 32);
    const material = new THREE.MeshStandardMaterial({
      color: color,
      metalness: 0.4,
      roughness: 0.6,
    });
    const cylinder = new THREE.Mesh(geometry, material);
    cylinder.castShadow = true;
    cylinder.receiveShadow = true;

    // Позиционирование посередине между start и end
    const midpoint = {
      x: (start.x + end.x) / 2,
      y: (start.y + end.y) / 2,
      z: (start.z + end.z) / 2,
    };
    cylinder.position.set(midpoint.x, midpoint.y, midpoint.z);

    // Ориентация цилиндра в направлении вектора
    const direction = new THREE.Vector3(dx, dy, dz).normalize();
    const up = new THREE.Vector3(0, 1, 0);
    if (Math.abs(direction.dot(up)) > 0.99) {
      up.set(1, 0, 0);
    }
    const axis = new THREE.Vector3().crossVectors(up, direction).normalize();
    const angle = Math.acos(up.dot(direction));
    cylinder.setRotationFromAxisAngle(axis, angle);

    this.tubeGroup.add(cylinder);
  }

  /**
   * Отрисовка гибки как тора
   */
  private drawBend(bend: BendDetail, radius: number, color: number): void {
    // Создание тора (для визуализации гибки)
    const geometry = new THREE.TorusGeometry(bend.radius, radius * 0.5, 16, 32);
    const material = new THREE.MeshStandardMaterial({
      color: color,
      metalness: 0.5,
      roughness: 0.5,
    });
    const torus = new THREE.Mesh(geometry, material);
    torus.castShadow = true;
    torus.receiveShadow = true;

    // Позиция гибки
    torus.position.set(bend.position.x, bend.position.y, bend.position.z);

    // Ориентация по углам
    const angleXYRad = (bend.angle_xy * Math.PI) / 180;
    const angleZRad = (bend.angle_z * Math.PI) / 180;
    torus.rotation.x = angleZRad;
    torus.rotation.z = angleXYRad;

    this.tubeGroup.add(torus);
  }

  /**
   * Добавление метки гибки
   */
  private addBendLabel(position: Point3D, index: number): void {
    // Создание спрайта с текстом (упрощённо - просто точка)
    const geometry = new THREE.SphereGeometry(5, 16, 16);
    const material = new THREE.MeshBasicMaterial({ color: 0xff6b6b });
    const sphere = new THREE.Mesh(geometry, material);
    sphere.position.set(position.x, position.y, position.z);
    this.tubeGroup.add(sphere);

    // Можно добавить canvas текстуру для текста, но это усложнит код
    // Для простоты оставляем просто цветную сферу
  }

  /**
   * Добавление маркера точки (начало/конец)
   */
  private addPointMarker(position: Point3D, label: string, color: number): void {
    // Создание большей сферы для маркера
    const geometry = new THREE.SphereGeometry(8, 16, 16);
    const material = new THREE.MeshBasicMaterial({ color: color });
    const sphere = new THREE.Mesh(geometry, material);
    sphere.position.set(position.x, position.y, position.z);
    this.tubeGroup.add(sphere);
  }

  /**
   * Добавление осей координат
   */
  private addCoordinateAxes(): void {
    // X, Y, Z оси
    const axesHelper = new THREE.AxesHelper(100);
    this.tubeGroup.add(axesHelper);
  }

  /**
   * Включение контроля мышью
   */
  private enableMouseControls(): void {
    let isDragging = false;
    let previousMousePosition = { x: 0, y: 0 };

    this.renderer.domElement.addEventListener('mousedown', (e) => {
      isDragging = true;
      previousMousePosition = { x: e.clientX, y: e.clientY };
    });

    this.renderer.domElement.addEventListener('mousemove', (e) => {
      if (isDragging) {
        const deltaX = e.clientX - previousMousePosition.x;
        const deltaY = e.clientY - previousMousePosition.y;

        // Вращение объекта вокруг центра
        this.tubeGroup.rotation.y += deltaX * 0.01;
        this.tubeGroup.rotation.x += deltaY * 0.01;

        previousMousePosition = { x: e.clientX, y: e.clientY };
      }
    });

    this.renderer.domElement.addEventListener('mouseup', () => {
      isDragging = false;
    });

    this.renderer.domElement.addEventListener('mouseleave', () => {
      isDragging = false;
    });

    // Масштабирование колёсиком мыши
    this.renderer.domElement.addEventListener('wheel', (e) => {
      e.preventDefault();
      const delta = e.deltaY > 0 ? 1.1 : 0.9;
      this.camera.position.multiplyScalar(delta);
    });
  }

  /**
   * Основной цикл анимации
   */
  private animate = (): void => {
    this.animationFrameId = requestAnimationFrame(this.animate);
    this.renderer.render(this.scene, this.camera);
  };

  /**
   * Обработка изменения размера окна
   */
  private onWindowResize = (): void => {
    const width = this.container.clientWidth || 800;
    const height = this.container.clientHeight || 600;

    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  };

  /**
   * Очистка ресурсов
   */
  public dispose(): void {
    if (this.animationFrameId !== null) {
      cancelAnimationFrame(this.animationFrameId);
    }
    this.renderer.dispose();
    if (this.container.contains(this.renderer.domElement)) {
      this.container.removeChild(this.renderer.domElement);
    }
  }

  /**
   * Установка камеры на определённую позицию
   */
  public setCamera(position: Point3D): void {
    this.camera.position.set(position.x, position.y, position.z);
    this.camera.lookAt(0, 0, 0);
  }

  /**
   * Сброс вида (вернуть камеру на начальную позицию)
   */
  public resetView(): void {
    this.camera.position.set(200, 200, 150);
    this.camera.lookAt(0, 0, 0);
    this.tubeGroup.rotation.set(0, 0, 0);
  }

  /**
   * Включение/отключение сетки координат
   */
  public setGridVisible(visible: boolean): void {
    this.gridHelper.visible = visible;
  }

  /**
   * Включение wireframe режима для отладки
   */
  public setWireframeMode(enabled: boolean): void {
    this.tubeGroup.traverse((child) => {
      if (child instanceof THREE.Mesh && child.material instanceof THREE.Material) {
        (child.material as any).wireframe = enabled;
      }
    });
  }
}
