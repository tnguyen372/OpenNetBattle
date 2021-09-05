#include "bnNinjaStar.h"
#include "bnTile.h"

#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include <cmath>
#include <Swoosh/Ease.h>
#include <Swoosh/Game.h>

NinjaStar::NinjaStar(Team _team, float _duration) : duration(_duration), Spell(_team) {
  SetLayer(0);;
  
  setTexture(Textures().GetTexture(TextureType::SPELL_NINJA_STAR));
  
  animation = CreateComponent<AnimationComponent>(weak_from_this());
  animation->SetPath("resources/spells/ninja_star.animation");
  animation->Load();
  animation->SetAnimation("DEFAULT");
  
  setScale(2.f, 2.f);

  progress = 0.0f;

  // Blue team starts from the right side of the screen
  if (GetTeam() == Team::blue) {
    start = sf::Vector2f(480.0f, -480.0f);
  }
  // Red ream starts from the left side of the screen
  else if (GetTeam() == Team::red) {
    start = sf::Vector2f(0.0f, -480.0f);
  }
  // Otherwise, unsupported
  else {
    Delete();
  }

  Audio().Play(AudioType::TOSS_ITEM_LITE);

  auto props = Hit::DefaultProperties;
  
  // Do 100 units of impact damage
  props.damage = 100;
  props.flags |= Hit::impact;
  SetHitboxProperties(props);
}

NinjaStar::~NinjaStar() {
}

void NinjaStar::OnUpdate(double _elapsed) {
  float beta = swoosh::ease::linear(progress, duration, 1.0);

  // interpolate from top of screen to the target tile spot
  float posX = (1.0f - beta)*start.x;
  float posY = (1.0f - beta)*start.y;

  Entity::drawOffset = { posX, posY };

  // When at the end of the line
  if (progress >= duration) {
    // deal damage
    tile->AffectEntities(*this);

    if (!changed) {
      changed = true;

      auto onFinish = [this]() {
        Delete();
      };

      animation->SetAnimation("GROUNDED", onFinish);
    }
  }
  
  progress += _elapsed;
}

void NinjaStar::Attack(std::shared_ptr<Character> _entity) {
    _entity->Hit(GetHitboxProperties());
}

void NinjaStar::OnDelete()
{
  Remove();
}
