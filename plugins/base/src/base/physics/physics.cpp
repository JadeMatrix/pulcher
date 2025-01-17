#include <pulcher-core/map.hpp>
#include <pulcher-core/player.hpp> // hitbox
#include <pulcher-core/scene-bundle.hpp>
#include <pulcher-gfx/context.hpp>
#include <pulcher-gfx/image.hpp>
#include <pulcher-gfx/imgui.hpp>
#include <pulcher-physics/intersections.hpp>
#include <pulcher-physics/tileset.hpp>
#include <pulcher-plugin/plugin.hpp>
#include <pulcher-util/enum.hpp>
#include <pulcher-util/log.hpp>
#include <pulcher-util/math.hpp>

#include <entt/entt.hpp>
#include <glad/glad.hpp>
#include <imgui/imgui.hpp>

#include <span>
#include <vector>

namespace {

bool showPhysicsQueries = false;
bool showHitboxes = false;

struct DebugRenderInfo {
  sg_buffer bufferOrigin;
  sg_buffer bufferCollision;
  sg_bindings bindings;
  sg_pipeline pipeline;
  sg_shader program;
};


constexpr size_t debugRenderMaxPoints = 1'000;
constexpr size_t debugRenderMaxRays = 1'000;
DebugRenderInfo debugRenderPoint = {};
DebugRenderInfo debugRenderRay = {};

void LoadSokolInfoRay() {
  { // -- origin buffer
    sg_buffer_desc desc = {};
    desc.size = debugRenderMaxRays * sizeof(float) * 4;
    desc.usage = SG_USAGE_STREAM;
    desc.content = nullptr;
    desc.label = "debug-render-info-ray-origin-buffer";
    debugRenderRay.bufferOrigin = sg_make_buffer(&desc);
  }

  { // -- collision buffer
    sg_buffer_desc desc = {};
    desc.size = debugRenderMaxRays * sizeof(float);
    desc.usage = SG_USAGE_STREAM;
    desc.content = nullptr;
    desc.label = "debug-render-info-ray-collision-buffer";
    debugRenderRay.bufferCollision = sg_make_buffer(&desc);
  }

  // bindings
  debugRenderRay.bindings.vertex_buffers[0] = debugRenderRay.bufferOrigin;
  debugRenderRay.bindings.vertex_buffers[1] = debugRenderRay.bufferCollision;

  { // -- shader
    sg_shader_desc desc = {};
    desc.vs.uniform_blocks[0].size = sizeof(float) * 2;
    desc.vs.uniform_blocks[0].uniforms[0].name = "originOffset";
    desc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;

    desc.vs.uniform_blocks[1].size = sizeof(float) * 2;
    desc.vs.uniform_blocks[1].uniforms[0].name = "framebufferResolution";
    desc.vs.uniform_blocks[1].uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;

    desc.vs.source = PUL_SHADER(
      layout(location = 0) in vec2 inOrigin;
      layout(location = 1) in float inCollision;

      uniform vec2 originOffset;
      uniform vec2 framebufferResolution;

      flat out int inoutCollision;

      void main() {
        vec2 framebufferScale = vec2(2.0f) / framebufferResolution;
        vec2 vertexOrigin = (inOrigin)*vec2(1,-1) * framebufferScale;
        vertexOrigin += originOffset*vec2(-1, 1) * framebufferScale;
        gl_Position = vec4(vertexOrigin.xy, 0.0f, 1.0f);
        inoutCollision = int(inCollision > 0.0f);
      }
    );

    desc.fs.source = PUL_SHADER(
      flat in int inoutCollision;

      out vec4 outColor;

      void main() {
        outColor =
            inoutCollision > 0
          ? vec4(1.0f, 0.7f, 0.7f, 1.0f) : vec4(0.7f, 1.0f, 0.7f, 1.0f)
        ;
      }
    );

    debugRenderRay.program = sg_make_shader(&desc);
  }

  { // -- pipeline
    sg_pipeline_desc desc = {};

    desc.layout.buffers[0].stride = 0u;
    desc.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;
    desc.layout.attrs[0].buffer_index = 0;
    desc.layout.attrs[0].offset = 0;
    desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;

    desc.layout.buffers[1].stride = 0u;
    desc.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_VERTEX;
    desc.layout.attrs[1].buffer_index = 1;
    desc.layout.attrs[1].offset = 0;
    desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT;

    desc.primitive_type = SG_PRIMITIVETYPE_LINES;
    desc.index_type = SG_INDEXTYPE_NONE;

    desc.shader = debugRenderRay.program;
    desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    desc.depth_stencil.depth_write_enabled = true;

    desc.blend.enabled = false;

    desc.rasterizer.alpha_to_coverage_enabled = false;
    desc.rasterizer.face_winding = SG_FACEWINDING_CCW;
    desc.rasterizer.sample_count = 1;

    desc.label = "debug-render-ray-pipeline";

    debugRenderRay.pipeline = sg_make_pipeline(&desc);
  }
}

void LoadSokolInfoPoint() {
  { // -- origin buffer
    sg_buffer_desc desc = {};
    desc.size = debugRenderMaxPoints * sizeof(float) * 2;
    desc.usage = SG_USAGE_STREAM;
    desc.content = nullptr;
    desc.label = "debug-render-info-point-origin-buffer";
    debugRenderPoint.bufferOrigin = sg_make_buffer(&desc);
  }

  { // -- collision buffer
    sg_buffer_desc desc = {};
    desc.size = debugRenderMaxPoints * sizeof(float);
    desc.usage = SG_USAGE_STREAM;
    desc.content = nullptr;
    desc.label = "debug-render-info-point-collision-buffer";
    debugRenderPoint.bufferCollision = sg_make_buffer(&desc);
  }

  // bindings
  debugRenderPoint.bindings.vertex_buffers[0] = debugRenderPoint.bufferOrigin;
  debugRenderPoint.bindings.vertex_buffers[1] =
    debugRenderPoint.bufferCollision;

  { // -- shader
    sg_shader_desc desc = {};
    desc.vs.uniform_blocks[0].size = sizeof(float) * 2;
    desc.vs.uniform_blocks[0].uniforms[0].name = "originOffset";
    desc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;

    desc.vs.uniform_blocks[1].size = sizeof(float) * 2;
    desc.vs.uniform_blocks[1].uniforms[0].name = "framebufferResolution";
    desc.vs.uniform_blocks[1].uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;

    desc.vs.source = PUL_SHADER(
      layout(location = 0) in vec2 inOrigin;
      layout(location = 1) in float inCollision;

      uniform vec2 originOffset;
      uniform vec2 framebufferResolution;

      flat out int inoutCollision;

      void main() {
        vec2 framebufferScale = vec2(2.0f) / framebufferResolution;
        vec2 vertexOrigin = (inOrigin)*vec2(1,-1) * framebufferScale;
        vertexOrigin += originOffset*vec2(-1, 1) * framebufferScale;
        gl_Position = vec4(vertexOrigin.xy, 0.0f, 1.0f);
        inoutCollision = int(inCollision > 0.0f);
      }
    );

    desc.fs.source = PUL_SHADER(
      flat in int inoutCollision;

      out vec4 outColor;

      void main() {
        outColor =
            inoutCollision > 0
          ? vec4(1.0f, 0.7f, 0.7f, 1.0f) : vec4(0.7f, 1.0f, 0.7f, 1.0f)
        ;
      }
    );

    debugRenderPoint.program = sg_make_shader(&desc);
  }

  { // -- pipeline
    sg_pipeline_desc desc = {};

    desc.layout.buffers[0].stride = 0u;
    desc.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;
    desc.layout.attrs[0].buffer_index = 0;
    desc.layout.attrs[0].offset = 0;
    desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;

    desc.layout.buffers[1].stride = 0u;
    desc.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_VERTEX;
    desc.layout.attrs[1].buffer_index = 1;
    desc.layout.attrs[1].offset = 0;
    desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT;

    desc.primitive_type = SG_PRIMITIVETYPE_POINTS;
    desc.index_type = SG_INDEXTYPE_NONE;

    desc.shader = debugRenderPoint.program;
    desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    desc.depth_stencil.depth_write_enabled = true;

    desc.blend.enabled = false;

    desc.rasterizer.cull_mode = SG_CULLMODE_BACK;
    desc.rasterizer.alpha_to_coverage_enabled = false;
    desc.rasterizer.face_winding = SG_FACEWINDING_CCW;
    desc.rasterizer.sample_count = 1;

    desc.label = "debug-render-point-pipeline";

    debugRenderPoint.pipeline = sg_make_pipeline(&desc);
  }
}

void LoadSokolInfo() {
  ::LoadSokolInfoPoint();
  ::LoadSokolInfoRay();
}

// basically, when doings physics, we want tile lookups to be cached / quick,
// and we only want to do one tile intersection test per tile-grid. In other
// words, while there may be multiple tilesets contributing to the
// collision layer, there is still only one collision layer

pul::physics::TilemapLayer tilemapLayer;

float CalculateSdfDistance(
  pul::physics::TilemapLayer::TileInfo const & tileInfo
, glm::u32vec2 texel
) {
  if (tileInfo.tilesetIdx == -1ul) { return 0.0f; }

  auto const * tileset = tilemapLayer.tilesets[tileInfo.tilesetIdx];

  if (!tileInfo.Valid()) { return 0.0f; }

  pul::physics::Tile const & physicsTile =
    tileset->tiles[tileInfo.imageTileIdx];

  // apply tile orientation
  auto const tileOrientation = Idx(tileInfo.orientation);

  if (tileOrientation & Idx(pul::core::TileOrientation::FlipHorizontal))
    { texel.x = 31 - texel.x; }

  if (tileOrientation & Idx(pul::core::TileOrientation::FlipVertical))
    { texel.y = 31 - texel.y; }

  if (tileOrientation & Idx(pul::core::TileOrientation::FlipDiagonal)) {
    std::swap(texel.x, texel.y);
  }

  // -- compute intersection SDF and accel hints
  return physicsTile.signedDistanceField[texel.x][texel.y];
}

glm::vec2 GetAabbMin(glm::vec2 const & aabbOrigin, glm::vec2 const & aabbDim) {
  glm::vec2 const p0 = aabbOrigin - aabbDim/2.0f;
  glm::vec2 const p1 = aabbOrigin + aabbDim/2.0f;
  return glm::min(p0, p1);
}

glm::vec2 GetAabbMax(glm::vec2 const & aabbOrigin, glm::vec2 const & aabbDim) {
  glm::vec2 const p0 = aabbOrigin - aabbDim/2.0f;
  glm::vec2 const p1 = aabbOrigin + aabbDim/2.0f;
  return glm::max(p0, p1);
}

bool IntersectionRayAabb(
  glm::vec2 const & rayBegin, glm::vec2 const & rayEnd
, glm::vec2 const & aabbOrigin, glm::vec2 const & aabbDim
, float & intersectionLength
) {
  glm::vec2 normal = glm::normalize(rayEnd - rayBegin);
  normal.x = normal.x == 0.0f ? 1.0f : 1.0f / normal.x;
  normal.y = normal.y == 0.0f ? 1.0f : 1.0f / normal.y;

  glm::vec2 const min = (GetAabbMin(aabbOrigin, aabbDim) - rayBegin) * normal;
  glm::vec2 const max = (GetAabbMax(aabbOrigin, aabbDim) - rayBegin) * normal;

  float tmin = glm::max(glm::min(min.x, max.x), glm::min(min.y, max.y));
  float tmax = glm::min(glm::max(min.x, max.x), glm::max(min.y, max.y));

  if (tmax < 0.0f || tmin > tmax)
    { return false; }

  float t = tmin < 0.0f ? tmax : tmin;
  intersectionLength = t;
  return t > 0.0f && t < glm::length(rayEnd - rayBegin);
}

bool IntersectionCircleAabb(
  glm::vec2 const & circleOrigin, float const circleRadius
, glm::vec2 const & aabbOrigin, glm::vec2 const & aabbDim
, glm::vec2 & closestOrigin
) {
  closestOrigin =
    glm::clamp(
      circleOrigin
    , GetAabbMin(aabbOrigin, aabbDim)
    , GetAabbMax(aabbOrigin, aabbDim)
    );

  return glm::length(circleOrigin - closestOrigin) <= circleRadius;
}

} // -- namespace

// -- plugin functions
extern "C" {

PUL_PLUGIN_DECL void Physics_EntityIntersectionRaycast(
  pul::core::SceneBundle & scene
, pul::physics::IntersectorRay const & ray
, pul::physics::EntityIntersectionResults & intersectionResults
) {
  auto & registry = scene.EnttRegistry();

  auto view =
    registry.view<
      pul::core::ComponentHitboxAABB
    , pul::core::ComponentOrigin
    >();

  intersectionResults.entities.clear();

  glm::vec2 const
    rayOriginBegin = glm::vec2(ray.beginOrigin)
  , rayOriginEnd = glm::vec2(ray.endOrigin)
  ;

  for (auto & entity : view) {
    auto const & hitbox = view.get<pul::core::ComponentHitboxAABB>(entity);
    auto const & origin = view.get<pul::core::ComponentOrigin>(entity);

    float intersectionLength;
    bool const intersection =
      ::IntersectionRayAabb(
        rayOriginBegin, rayOriginEnd
      , origin.origin, glm::vec2(hitbox.dimensions)
      , intersectionLength
      );

    if (intersection) {
      glm::vec2 const intersectionOrigin =
          rayOriginBegin
        + intersectionLength * glm::normalize(rayOriginEnd - rayOriginBegin)
      ;

      intersectionResults.collision = true;
      std::pair<glm::i32vec2, entt::entity> results;
      std::get<0>(results) = glm::i32vec2(glm::round(intersectionOrigin));
      std::get<1>(results) = entity;
      intersectionResults.entities.emplace_back(results);
    }
  }
}

PUL_PLUGIN_DECL void Physics_EntityIntersectionCircle(
  pul::core::SceneBundle & scene
, pul::physics::IntersectorCircle const & circle
, pul::physics::EntityIntersectionResults & intersectionResults
) {
  auto & registry = scene.EnttRegistry();

  auto view =
    registry.view<
      pul::core::ComponentHitboxAABB
    , pul::core::ComponentOrigin
    >();

  intersectionResults.entities.clear();

  for (auto & entity : view) {
    auto const & hitbox = view.get<pul::core::ComponentHitboxAABB>(entity);
    auto const & origin = view.get<pul::core::ComponentOrigin>(entity);

    glm::vec2 closestOrigin;
    bool intersection =
      ::IntersectionCircleAabb(
        glm::vec2(circle.origin), circle.radius
      , origin.origin, glm::vec2(hitbox.dimensions)
      , closestOrigin
      );

    if (intersection) {
      intersectionResults.collision = true;
      std::pair<glm::i32vec2, entt::entity> results;
      std::get<0>(results) = glm::i32vec2(glm::round(closestOrigin));
      std::get<1>(results) = entity;
      intersectionResults.entities.emplace_back(results);
    }
  }
}

PUL_PLUGIN_DECL void Physics_ProcessTileset(
  pul::physics::Tileset & tileset
, pul::gfx::Image const & image
) {
  tileset = {};
  tileset.tiles.reserve((image.width / 32ul) * (image.height / 32ul));

  // iterate thru every tile
  for (size_t tileY = 0ul; tileY < image.height / 32ul; ++ tileY)
  for (size_t tileX = 0ul; tileX < image.width  / 32ul; ++ tileX) {
    pul::physics::Tile tile;

    auto constexpr gridSize = pul::physics::Tile::gridSize;

    // iterate thru every texel of the tile, stratified by physics tile GridSize
    for (size_t texelY = 0ul; texelY < 32ul; texelY += 32ul/gridSize)
    for (size_t texelX = 0ul; texelX < 32ul; texelX += 32ul/gridSize) {

      size_t const
        imageTexelX = tileX*32ul + texelX
      , imageTexelY = tileY*32ul + texelY
      , gridTexelX = static_cast<size_t>(texelX*(gridSize/32.0f))
      , gridTexelY = static_cast<size_t>(texelY*(gridSize/32.0f))
      ;

      float const alpha =
        image.data[(image.height-imageTexelY-1)*image.width + imageTexelX].a;

      tile.signedDistanceField[gridTexelX][gridTexelY] = alpha;
    }

    tileset.tiles.emplace_back(tile);
  }
}

PUL_PLUGIN_DECL void Physics_ClearMapGeometry() {
  sg_destroy_buffer(::debugRenderPoint.bufferOrigin);
  sg_destroy_buffer(::debugRenderRay  .bufferOrigin);

  sg_destroy_buffer(::debugRenderPoint.bufferCollision);
  sg_destroy_buffer(::debugRenderRay  .bufferCollision);

  sg_destroy_pipeline(::debugRenderPoint.pipeline);
  sg_destroy_pipeline(::debugRenderRay  .pipeline);

  ::debugRenderPoint.bindings = {};
  ::debugRenderRay  .bindings = {};

  sg_destroy_shader(::debugRenderPoint.program);
  sg_destroy_shader(::debugRenderRay  .program);

  tilemapLayer = {};
}

PUL_PLUGIN_DECL void Physics_LoadMapGeometry(
  std::vector<pul::physics::Tileset const *> const & tilesets
, std::vector<std::span<size_t>>             const & mapTileIndices
, std::vector<std::span<glm::u32vec2>>       const & mapTileOrigins
, std::vector<std::span<pul::core::TileOrientation>> const & mapTileOrientations
) {
  Physics_ClearMapGeometry();

  // -- assert tilesets.size == mapTileIndices.size == mapTileOrigins.size
  if (tilesets.size() != mapTileOrigins.size()) {
    spdlog::critical("mismatching size on tilesets & map tile origins");
    return;
  }

  if (mapTileIndices.size() != mapTileOrigins.size()) {
    spdlog::critical("mismatching size on map tile indices/origins");
    return;
  }

  // -- compute max width/height of tilemap
  uint32_t width = 0ul, height = 0ul;
  for (auto & tileOrigins : mapTileOrigins)
  for (auto & origin : tileOrigins) {
    width = std::max(width, origin.x+1);
    height = std::max(height, origin.y+1);
  }
  ::tilemapLayer.width = width;

  // copy tilesets over
  ::tilemapLayer.tilesets =
    decltype(::tilemapLayer.tilesets){tilesets.begin(), tilesets.end()};

  // resize
  ::tilemapLayer.tileInfo.resize(width * height);

  // cache tileset info for quick tile fetching
  for (size_t tilesetIdx = 0ul; tilesetIdx < tilesets.size(); ++ tilesetIdx) {
    auto const & tileIndices = mapTileIndices[tilesetIdx];
    auto const & tileOrigins = mapTileOrigins[tilesetIdx];
    auto const & tileOrientations = mapTileOrientations[tilesetIdx];

    PUL_ASSERT(tilesets[tilesetIdx], continue;);

    for (size_t i = 0ul; i < tileIndices.size(); ++ i) {
      auto const & imageTileIdx    = tileIndices[i];
      auto const & tileOrigin      = tileOrigins[i];
      auto const & tileOrientation = tileOrientations[i];

      PUL_ASSERT_CMP(
        imageTileIdx, <, tilesets[tilesetIdx]->tiles.size(), continue;
      );

      size_t const tileIdx = tileOrigin.y * width + tileOrigin.x;

      PUL_ASSERT_CMP(tileIdx, <, ::tilemapLayer.tileInfo.size(), continue;);

      auto & tile = ::tilemapLayer.tileInfo[tileIdx];
      if (tile.imageTileIdx != -1ul) {
        spdlog::error("multiple tiles are intersecting on the collision layer");
        continue;
      }

      tile.tilesetIdx   = tilesetIdx;
      tile.imageTileIdx = imageTileIdx;
      tile.origin       = tileOrigin;
      tile.orientation  = tileOrientation;
    }
  }

  ::LoadSokolInfo();
}

PUL_PLUGIN_DECL bool Physics_InverseSceneIntersectionRaycast(
  pul::core::SceneBundle & scene
, pul::physics::IntersectorRay const & ray
, pul::physics::IntersectionResults & intersectionResults
) {
  intersectionResults = {};
  // TODO this is slow and can be optimized by using SDFs
  pul::physics::BresenhamLine(
    ray.beginOrigin, ray.endOrigin
  , [&](int32_t x, int32_t y) {
      if (intersectionResults.collision) { return; }
      auto origin = glm::i32vec2(x, y);
      // -- get physics tile from acceleration structure

      // calculate tile indices, not for the spritesheet but for the tile in
      // the physx map
      size_t tileIdx;
      glm::u32vec2 texelOrigin;
      if (
        !pul::util::CalculateTileIndices(
          tileIdx, texelOrigin, origin
        , ::tilemapLayer.width, ::tilemapLayer.tileInfo.size()
        )
      ) {
        return;
      }

      PUL_ASSERT_CMP(::tilemapLayer.tileInfo.size(), >, tileIdx, return;);
      auto const & tileInfo = ::tilemapLayer.tileInfo[tileIdx];

      if (::CalculateSdfDistance(tileInfo, texelOrigin) == 0.0f) {
        intersectionResults =
          pul::physics::IntersectionResults {
            true, origin, tileInfo.imageTileIdx, tileInfo.tilesetIdx
          };
      }
    }
  );

  auto & queries = scene.PhysicsDebugQueries();
  queries.Add(ray, intersectionResults);

  return intersectionResults.collision;
}

PUL_PLUGIN_DECL bool Physics_IntersectionRaycast(
  pul::core::SceneBundle & scene
, pul::physics::IntersectorRay const & ray
, pul::physics::IntersectionResults & intersectionResults
) {
  intersectionResults = {};
  // TODO this is slow and can be optimized by using SDFs
  pul::physics::BresenhamLine(
    ray.beginOrigin, ray.endOrigin
  , [&](int32_t x, int32_t y) {
      if (intersectionResults.collision) { return; }
      auto origin = glm::i32vec2(x, y);
      // -- get physics tile from acceleration structure

      // calculate tile indices, not for the spritesheet but for the tile in
      // the physx map
      size_t tileIdx;
      glm::u32vec2 texelOrigin;
      if (
        !pul::util::CalculateTileIndices(
          tileIdx, texelOrigin, origin
        , ::tilemapLayer.width, ::tilemapLayer.tileInfo.size()
        )
      ) {
        return;
      }

      PUL_ASSERT_CMP(::tilemapLayer.tileInfo.size(), >, tileIdx, return;);
      auto const & tileInfo = ::tilemapLayer.tileInfo[tileIdx];

      if (::CalculateSdfDistance(tileInfo, texelOrigin) > 0.0f) {
        intersectionResults =
          pul::physics::IntersectionResults {
            true, origin, tileInfo.imageTileIdx, tileInfo.tilesetIdx
          };
      }
    }
  );

  auto & queries = scene.PhysicsDebugQueries();
  queries.Add(ray, intersectionResults);

  return intersectionResults.collision;
}

PUL_PLUGIN_DECL pul::physics::TilemapLayer * Physics_TilemapLayer() {
  return &tilemapLayer;
}

PUL_PLUGIN_DECL bool Physics_IntersectionAabb(
  pul::core::SceneBundle &
, pul::physics::IntersectorAabb const &
, pul::physics::IntersectionResults &
) {
  return false;
}

PUL_PLUGIN_DECL bool Physics_IntersectionPoint(
  pul::core::SceneBundle & scene
, pul::physics::IntersectorPoint const & point
, pul::physics::IntersectionResults & intersectionResults
) {
  intersectionResults = {};

  auto & queries = scene.PhysicsDebugQueries();

  // -- get physics tile from acceleration structure
  size_t tileIdx;
  glm::u32vec2 texelOrigin;
  if (
    !pul::util::CalculateTileIndices(
      tileIdx, texelOrigin, point.origin
    , ::tilemapLayer.width, ::tilemapLayer.tileInfo.size()
    )
  ) {
    queries.Add(point, {});
    return false;
  }

  PUL_ASSERT_CMP(::tilemapLayer.tileInfo.size(), >, tileIdx, return false;);
  auto const & tileInfo = ::tilemapLayer.tileInfo[tileIdx];

  if (::CalculateSdfDistance(tileInfo, texelOrigin) > 0.0f) {
    intersectionResults =
      pul::physics::IntersectionResults {
        true, point.origin, tileInfo.imageTileIdx, tileInfo.tilesetIdx
      };

    queries.Add(point, intersectionResults);
    return true;
  }

  queries.Add(point, {});
  return false;
}

PUL_PLUGIN_DECL void Physics_RenderDebug(pul::core::SceneBundle & scene) {
  auto & queries = scene.PhysicsDebugQueries();
  auto & registry = scene.EnttRegistry();

  if (::showPhysicsQueries && queries.intersectorPoints.size() > 0ul) {
    { // -- update buffers
      std::vector<glm::vec2> points;
      std::vector<float> collisions;
      points.reserve(queries.intersectorPoints.size());
      // update queries
      for (auto & query : queries.intersectorPoints) {
        points.emplace_back(std::get<0>(query).origin);
        collisions
          .emplace_back(static_cast<float>(std::get<1>(query).collision));
      }

      sg_update_buffer(
        debugRenderPoint.bufferOrigin
      , points.data(), points.size() * sizeof(glm::vec2)
      );

      sg_update_buffer(
        debugRenderPoint.bufferCollision
      , collisions.data(), collisions.size() * sizeof(float)
      );
    }

    // apply pipeline and render
    sg_apply_pipeline(debugRenderPoint.pipeline);
    sg_apply_bindings(&debugRenderPoint.bindings);
    glm::vec2 cameraOrigin = scene.cameraOrigin;

    sg_apply_uniforms(
      SG_SHADERSTAGE_VS
    , 0
    , &cameraOrigin.x
    , sizeof(float) * 2ul
    );

    sg_apply_uniforms(
      SG_SHADERSTAGE_VS
    , 1
    , &scene.config.framebufferDimFloat.x
    , sizeof(float) * 2ul
    );

    glPointSize(2);
    sg_draw(0, queries.intersectorPoints.size(), 1);
  }


  bool const showQueries = ::showHitboxes || ::showPhysicsQueries;

  if (showQueries && queries.intersectorRays.size() > 0ul) {
    size_t drawCount = 0ul;
    { // -- update buffers
      std::vector<glm::vec2> lines;
      std::vector<float> collisions;
      lines.reserve(queries.intersectorRays.size()*2);
      // update queries
      if (::showPhysicsQueries) {
        for (auto & query : queries.intersectorRays) {
          auto & queryRay = std::get<0>(query);
          auto & queryResult = std::get<1>(query);
          bool collision = queryResult.collision;
          lines.emplace_back(queryRay.beginOrigin);
          lines.emplace_back(
            collision ? queryResult.origin : queryRay.endOrigin
          );
          collisions.emplace_back(static_cast<float>(collision));
          collisions.emplace_back(static_cast<float>(collision));
        }
      }

      if (::showHitboxes) {
        // update hitboxes
        auto view =
          registry.view<
            pul::core::ComponentHitboxAABB, pul::core::ComponentOrigin
          >();

        for (auto & entity : view) {
          // get origin/dimensions, for dimensions multiply by half in order to
          // get its "radius" or whatever
          auto const & dim =
            glm::vec2(
              view.get<pul::core::ComponentHitboxAABB>(entity).dimensions
            ) * 0.5f
          ;
          auto const & origin =
            view.get<pul::core::ComponentOrigin>(entity).origin;

          auto const * damageable =
            registry.try_get<pul::core::ComponentDamageable>(entity)
          ;

          bool const hasCollision =
            damageable && !damageable->frameDamageInfos.empty();

          // top
          lines.emplace_back(origin + glm::vec2(-dim.x, -dim.y));
          lines.emplace_back(origin + glm::vec2(+dim.x, -dim.y));
          collisions.emplace_back(hasCollision);
          collisions.emplace_back(hasCollision);

          // bottom
          lines.emplace_back(origin + glm::vec2(-dim.x, +dim.y));
          lines.emplace_back(origin + glm::vec2(+dim.x, +dim.y));
          collisions.emplace_back(hasCollision);
          collisions.emplace_back(hasCollision);

          // left
          lines.emplace_back(origin + glm::vec2(-dim.x, -dim.y));
          lines.emplace_back(origin + glm::vec2(-dim.x, +dim.y));
          collisions.emplace_back(hasCollision);
          collisions.emplace_back(hasCollision);

          // right
          lines.emplace_back(origin + glm::vec2(+dim.x, -dim.y));
          lines.emplace_back(origin + glm::vec2(+dim.x, +dim.y));
          collisions.emplace_back(hasCollision);
          collisions.emplace_back(hasCollision);
        }
      }

      sg_update_buffer(
        debugRenderRay.bufferOrigin
      , lines.data(), lines.size() * sizeof(glm::vec2)
      );

      sg_update_buffer(
        debugRenderRay.bufferCollision
      , collisions.data(), collisions.size() * sizeof(float)
      );

      drawCount = lines.size();
    }

    // apply pipeline and render
    sg_apply_pipeline(debugRenderRay.pipeline);
    sg_apply_bindings(&debugRenderRay.bindings);
    glm::vec2 cameraOrigin = scene.cameraOrigin;

    sg_apply_uniforms(
      SG_SHADERSTAGE_VS
    , 0
    , &cameraOrigin.x
    , sizeof(float) * 2ul
    );

    sg_apply_uniforms(
      SG_SHADERSTAGE_VS
    , 1
    , &scene.config.framebufferDimFloat.x
    , sizeof(float) * 2ul
    );

    glLineWidth(1.0f);

    sg_draw(0, drawCount, 1);
  }
}

PUL_PLUGIN_DECL void Physics_UiRender(pul::core::SceneBundle &) {
  ImGui::Begin("Physics");

  pul::imgui::Text("tilemap width {}", ::tilemapLayer.width);
  pul::imgui::Text("tile info size {}", ::tilemapLayer.tileInfo.size());

  ImGui::Checkbox("show physics queries", &::showPhysicsQueries);
  ImGui::Checkbox("show hitboxes", &::showHitboxes);

  ImGui::End();
}

}
