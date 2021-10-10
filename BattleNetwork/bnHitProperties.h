#pragma once
#include "bnElements.h"
#include "bnDirection.h"
#include "bnEntity.h"

namespace Hit {
  using Flags = uint32_t;

  // These are in order!
  // Hitboxes properties will be inserted in queue
  // based on priority (A < B) where the highest priority
  // is the lowest value
  const Flags none = 0x00000000;
  const Flags retangible = 0x00000001;
  const Flags freeze = 0x00000002;
  const Flags pierce = 0x00000004;
  const Flags flinch = 0x00000008;
  const Flags shake = 0x00000010;
  const Flags stun = 0x00000020;
  const Flags flash = 0x00000040;
  const Flags breaking = 0x00000080;
  const Flags impact = 0x00000100;
  const Flags drag = 0x00000200;
  const Flags bubble = 0x00000400;
  const Flags no_counter = 0x00000800;
  const Flags root = 0x00001000;


  struct Drag {
    Direction dir{ Direction::none };
    unsigned count{ 0 };
  };

  /**
   * @struct Properties
   * @author mav
   * @date 05/05/19
   * @brief Hit box information
   */
  struct Properties {
    int damage{};
    Flags flags{ Hit::none };
    Element element{ Element::none };
    Entity::ID_t aggressor{};
    Drag drag{ }; // Used by Hit::drag flag
  };

  const constexpr Hit::Properties DefaultProperties = { 
    0, 
    Flags(Hit::flinch | Hit::impact), 
    Element::none, 
    0, 
    Direction::none,
    true
  };
}