#include "bnMetalManBossFight.h"
#include "bnMetalMan.h"
#include "bnBattleItem.h"
#include "bnStringEncoder.h"
#include "bnCard.h"
#include "bnField.h"
#include "bnTile.h"
#include "bnGear.h"
#include "bnFadeInState.h"
#include "bnUndernetBackground.h"

Mob* MetalManBossFight::Build(Field* field) {
  Mob* mob = new Mob(field);
  mob->SetBackground(std::make_shared<UndernetBackground>());
  mob->StreamCustomMusic("resources/loops/loop_boss_battle.ogg");

  //mob->RegisterRankedReward(1, BattleItem(Battle::Card(100, 139, 'Y', 0, Element::none, "YoYo", "", "", 0)));
  //mob->RegisterRankedReward(4, BattleItem(Battle::Card(100, 139, '*', 0, Element::none, "YoYo", "", "", 0)));

  field->AddEntity(std::make_shared<Gear>(Team::blue, Direction::left), 3, 2);
  field->AddEntity(std::make_shared<Gear>(Team::blue, Direction::right), 4, 2);

  auto spawner = mob->CreateSpawner<MetalMan>();
  spawner.SpawnAt<FadeInState>(6, 2);

  mob->ToggleBossFlag();

  for (int i = 0; i < field->GetWidth(); i++) {
    for (int j = 0; j < field->GetHeight(); j++) {
      Battle::Tile* tile = field->GetAt(i + 1, j + 1);
    }
  }

  return mob;
}
