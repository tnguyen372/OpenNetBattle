#include "bnCardAction.h"
#include "bnAnimation.h"
#include "bnArtifact.h"
#include "bnSpriteProxyNode.h"

#include <vector>

class Character;
class Target;

class MachGunCardAction : public CardAction {
  int damage{};
  bool moveUp{ true }, firstSpawn{ true };
  SpriteProxyNode machgun;
  Animation machgunAnim;
  std::shared_ptr<Entity> target{ nullptr };
  Battle::Tile* targetTile{ nullptr };

  void FreeTarget();
  Battle::Tile* MoveRectical(Field*, bool columnMove);

public:
  MachGunCardAction(std::shared_ptr<Character> actor, int damage);
  ~MachGunCardAction();

  void OnExecute(std::shared_ptr<Character> user) override;
  void OnActionEnd() override;
  void OnAnimationEnd() override;
};

class Target : public Artifact {
  Animation anim;
  double attack{}; //!< in seconds
  int damage{};
public:
  Target(int damage);
  ~Target();

  void OnSpawn(Battle::Tile& start) override;
  void OnUpdate(double elapsed) override;
  void OnDelete() override;
};