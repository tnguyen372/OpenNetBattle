#include "bnButton.h"

#include <math.h>

Button::Button(const std::string& labelStr) :
  Widget()
{
  label = std::make_shared<Text>(Font::Style::thick);
  AddNode(label);
  SetLabel(labelStr);

  img = std::make_shared<SpriteProxyNode>();
  AddNode(img);

  btnLayout = new ButtonLayout(this);
}

Button::~Button()
{
  delete btnLayout;
}

Button::ButtonLayout::ButtonLayout(Button* btn) : 
  btn(btn),
  Widget::Layout(btn)
{
}

const sf::FloatRect Button::ButtonLayout::CalculateBounds() const
{
  auto& btn = *this->btn;
  float distx = std::fabs(btn.label->getPosition().x - btn.img->getPosition().x);
  float disty = std::fabs(btn.label->getPosition().y - btn.img->getPosition().y);
  float minLeft   = std::min(btn.label->GetLocalBounds().left, btn.img->getLocalBounds().left);
  float minTop    = std::min(btn.label->GetLocalBounds().top, btn.img->getLocalBounds().top);
  float maxWidth  = distx + std::max(btn.label->GetLocalBounds().width, btn.img->getLocalBounds().width);
  float maxHeight = disty + std::max(btn.label->GetLocalBounds().height, btn.img->getLocalBounds().height);

  return sf::FloatRect(minLeft, minTop, maxWidth, maxHeight);
}

void Button::SetLabel(const std::string& labelStr)
{
  this->label->SetString(labelStr);
  img->setPosition(this->label->GetLocalBounds().width * 0.5f, this->label->GetFont().GetLetterHeight());
}

void Button::SetImage(const std::string& path)
{
  ResourceHandle handle;
  img->setTexture(handle.Textures().LoadTextureFromFile(path));
}

void Button::ClearImage()
{
  img->setTexture(std::make_shared<sf::Texture>(), true);
}
