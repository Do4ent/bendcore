# Сервис тела трубы (bendcore_server): HTTP на порту 8080, см. src/server/main.cpp.
# Сборка (из корня репозитория), OCCT — установка в ~/occt-install (см. .github/workflows/wasm.yml):
#   docker build --build-context occt="$HOME/occt-install" -f docker/Dockerfile -t bendcore-server .
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends cmake ninja-build g++ libeigen3-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
COPY --from=occt . /opt/occt
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY tests tests
COPY examples examples
COPY third_party third_party
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBENDCORE_WITH_OCCT=ON \
      -DBENDCORE_OCCT_DIR=/opt/occt -DBENDCORE_EIGEN_DIR=/usr/include/eigen3 \
    && cmake --build /build --target bendcore_server --parallel

# Рантайм: OCCT слинкован статически, нужны только системные библиотеки стандартного рантайма.
FROM ubuntu:24.04
COPY --from=build /build/bendcore_server /usr/local/bin/bendcore_server
USER nobody
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/bendcore_server"]
CMD ["8080"]
