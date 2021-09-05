#include "bnBees.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnObstacle.h"
#include "bnParticleImpact.h"
#include "bnHitbox.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"

void Bees::MonitorTarget(Entity* target)
{
  if (target) {
    auto leaderDeleteHandler = [](auto target, auto observer) {
      Bees& bees = dynamic_cast<Bees&>(*observer);

      if (target.get() == bees.leader) bees.leader = nullptr;
      bees.target = nullptr;
    };

    field->DropNotifier(notifier);
    notifier = field->NotifyOnDelete(target->GetID(), this->GetID(), leaderDeleteHandler);
    this->target = target;
  }
}

Bees::Bees(Team _team,int damage) :
  damage(damage),
  Obstacle(_team) {
  SetName("Bees");
  SetHealth(1);
  SetLayer(0);
  SetHeight(42.f);
  ShareTileSpace(true);
  setTexture(Textures().GetTexture(TextureType::SPELL_BEES));
  setScale(2.f, 2.f);

  HighlightTile(Battle::Tile::Highlight::solid);

  animation = Animation("resources/spells/spell_bees.animation");
  animation.SetAnimation("DEFAULT");
  animation << Animator::Mode::Loop;

  attackCooldown = 0.60f;

  animation.Refresh(getSprite());

  shadow = new SpriteProxyNode();
  shadow->setTexture(LOAD_TEXTURE(MISC_SHADOW));
  shadow->SetLayer(1);
  shadow->setPosition(-6.0f, 12.0f);
  AddNode(shadow);

  auto props = GetHitboxProperties();

  // Bees do impact damage only
  props.flags = Hit::impact;

  // Thunder has electric properties
  props.element = Element::wood;

  // Attack does 5 units of damage
  props.damage = damage;

  SetHitboxProperties(props);

  if (GetTeam() == Team::red) {
    SetDirection(Direction::right);
  }
  else {
    SetDirection(Direction::left);
  }

  absorbDamage = std::make_shared<BeeDefenseRule>();
  AddDefenseRule(absorbDamage);

  flickerCooldown = frames(3 * 60);
}

Bees::Bees(const Bees & leader) :
  animation(leader.animation), 
  target(leader.target),
  leader(const_cast<Bees*>(&leader)),
  attackCooldown(leader.attackCooldown), 
  damage(leader.damage),
  madeContact(false),
  Obstacle(leader.GetTeam())
{
  SetName("Bees");
  SetHealth(1);
  SetLayer(0);
  SetHeight(leader.GetHeight());
  ShareTileSpace(true);

  setTexture(Textures().GetTexture(TextureType::SPELL_BEES));
  setScale(2.f, 2.f);

  HighlightTile(Battle::Tile::Highlight::solid);

  animation = Animation("resources/spells/spell_bees.animation");
  animation.SetAnimation("DEFAULT");
  animation << Animator::Mode::Loop;
  animation.Refresh(getSprite());

  shadow = new SpriteProxyNode();
  shadow->setTexture(LOAD_TEXTURE(MISC_SHADOW));
  shadow->SetLayer(1);
  shadow->setPosition(-6.0f, 12.0f);

  AddNode(shadow);

  SetHitboxProperties(leader.GetHitboxProperties());

  if (GetTeam() == Team::red) {
    SetDirection(Direction::right);
  }
  else {
    SetDirection(Direction::left);
  }

  absorbDamage = std::make_shared<BeeDefenseRule>();
  AddDefenseRule(absorbDamage);

  flickerCooldown = frames(3 * 60);
}

Bees::~Bees() {
  delete shadow;
}

void Bees::OnUpdate(double _elapsed) {
  elapsed += _elapsed;

  if (battleOver) {
    flickerCooldown -= from_seconds(_elapsed);

    if (flickerCooldown.count() % 3 == 0) {
      SetAlpha(0);
    }
    else {
      SetAlpha(255);
    }

    if (flickerCooldown <= frames(0)) {
      Remove();
      return;
    }
  }

  Entity::drawOffset.y = -GetHeight();

  animation.Update(_elapsed, getSprite());

  // Find target if we don't have one
  if (!leader && !target) {
    // Find all characters that are not on our team and not an obstacle
    auto query = [&](std::shared_ptr<Entity> e) {
      return (e->GetTeam() != team && dynamic_cast<Character*>(e.get()) && !dynamic_cast<Obstacle*>(e.get()));
    };

    auto list = field->FindEntities(query);

    for (auto l : list) {
      if (!target) { target = l.get(); }
      else {
        // If the distance to one enemy is shorter than the other, target the shortest enemy path
        int currentDist = abs(tile->GetX() - l->GetTile()->GetX()) + abs(tile->GetY() - l->GetTile()->GetY());
        int targetDist = abs(tile->GetX() - target->GetTile()->GetX()) + abs(tile->GetY() - target->GetTile()->GetY());

        if (currentDist < targetDist) {
          target = l.get();
        }
      }
    }

    MonitorTarget(target);
    Logger::Logf("Bee %d monitoring target %d", GetID(), target->GetID());
  }
  else if (leader && !target) {
    // Follow the leader
    target = leader;

    MonitorTarget(target);
    Logger::Logf("Bee %d following leader %d", GetID(), target->GetID());
  }

  // If sliding is flagged to false, we know we've ended a move
  auto direction = Direction::none;
  bool wasMovingVertical   = (GetDirection() == Direction::down || GetDirection() == Direction::up);
  bool wasMovingHorizontal = (GetDirection() == Direction::left || GetDirection() == Direction::right);
  bool skipMoveCode = false;

  if (target && target != this->leader) {
    Battle::Tile* targetTile = target->GetTile();
    if (targetTile) {
      if (this->madeContact) {
        if (this->IsMoving()) {
          this->FinishMove(); // flush movement if moving
        }

        this->Teleport(targetTile, ActionOrder::immediate);
        skipMoveCode = true;
      }
    }
  }

  if (!IsSliding() && !skipMoveCode) {
    if (target) {
      if (target->GetTile()) {
        if (this->madeContact) {
          this->FinishMove();
        }
        /*
          once a bee commits to travelling in a direction, they will not change directions again until 
          they enter the same column (if moving horizontally) or row (if moving vertically) as their target, 
          or if their target moves behind them
        */
        bool isbehind = false;
        Battle::Tile* targetTile = target->GetTile();
        Battle::Tile* tile = GetTile();

        if (tile->GetX() <= targetTile->GetX() && GetDirection() != Direction::right) {
          isbehind = true;
        }

        if (tile->GetX() >= targetTile->GetX() && GetDirection() != Direction::left) {
          isbehind = true;
        }

        if (tile->GetY() == targetTile->GetY()) {
          if (targetTile->GetX() < tile->GetX()) {
            direction = Direction::left;
          }
          else if (targetTile->GetX() > tile->GetX()) {
            direction = Direction::right;
          }
        }
        else {
          bool lastTurn = turnCount + 1 < 2;
          bool sameCol = tile->GetX() == targetTile->GetX();
          bool changeDir = lastTurn || sameCol;
          if (isbehind && changeDir) {
            if (targetTile->GetY() < tile->GetY()) {
              direction = Direction::up;
            }
            else if (targetTile->GetY() > tile->GetY()) {
              direction = Direction::down;
            }
          }
        }
      }
    }

    // only turn twice
    if (turnCount >= 2) {
      direction = Direction::none;
    }

    if (direction != GetDirection() && direction != Direction::none) {
      turnCount++;
      SetDirection(direction);
    }

    // stay on top of the real target, not the leader
    if (target && target->GetTile() == GetTile() && !Teammate(target->GetTeam()) && madeContact) {
      SetDirection(Direction::none);
      setPosition(target->getPosition() - sf::Vector2f{ 0, GetHeight() });
    }
    else {
      // Always slide to the tile we're moving to
      Slide(GetTile() + GetDirection(), frames(27), frames(0));

      // Did not move and update next tile pointer
      if (!IsMoving() && GetTile()->IsEdgeTile()) {
        Delete();
      }
    }
  }

  if (GetDirection() == Direction::left) {
    setScale(2, 2);
  }
  else if (GetDirection() == Direction::right) {
    setScale(-2, 2);
  }

  // Always affect the tile we're occupying
  GetTile()->AffectEntities(*this);

  if (target && GetTile() == target->GetTile() && attackCooldown == 0) {
    // Try to attack 5 times
    attackCooldown = 1.80f; // est 3 frames
    auto hitbox = std::make_shared<Hitbox>(GetTeam());
    hitbox->SetHitboxProperties(GetHitboxProperties());

    // all other hitbox events will be ignored after 5 hits
    if (hitCount < 5) {
      hitbox->AddCallback([this](std::shared_ptr<Character> entity) {
        this->madeContact = true; // we hit something!
        this->target = entity.get();
        this->hitCount++;
        Audio().Play(AudioType::HURT, AudioPriority::high);
        }, [](const std::shared_ptr<Character> entity) {
          auto fx = std::make_shared<ParticleImpact>(ParticleImpact::Type::green);
          fx->SetHeight(entity->GetHeight());
          entity->GetField()->AddEntity(fx, *entity->GetTile());
          fx->SetHeight(entity->GetHeight());
        });

    }

    if (GetField()->AddEntity(hitbox, *GetTile()) != Field::AddEntityStatus::deleted) {
      auto hitboxRemoveCallback = [](auto target, auto observer) {
        Hitbox& hitbox = dynamic_cast<Hitbox&>(*observer);
        hitbox.Remove();
      };

      field->NotifyOnDelete(this->GetID(), hitbox->GetID(), hitboxRemoveCallback);
    }
  }

  attackCooldown = std::max(attackCooldown - (float)elapsed, 0.0f);

  if(hitCount >= 5) {
    // Mark us for deletion
    Delete();
  }
}

void Bees::OnSpawn(Battle::Tile& start)
{
  MonitorTarget(this->leader);
}

void Bees::OnBattleStop()
{
  Character::OnBattleStop();
  battleOver = true;
}

bool Bees::CanMoveTo(Battle::Tile* tile) {
  return true;
}

void Bees::Attack(std::shared_ptr<Character> _entity) {
  // Bees doesn't directly attack, they drop 5 hitboxes
  // and we track that...

  // However, if Bee's attack an object, they are removed by end of frame
  if (dynamic_cast<Obstacle*>(_entity.get())) {
    this->Delete();
  }
}

void Bees::OnDelete()
{
  Remove();
}
