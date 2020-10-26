#pragma once

#include <pulcher-core/config.hpp>
#include <pulcher-util/consts.hpp>
#include <pulcher-util/pimpl.hpp>

#include <glm/glm.hpp>

namespace entt { enum class entity : std::uint32_t; }
namespace entt { template <typename> class basic_registry; }
namespace entt { using registry = basic_registry<entity>; }
namespace pul::animation { struct System; }
namespace pul::audio { struct System; }
namespace pul::controls { struct Controller; }
namespace pul::physics { struct DebugQueries; }

namespace pul::core {
  struct SceneBundle {
    glm::i32vec2 cameraOrigin = {};

    float calculatedMsPerFrame = pul::util::MsPerFrame;
    size_t numCpuFrames = 0ul;

    pul::core::Config config = {};

    glm::u32vec2 playerCenter = {};

    pul::animation::System & AnimationSystem();
    pul::controls::Controller & PlayerController();
    pul::physics::DebugQueries & PhysicsDebugQueries();
    pul::audio::System & AudioSystem();

    entt::registry & EnttRegistry();

    struct Impl;
    pul::util::pimpl<Impl> impl;

    static SceneBundle Construct();
  };
}
