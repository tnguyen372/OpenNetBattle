#pragma once
#include "bnAIState.h"

class AlphaCore;
class AlphaArm;

class AlphaClawSwipeState : public AIState<AlphaCore>
{
private:
  std::shared_ptr<AlphaArm> leftArm{ nullptr }, rightArm{nullptr};

  bool goldenArmState;
  Battle::Tile* last;

  void SpawnLeftArm(AlphaCore&);
  void SpawnRightArm(AlphaCore&);
public:
  AlphaClawSwipeState(bool goldenArmState = false);
  ~AlphaClawSwipeState();

  void OnEnter(AlphaCore& a) override;
  void OnUpdate(double _elapsed, AlphaCore& a) override;
  void OnLeave(AlphaCore& a) override;
};
