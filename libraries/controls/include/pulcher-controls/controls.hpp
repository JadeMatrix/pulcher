#pragma once

#include <cstdint>

#include <glm/fwd.hpp>

struct GLFWwindow;

namespace pulcher::controls {

  struct Controller {
    enum struct Movement : int8_t {
      Left = -1, Right = +1,
      Up = -1, Down = +1,
      None = 0ul, Size = 3ul
    };

    struct Frame {
      Movement
        movementHorizontal = Movement::None
      , movementVertical = Movement::None
      ;

      bool jump = false, dash = false, crouch = false;

      glm::vec2 lookDirection = { 0.0f, 0.0f };
    };

    Frame current, previous;
  };

  void UpdateControls(
    GLFWwindow * window
  , uint32_t displayWidth, uint32_t displayHeight
  , pulcher::controls::Controller & controller
  );

}