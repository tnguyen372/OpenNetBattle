#include "bnLightningCardAction.h"
#include "bnCardAction.h"
#include "bnSpriteProxyNode.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnHitbox.h"

#define FRAME1 { 1, 0.05 }
#define FRAME2 { 2, 0.05 }
#define FRAME3 { 3, 0.05 }

#define FRAMES FRAME1, FRAME2, FRAME3, FRAME3, FRAME3

#define LIGHTNING_IMG "resources/spells/spell_lightning.png"
#define LIGHTNING_ANI "resources/spells/spell_lightning.animation"

LightningCardAction::LightningCardAction(Character * owner, int damage) 
  : CardAction(owner, "PLAYER_SHOOTING", &attachment, "Buster") 
{
  LightningCardAction::damage = damage;

  attachment = new SpriteProxyNode();
  attachment->setTexture(owner->getTexture());
  attachment->SetLayer(-1);

  attachmentAnim = Animation(owner->GetFirstComponent<AnimationComponent>()->GetFilePath());
  attachmentAnim.Reload();
  attachmentAnim.SetAnimation("BUSTER");

  attack = new SpriteProxyNode();
  attack->setTexture(LOAD_TEXTURE_FILE(LIGHTNING_IMG));
  attack->SetLayer(-2);

  attackAnim = Animation(LIGHTNING_ANI);
  attackAnim.SetAnimation("DEFAULT");

  // add override anims
  OverrideAnimationFrames({ FRAMES });
}

void LightningCardAction::Execute() {
  auto owner = GetOwner();

  owner->AddNode(attachment);
  attachment->EnableParentShader(true);
  attachmentAnim.Update(0, attachment->getSprite());

  // On shoot frame, drop projectile
  auto onFire = [this]() -> void {
    AUDIO.Play(AudioType::SPREADER);

    attachment->AddNode(attack);
    attack->setPosition(attachmentAnim.GetPoint("endpoint"));

    attackAnim.Update(0, attack->getSprite());

    auto field = GetOwner()->GetField();
    auto team = GetOwner()->GetTeam();
    int col = GetOwner()->GetTile()->GetX();
    int row = GetOwner()->GetTile()->GetY();

    for (int i = 1; i < 5; i++) {
      auto hitbox = new Hitbox(field, team, LightningCardAction::damage);
      hitbox->HighlightTile(Battle::Tile::Highlight::solid);
      auto props = hitbox->GetHitboxProperties();
      props.aggressor = GetOwnerAs<Character>();
      props.damage = LightningCardAction::damage;
      hitbox->SetHitboxProperties(props);
      field->AddEntity(*hitbox, col + i, row);
    }

    AUDIO.Play(AudioType::THUNDER);
    this->fired = true;
  };

  AddAction(2, onFire);
}

LightningCardAction::~LightningCardAction()
{
}

void LightningCardAction::OnUpdate(float _elapsed)
{
  attachmentAnim.Update(_elapsed, attachment->getSprite());

  if (fired) {
    attackAnim.Update(_elapsed, attack->getSprite());
  }

  CardAction::OnUpdate(_elapsed);
}

void LightningCardAction::EndAction()
{
  attachment->RemoveNode(attack);
  delete attack;

  GetOwner()->RemoveNode(attachment);
  GetOwner()->FreeComponentByID(GetID());
  delete this;
}
