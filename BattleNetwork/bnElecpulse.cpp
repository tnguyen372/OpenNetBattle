#include <random>
#include <time.h>

#include "bnElecpulse.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnSharedHitbox.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"

Elecpulse::Elecpulse(Team _team, int _damage) : Spell(_team) {
  SetLayer(0);
  SetPassthrough(true);
  SetElement(Element::elec);
  setScale(2.f, 2.f);
  progress = 0.0f;

  damage = _damage;

  setTexture(Textures().GetTexture(TextureType::SPELL_ELEC_PULSE));

  animation = CreateComponent<AnimationComponent>(weak_from_this());
  animation->SetPath("resources/spells/elecpulse.animation");
  animation->Reload();

  auto onFinish = [this]() { Remove(); };

  animation->SetAnimation("PULSE", onFinish);
}

Elecpulse::~Elecpulse(void) {
}

void Elecpulse::OnSpawn(Battle::Tile & start)
{
  int step = 1;

  if (GetTeam() == Team::blue) {
    step = -1;
    
    // TODO: remove when engine auto-flips sprites
    sf::Vector2f s = getScale();
    setScale(-s.x, s.y);
  }

  auto& tile = *GetTile();
  auto top = tile.Offset(step, -1);
  auto forward = tile.Offset(step, 0);
  auto bottom = tile.Offset(step, 1);

  auto spellPtr = shared_from_base<Spell>();

  if (forward) {
    auto shared = std::make_shared<SharedHitbox>(spellPtr);
    field->AddEntity(shared, *forward);
  }

  if (top) {
    auto shared = std::make_shared<SharedHitbox>(spellPtr);
    field->AddEntity(shared, *top);
  }

  if (bottom) {
    auto shared = std::make_shared<SharedHitbox>(spellPtr);
    field->AddEntity(shared, *bottom);
  }

  Entity::drawOffset = sf::Vector2f(70.0f * step, -60.0f);
}

void Elecpulse::OnUpdate(double _elapsed) {
  int step = 1;
  if (GetTeam() == Team::blue) {
    step = -1;
  }

  if (hasHitbox) {
    GetTile()->AffectEntities(*this);

    auto& tile = *GetTile();
    auto top = tile.Offset(step, -1);
    auto forward = tile.Offset(step, 0);
    auto bottom = tile.Offset(step, 1);

    auto flashMode = Battle::Tile::Highlight::solid;
    tile.RequestHighlight(flashMode);
    top ? top->RequestHighlight(flashMode) : (void)0;
    forward ? forward->RequestHighlight(flashMode) : (void)0;
    bottom ? bottom->RequestHighlight(flashMode) : (void)0;
  }
}

void Elecpulse::OnDelete()
{
  hasHitbox = false;
}

void Elecpulse::Attack(std::shared_ptr<Character> _entity) {
  long ID = _entity->GetID();

  if (std::find(taggedCharacters.begin(), taggedCharacters.end(), ID) != taggedCharacters.end())
    return; // we've already hit this entity somewhere

  Hit::Properties props;
  props.element = GetElement();
  props.flags = Hit::flinch | Hit::stun | Hit::drag;
  props.drag = { (GetTeam() == Team::red) ? Direction::left : Direction::right, 1u };
  props.damage = damage;

  _entity->Hit(props);

  taggedCharacters.insert(taggedCharacters.begin(), _entity->GetID());
}

