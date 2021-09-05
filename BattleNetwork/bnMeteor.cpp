#include "bnMeteor.h"
#include "bnRingExplosion.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnField.h"
#include "bnTile.h"
#include <cmath>
#include <Swoosh/Ease.h>
#include <Swoosh/Game.h>

Meteor::Meteor(Team _team, int damage, float _duration) : 
  duration(_duration), 
  Spell(_team) {
  SetLayer(1);

  HighlightTile(Battle::Tile::Highlight::flash);

  setTexture(Textures().GetTexture(TextureType::SPELL_METEOR));

  setScale(0.f, 0.f);

  progress = 0.0f;

  if(target) {
    // Which direction to come down from
    if (GetTeam() == Team::blue) {
      start = sf::Vector2f(480, - 480.0f);
    }
    else if (GetTeam() == Team::red) {
      start = sf::Vector2f(0, - 480.0f);
    }
    else {
      Delete();
    }
  } else {
    Delete();
  }

  auto props = Hit::DefaultProperties;
  props.damage = damage;
  props.flags |= Hit::impact | Hit::flash | Hit::flinch;
  SetHitboxProperties(props);
}

Meteor::~Meteor() {
}

void Meteor::OnUpdate(double _elapsed) {
  if (GetTeam() == Team::blue) {
    setScale(-2.f, 2.f);
    swoosh::game::setOrigin(getSprite(), 1.0, 1.0);
  }
  else {
    setScale(2.f, 2.f);
    swoosh::game::setOrigin(getSprite(), 0.0, 1.0);
  }

  float beta = swoosh::ease::linear(progress, duration, 1.0);
  float posX = (beta * 0) + ((1.0f - beta) * start.x);
  float posY = (beta * 0) + ((1.0f - beta) * start.y);

  Entity::drawOffset = { posX, posY };

  // When at the end of the arc
  if (beta >= 1.0f) {
    // update tile to target tile
    tile->AffectEntities(*this);

    if (tile->GetState() != TileState::empty && tile->GetState() != TileState::broken) {
      EventChannel().Emit(&Camera::ShakeCamera, 5, sf::seconds(0.5));

      field->AddEntity(std::make_shared<RingExplosion>(), *GetTile());
    }

    Delete();
  }

  progress += _elapsed;
}

void Meteor::Attack(std::shared_ptr<Character> _entity) {
  _entity->Hit(GetHitboxProperties());
}

void Meteor::OnSpawn(Battle::Tile& start)
{
  Audio().Play(AudioType::METEOR, AudioPriority::high);
}

void Meteor::OnDelete()
{
  Remove();
}
