#pragma once
#include "bnCardAction.h"
#include "bnAnimation.h"
#include <SFML/Graphics.hpp>

class SpriteProxyNode;
class Character;
class YoYoCardAction : public CardAction {
private:
  SpriteProxyNode* attachment;
  Animation attachmentAnim;
  Entity* yoyo;
  int damage;
public:
  YoYoCardAction(Character* owner, int damage);
  ~YoYoCardAction();
  void OnUpdate(float _elapsed);
  void EndAction();
  void Execute();
};