#include <random>
#include <time.h>

#include "bnRollHeal.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnPlayer.h"
#include "bnCharacter.h"
#include "bnObstacle.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnHitbox.h"

#define RESOURCE_PATH "resources/spells/spell_roll.animation"

RollHeal::RollHeal(Team team, std::shared_ptr<Character> user, int _heal) : 
  Spell(team), 
  user(user)
{
  SetPassthrough(true);

  random = rand() % 20 - 20;

  heal = _heal;

  int lr = (team == Team::red) ? 1 : -1;
  setScale(2.0f*lr, 2.0f);

  Audio().Play(AudioType::APPEAR);

  setTexture(Textures().LoadTextureFromFile("resources/spells/spell_roll.png"), true);

  animationComponent = CreateComponent<AnimationComponent>(weak_from_this());
  animationComponent->SetPath(RESOURCE_PATH);
  animationComponent->Reload();

  auto props = Hit::DefaultProperties;
  props.damage = heal;
  props.flags |= Hit::flinch;
  SetHitboxProperties(props);

  /**
   * This is very convoluted and will change with the card summon refactored
   * Essentially we nest callbacks
   *
   * First Roll is IDLE. when the animation ends, we set the animation to MOVE
   *
   * While roll is moving, we find the first enemy in the field.
   * We set our target named `attack`
   *
   * After MOVE is over, we set the animation to ATTACKING
   *
   * If we found a target, we add 3 callbacks to frames 4, 12, and 20
   * to deal damage to the enemy
   *
   * At the animation end, we set the final animation to MOVE
   *
   * At the end of the last MOVE animation, we spawn a heart
   * and request the summon system to remove this entity
   */
  animationComponent->SetAnimation("ROLL_IDLE", [this] {
    animationComponent->SetAnimation("ROLL_MOVE", [this] {

      bool found = false;

      Battle::Tile* attack = nullptr;

      auto allTiles = GetField()->FindTiles([](Battle::Tile* tile) { return true; });
      auto iter = allTiles.begin();

      while (iter != allTiles.end()) {
        Battle::Tile* next = (*iter);

        if (!found) {
          if (next->ContainsEntityType<Character>() && !next->ContainsEntityType<Obstacle>() && next->GetTeam() != GetTeam()) {
            GetTile()->RemoveEntityByID(GetID());

            Battle::Tile* prev = GetField()->GetAt(next->GetX() - 1, next->GetY());
            AdoptTile(prev);

            attack = next;

            found = true;
          }
        }

        iter++;
      }

      if (found) {
        animationComponent->SetAnimation("ROLL_ATTACKING", [this] {
          animationComponent->SetAnimation("ROLL_MOVE", [this] {
            Delete();
          });
        });

        if (attack) {
          animationComponent->AddCallback(4,  [this, attack]() { DropHitbox(attack); }, true);
          animationComponent->AddCallback(12, [this, attack]() { DropHitbox(attack); }, true);
          animationComponent->AddCallback(20, [this, attack]() { DropHitbox(attack); }, true);
        }
      }
      else { //no enemies, just heal the player
        animationComponent->SetAnimation("ROLL_MOVE", [this] {
          Delete();
        });
      }
    });
  });
}

RollHeal::~RollHeal() {
}

void RollHeal::OnUpdate(double _elapsed) { }

void RollHeal::Attack(std::shared_ptr<Character> _entity) {
    // Old code went here
}

void RollHeal::OnDelete()
{
  Remove();
}

void RollHeal::DropHitbox(Battle::Tile* target)
{
    auto hitbox = std::make_shared<Hitbox>(GetTeam());
    hitbox->HighlightTile(Battle::Tile::Highlight::solid);
    hitbox->SetHitboxProperties(GetHitboxProperties());
    hitbox->AddCallback([](std::shared_ptr<Character> hit) {
        ResourceHandle().Audio().Play(AudioType::HURT);
    });
    GetField()->AddEntity(hitbox, *target);
}
