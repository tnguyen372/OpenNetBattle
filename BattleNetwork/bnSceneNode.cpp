#include "bnSceneNode.h"

SceneNode::SceneNode() :
show(true), layer(0), parent(nullptr), childNodes() {
}

SceneNode::~SceneNode() {

}

void SceneNode::SetLayer(int layer) {
  SceneNode::layer = layer;
}

const int SceneNode::GetLayer() const {
  return layer;
}

void SceneNode::Hide() {
  show = false;
}

void SceneNode::Reveal() {
  show = true;
}

const bool SceneNode::IsHidden() const {
  return !show;
}

const bool SceneNode::IsVisible() const {
  return show;
}

void SceneNode::draw(sf::RenderTarget& target, sf::RenderStates states) const {
  if (!show) return;

  std::sort(childNodes.begin(), childNodes.end(), [](SceneNode* a, SceneNode* b) { return (a->GetLayer() > b->GetLayer()); });

  // draw its children
  for (std::size_t i = 0; i < childNodes.size(); i++) {
    auto childStates = states;

    if (!childNodes[i]->useParentShader) {
      childStates.shader = nullptr;
    }

    childNodes[i]->draw(target, childStates);
  }
}

void SceneNode::AddNode(SceneNode* child) { 
  if (child == nullptr) return;  child->parent = this; childNodes.push_back(child); 
}

void SceneNode::AddNode(std::shared_ptr<SceneNode> child) { 
  if (child == nullptr) return;
  ownedChildren.push_back(child);
  AddNode(child.get());
}

void SceneNode::RemoveNode(SceneNode* find) {
  if (find == nullptr) return;

  auto iter = std::remove_if(childNodes.begin(), childNodes.end(), [find](SceneNode *in) { return in == find; }); 
  auto ownedIter = std::remove_if(ownedChildren.begin(), ownedChildren.end(), [find](std::shared_ptr<SceneNode>& in) { return in.get() == find; });

  if (iter != childNodes.end()) {
    (*iter)->parent = nullptr;
  }

  childNodes.erase(iter, childNodes.end());
  ownedChildren.erase(ownedIter, ownedChildren.end());
}

void SceneNode::EnableParentShader(bool use)
{
  useParentShader = use;
}

const bool SceneNode::IsUsingParentShader() const
{
  return useParentShader;
}

std::vector<SceneNode*>& SceneNode::GetChildNodes() const
{
  return childNodes;
}

std::set<SceneNode*> SceneNode::GetChildNodesWithTag(const std::initializer_list<std::string>& query)
{
  std::set<SceneNode*> results;

  for (auto& q : query) {
    for (auto& n : childNodes) {
      if (n->HasTag(q)) {
        results.insert(n);
      }
    }
  }

  return results;
}

SceneNode* SceneNode::GetParent() {
  return parent;
}

void SceneNode::AddTags(const std::initializer_list<std::string> tags)
{
  for (auto& t : tags) {
    this->tags.insert(t);
  }
}

void SceneNode::RemoveTags(const std::initializer_list<std::string> tags)
{
  for (auto& t : tags) {
    this->tags.insert(t);
  }
}

const bool SceneNode::HasTag(const std::string& name)
{
  for (auto& t : tags) {
    if (t == name) return true;
  }

  return false;
}
