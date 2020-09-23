#pragma once

#include <glm/glm.hpp>

#include <array>
#include <vector>

namespace pulcher::physics {

  enum class IntersectorType : size_t {
    Point = 0x10000000
  , Ray   = 0x20000000
  };

  struct IntersectorPoint {
    static IntersectorType constexpr type = IntersectorType::Point;

    // inputs
    glm::u32vec2 origin;
  };

  struct IntersectorRay {
    static IntersectorType constexpr type = IntersectorType::Ray;

    // inputs
    glm::u32vec2 origin;
    glm::vec2 direction;
    float maxDistance = -1.0f; // -1.0f indicates no max distance
  };

  struct IntersectionResults {
    bool collision = false;
    glm::u32vec2 origin = glm::u32vec2(0);
  };

  struct Queries {
    size_t AddQuery(IntersectorPoint const & intersector);
    size_t AddQuery(IntersectorRay const & intersector);

    pulcher::physics::IntersectionResults RetrieveQuery(size_t);

    std::vector<pulcher::physics::IntersectorPoint> intersectorPoints;
    std::vector<pulcher::physics::IntersectorRay>   intersectorRays;

    std::vector<pulcher::physics::IntersectionResults> intersectorResultsPoints;
    std::vector<pulcher::physics::IntersectionResults> intersectorResultsRays;

    // Swaps buffers around
    void Submit();
  };

}