#include "bnDownloadScene.h"
#include "bnBufferReader.h"
#include "bnBufferWriter.h"
#include "../stx/string.h"
#include "../stx/zip_utils.h"
#include "../bnPlayer.h"
#include "../bindings/bnScriptedPlayer.h"
#include "../bnPackageManager.h"
#include "../bnPlayerPackageManager.h"
#include "../bnCardPackageManager.h"
#include "../bnBlockPackageManager.h"
#include "../bnLuaLibraryPackageManager.h"
#include "../bindings/bnScriptedCard.h"
#include "../bindings/bnScriptedBlock.h"
#include <Segues/PixelateBlackWashFade.h>

constexpr std::string_view CACHE_FOLDER = "cache";

// class DownloadScene
DownloadScene::DownloadScene(swoosh::ActivityController& ac, const DownloadSceneProps& props) : 
  downloadSuccess(props.downloadSuccess),
  coinFlip(props.coinFlip),
  lastScreen(props.lastScreen),
  playerHash(props.playerHash),
  remotePlayer(props.remotePlayer),
  remoteBlocks(props.remotePlayerBlocks),
  label(Font::Style::tiny),
  Scene(ac)
{
  playerCardPackageList = props.cardPackageHashes;
  playerBlockPackageList = props.blockPackageHashes;

  std::sort(playerCardPackageList.begin(), playerCardPackageList.end());
  playerCardPackageList.erase(std::unique(playerCardPackageList.begin(), playerCardPackageList.end()), playerCardPackageList.end());

  std::sort(playerBlockPackageList.begin(), playerBlockPackageList.end());
  playerBlockPackageList.erase(std::unique(playerBlockPackageList.begin(), playerBlockPackageList.end()), playerBlockPackageList.end());

  downloadSuccess = false; 

  packetProcessor = props.packetProcessor;
  packetProcessor->SetKickCallback([this] {
    static bool once = false;

    if (!once) {
      Logger::Logf(LogLevel::info, "Kicked for silence!");
      once = true;
    }

    this->Abort();
  });

  packetProcessor->EnableKickForSilence(true);

  packetProcessor->SetPacketBodyCallback([this](NetPlaySignals header, const Poco::Buffer<char>& body) {
    this->ProcessPacketBody(header, body);
  });

  blur.setPower(40);
  blur.setTexture(&lastScreen);

  bg = sf::Sprite(lastScreen);
  bg.setColor(sf::Color(255, 255, 255, 200));

  setView(sf::Vector2u(480, 320));

  std::filesystem::create_directories(CACHE_FOLDER);

  ResetRemotePartitions();
}

DownloadScene::~DownloadScene()
{

}

void DownloadScene::SendHandshakeAck()
{
  Poco::Buffer<char> buffer(0);
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::download_handshake);
  
  mySeed = getController().GetRandSeed();
  writer.Write(buffer, mySeed);

  auto id = packetProcessor->SendPacket(Reliability::Reliable, buffer).second;
  packetProcessor->UpdateHandshakeID(id);
}

void DownloadScene::SendCoinFlip(bool completed) {
  Poco::Buffer<char> buffer(0);
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::coin_flip);

  coinFlipComplete = completed;

  if (!coinFlipComplete) {
    coinFlip = rand() % 2;
  }

  writer.Write(buffer, coinFlip);
  writer.Write(buffer, coinFlipComplete);

  Logger::Logf(LogLevel::debug, "Coin flip was %i with seed %i and [coinflip complete=%i]", coinFlip, getController().GetRandSeed(), coinFlipComplete);

  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void DownloadScene::ResetRemotePartitions()
{
  CardPackagePartitioner& cardPartitioner = getController().CardPackagePartitioner();
  cardPartitioner.CreateNamespace(Game::RemotePartition);
  cardPartitioner.GetPartition(Game::RemotePartition).ErasePackages();

  PlayerPackagePartitioner& playerPartitioner = getController().PlayerPackagePartitioner();
  playerPartitioner.CreateNamespace(Game::RemotePartition);
  playerPartitioner.GetPartition(Game::RemotePartition).ErasePackages();

  BlockPackagePartitioner& blockPartitioner = getController().BlockPackagePartitioner();
  blockPartitioner.CreateNamespace(Game::RemotePartition);
  blockPartitioner.GetPartition(Game::RemotePartition).ErasePackages();

  LuaLibraryPackagePartitioner& libPartitioner = getController().LuaLibraryPackagePartitioner();
  libPartitioner.CreateNamespace(Game::RemotePartition);
  libPartitioner.GetPartition(Game::RemotePartition).ErasePackages();
}

CardPackageManager& DownloadScene::RemoteCardPartition()
{
  CardPackagePartitioner& partitioner = getController().CardPackagePartitioner();
  return partitioner.GetPartition(Game::RemotePartition);
}

CardPackageManager& DownloadScene::LocalCardPartition()
{
  return getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);
}

BlockPackageManager& DownloadScene::RemoteBlockPartition()
{
  return getController().BlockPackagePartitioner().GetPartition(Game::RemotePartition);
}

BlockPackageManager& DownloadScene::LocalBlockPartition()
{
  return getController().BlockPackagePartitioner().GetPartition(Game::LocalPartition);
}

PlayerPackageManager& DownloadScene::RemotePlayerPartition()
{
  return getController().PlayerPackagePartitioner().GetPartition(Game::RemotePartition);
}

PlayerPackageManager& DownloadScene::LocalPlayerPartition()
{
  return getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);
}

void DownloadScene::RemoveFromDownloadList(const std::string& id)
{
  auto iter = contentToDownload.find(id);
  if (iter != contentToDownload.end()) {
    contentToDownload.erase(iter);
  }
}

bool DownloadScene::AllTasksComplete()
{
  return coinFlipComplete && remoteCoinFlipComplete && cardPackageRequested && playerPackageRequested && blockPackageRequested && contentToDownload.empty();
}

void DownloadScene::TradePlayerPackageData(const PackageHash& hash)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write<NetPlaySignals>(buffer, NetPlaySignals::trade_player_package);
  writer.WriteTerminatedString(buffer, hash.packageId);
  writer.WriteTerminatedString(buffer, hash.md5);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void DownloadScene::TradeCardPackageData(const std::vector<PackageHash>& hashes)
{
  // Upload card list to remote
  packetProcessor->SendPacket(Reliability::Reliable, SerializeListOfHashes(NetPlaySignals::trade_card_package_list, hashes));
}

void DownloadScene::TradeBlockPackageData(const std::vector<PackageHash>& hashes)
{
  // Upload card list to remote
  packetProcessor->SendPacket(Reliability::Reliable, SerializeListOfHashes(NetPlaySignals::trade_block_package_list, hashes));
}

void DownloadScene::RequestPlayerPackageData(const std::string& packageId)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write<NetPlaySignals>(buffer, NetPlaySignals::player_package_request);
  writer.WriteTerminatedString(buffer, packageId);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void DownloadScene::RequestCardPackageList(const std::vector<std::string>& packageIds)
{
  for (const std::string& pid : packageIds) {
    BufferWriter writer;
    Poco::Buffer<char> buffer{ 0 };
    writer.Write<NetPlaySignals>(buffer, NetPlaySignals::card_package_request);
    writer.WriteTerminatedString(buffer, pid);
    packetProcessor->SendPacket(Reliability::Reliable, buffer);
  }
}

void DownloadScene::RequestBlockPackageList(const std::vector<std::string>& packageIds)
{
  for (const std::string& pid : packageIds) {
    BufferWriter writer;
    Poco::Buffer<char> buffer{ 0 };
    writer.Write<NetPlaySignals>(buffer, NetPlaySignals::block_package_request);
    writer.WriteTerminatedString(buffer, pid);
    packetProcessor->SendPacket(Reliability::Reliable, buffer);
  }
}

void DownloadScene::SendDownloadComplete(bool value)
{
  if (!downloadFlagSet) {
    downloadSuccess = value;
    downloadFlagSet = true;

    Poco::Buffer<char> buffer{ 0 };
    NetPlaySignals type{ NetPlaySignals::downloads_complete };
    buffer.append((char*)&type, sizeof(NetPlaySignals));
    buffer.append((char*)&downloadSuccess, sizeof(bool));

    packetProcessor->SendPacket(Reliability::Reliable, buffer);
    Abort();
  }
}

void DownloadScene::SendPing()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::ping };
  buffer.append((char*)&type, sizeof(NetPlaySignals));

  packetProcessor->SendPacket(Reliability::Unreliable, buffer);
}

void DownloadScene::ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body)
{
  switch (header) {
  case NetPlaySignals::download_handshake:
    Logger::Logf(LogLevel::info, "Remote is sending initial handshake");
    this->RecieveHandshake(body);
    break;
  case NetPlaySignals::coin_flip:
    Logger::Logf(LogLevel::info, "Remote is sending a coin flip");
    this->RecieveCoinFlip(body);
    break;
  case NetPlaySignals::trade_card_package_list:
    Logger::Logf(LogLevel::info, "Remote is requesting to compare the card packages...");
    this->RecieveTradeCardPackageData(body);
    break;
  case NetPlaySignals::trade_block_package_list:
    Logger::Logf(LogLevel::info, "Remote is requesting to compare the block packages...");
    this->RecieveTradeBlockPackageData(body);
    break;
  case NetPlaySignals::trade_player_package:
    Logger::Logf(LogLevel::info, "Remote is requesting to compare the player packages...");
    this->RecieveTradePlayerPackageData(body);
    break;
  case NetPlaySignals::player_package_request:
    Logger::Logf(LogLevel::info, "Remote is requesting to download the player package...");
    this->RecieveRequestPlayerPackageData(body);
    break;
  case NetPlaySignals::card_package_request:
    Logger::Logf(LogLevel::info, "Remote is requesting to download a card package...");
    this->RecieveRequestCardPackageData(body);
    break;
  case NetPlaySignals::block_package_request:
    Logger::Logf(LogLevel::info, "Remote is requesting to download a block package...");
    this->RecieveRequestBlockPackageData(body);
    break;
  case NetPlaySignals::card_package_download:
    Logger::Logf(LogLevel::info, "Downloading card package...");
    this->DownloadPackageData<CardPackageManager, ScriptedCard>(body, RemoteCardPartition());
    break;
  case NetPlaySignals::block_package_download:
    Logger::Logf(LogLevel::info, "Downloading block package...");
    this->DownloadPackageData<BlockPackageManager, ScriptedBlock>(body, RemoteBlockPartition());
    break;
  case NetPlaySignals::player_package_download:
    Logger::Logf(LogLevel::info, "Downloading player package...");
    this->DownloadPlayerData(body);
    break;
  case NetPlaySignals::downloads_complete:
    this->RecieveDownloadComplete(body);
    break;
  }
}

void DownloadScene::RecieveTradeCardPackageData(const Poco::Buffer<char>& buffer)
{
  std::vector<PackageHash> packageCardList = DeserializeListOfHashes(buffer);
  std::vector<std::string> requestList;
  CardPackageManager& packageManager = LocalCardPartition();
  for (PackageHash& remotePackage : packageCardList) {
    auto& [packageId, md5] = remotePackage;

    bool needsDownload = (packageManager.HasPackage(packageId) && DifferentHash(packageManager, packageId, md5));
    needsDownload = needsDownload || !packageManager.HasPackage(packageId);

    if (needsDownload) {
      requestList.push_back(packageId);
      contentToDownload[packageId] = "Downloading";
    }
  }

  Logger::Logf(LogLevel::info, "Recieved remote's card package list size: %d", packageCardList.size());

  // move to the next state
  if (requestList.size()) {
    Logger::Logf(LogLevel::info, "Need to download %d card packages", requestList.size());
    RequestCardPackageList(requestList);
  }
  else {
    Logger::Logf(LogLevel::info, "Nothing to download.");
  }
  cardPackageRequested = true;
}

void DownloadScene::RecieveTradeBlockPackageData(const Poco::Buffer<char>& buffer)
{
  std::vector<PackageHash> packageBlockList = DeserializeListOfHashes(buffer);
  std::vector<std::string> requestList;
  BlockPackageManager& packageManager = LocalBlockPartition();
  for (PackageHash& remotePackage : packageBlockList) {
    auto& [packageId, md5] = remotePackage;

    bool needsDownload = (packageManager.HasPackage(packageId) && DifferentHash(packageManager, packageId, md5));
    needsDownload = needsDownload || !packageManager.HasPackage(packageId);

    if (needsDownload) {
      requestList.push_back(packageId);
      contentToDownload[packageId] = "Downloading";
      remoteBlocks.push_back(PackageAddress{ Game::RemotePartition, packageId });
    }
    else {
      remoteBlocks.push_back(PackageAddress{ Game::LocalPartition, packageId });
    }
  }

  Logger::Logf(LogLevel::info, "Recieved remote's block package list size: %d", packageBlockList.size());

  // move to the next state
  if (requestList.size()) {
    Logger::Logf(LogLevel::info, "Need to download %d block packages", requestList.size());
    RequestBlockPackageList(requestList);
  }
  else {
    Logger::Logf(LogLevel::info, "Nothing to download.");
  }
  blockPackageRequested = true;
}

void DownloadScene::RecieveHandshake(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  unsigned int seed = reader.Read<unsigned int>(buffer);
  maxSeed = std::max(seed, mySeed);

  // kick off coin flip
  this->SendCoinFlip(false);

  // mark handshake as completed
  this->remoteHandshake = true;
}

void DownloadScene::RecieveTradePlayerPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);
  std::string md5 = reader.ReadTerminatedString(buffer);

  PlayerPackageManager& packageManager = LocalPlayerPartition();
  bool needsDownload = (packageManager.HasPackage(packageId) && DifferentHash(packageManager, packageId, md5));
  needsDownload = needsDownload || !packageManager.HasPackage(packageId);

  if (needsDownload) {
    contentToDownload[packageId] = "Downloading";
    // This will download the player data and also set the remotePlayerHash when completed successfully
    RequestPlayerPackageData(packageId);
    remotePlayer = PackageAddress{ Game::RemotePartition, packageId };
  }
  else {
    remotePlayer = PackageAddress{ Game::LocalPartition, packageId };
  }

  playerPackageRequested = true;
}

void DownloadScene::RecieveRequestPlayerPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (packageId.size()) {
    Logger::Logf(LogLevel::info, "Recieved download request for player hash %s", packageId.c_str());
    packetProcessor->SendPacket(Reliability::BigData,
      SerializePackageData(
        packageId,
        NetPlaySignals::player_package_download,
        LocalPlayerPartition()
      )
    );
  }
}

void DownloadScene::RecieveRequestCardPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (!packageId.empty()) {
    Logger::Logf(LogLevel::info, "Recieved download request for %s card package", packageId.c_str());

    packetProcessor->SendPacket(Reliability::BigData,
      SerializePackageData<CardPackageManager>(
        packageId,
        NetPlaySignals::card_package_download,
        LocalCardPartition()
      )
    );
  }
}

void DownloadScene::RecieveRequestBlockPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (!packageId.empty()) {
    Logger::Logf(LogLevel::info, "Recieved download request for %s block package", packageId.c_str());

    packetProcessor->SendPacket(Reliability::BigData,
      SerializePackageData<BlockPackageManager>(
        packageId,
        NetPlaySignals::block_package_download,
        LocalBlockPartition()
        )
    );
  }
}

void DownloadScene::RecieveDownloadComplete(const Poco::Buffer<char>& buffer)
{
  bool result{};
  std::memcpy(&result, buffer.begin(), sizeof(bool));

  if (result) {
    remoteSuccess = true;
  }
  else {
    // downloadSuccess ref will tell matchmaking scene we failed
    Abort();
  }

  Logger::Logf(LogLevel::info, "Remote says download complete. Result: %s", result ? "Success" : "Fail");
}

void DownloadScene::RecieveCoinFlip(const Poco::Buffer<char>& buffer)
{
  unsigned int result{};
  std::memcpy(&result, buffer.begin(), sizeof(unsigned int));
  std::memcpy(&remoteCoinFlipComplete, buffer.begin() + sizeof(unsigned int), sizeof(bool));

  // We can't both have the same result
  if (result == coinFlip) {
    // revert seed, diverge, and try to come to an agreement about order
    getController().SeedRand(mySeed++);
    SendCoinFlip(false);
    return;
  }

  // sync seed
  getController().SeedRand(maxSeed); 
  SendCoinFlip(true);
}

void DownloadScene::DownloadPlayerData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (packageId.empty()) return;
  RemoveFromDownloadList(packageId);

  size_t file_len = reader.Read<size_t>(buffer);
  std::string path = "cache/" + stx::rand_alphanum(12) + ".zip";

  std::fstream file;
  file.open(path, std::ios::out | std::ios::binary);

  stx::result_t<std::string> result(std::nullptr_t{}, "Unset");

  if (file.is_open()) {
    while (file_len > 0) {
      char byte = reader.Read<char>(buffer);
      file << byte;
      file_len--;
    }

    file.close();

    result = RemotePlayerPartition().LoadPackageFromZip<ScriptedPlayer>(path);
  }
  
  if (result.is_error()) {
    Logger::Logf(LogLevel::critical, "Failed to download custom navi with package ID %s: %s", packageId.c_str(), result.error_cstr());

    // There was a problem creating the file
    SendDownloadComplete(false);
  }
}

std::vector<PackageHash> DownloadScene::DeserializeListOfHashes(const Poco::Buffer<char>& buffer)
{
  size_t len{};
  size_t read{};
  std::vector<PackageHash> list;

  // list length
  std::memcpy(&len, buffer.begin() + read, sizeof(size_t));
  read += sizeof(size_t);

  while (len > 0) {
    // package id
    size_t id_len{};
    std::memcpy(&id_len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);

    std::string id = std::string(buffer.begin() + read, id_len);
    read += id_len;

    // md5
    size_t md5_len{};
    std::memcpy(&md5_len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);

    std::string md5 = std::string(buffer.begin() + read, md5_len);
    read += md5_len;

    list.push_back({ id, md5 });

    len--;
  }

  return list;
}

Poco::Buffer<char> DownloadScene::SerializeListOfHashes(NetPlaySignals header, const std::vector<PackageHash>& list)
{
  Poco::Buffer<char> data{ 0 };

  // header
  data.append((char*)&header, sizeof(NetPlaySignals));

  // list length
  size_t len = list.size();
  data.append((char*)&len, sizeof(size_t));

  for(const PackageHash& hash : list) {
    // package id
    size_t sz = hash.packageId.length();
    data.append((char*)&sz, sizeof(size_t));
    data.append(hash.packageId.c_str(), hash.packageId.length());

    // md5
    sz = hash.md5.length();
    data.append((char*)&sz, sizeof(size_t));
    data.append(hash.md5.c_str(), hash.md5.length());
  }

  return data;
}

template<template<typename> class PackageManagerType, class MetaType>
bool DownloadScene::DifferentHash(PackageManagerType<MetaType>& packageManager, const std::string& packageId, const std::string& desiredFingerprint)
{
  MetaType& package = packageManager.FindPackageByID(packageId);
  return package.GetPackageFingerprint() != desiredFingerprint;
}

template<typename PackageManagerType, typename ScriptedDataType>
void DownloadScene::DownloadPackageData(const Poco::Buffer<char>& buffer, PackageManagerType& pm)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (packageId.empty()) return;
  RemoveFromDownloadList(packageId);

  size_t file_len = reader.Read<size_t>(buffer);
  std::string path = "cache/" + stx::rand_alphanum(12) + ".zip";

  std::fstream file;
  file.open(path, std::ios::out | std::ios::binary);

  stx::result_t<std::string> result(std::nullptr_t{}, "Unset");

  if (file.is_open()) {
    while (file_len > 0) {
      char byte = reader.Read<char>(buffer);
      file << byte;
      file_len--;
    }

    file.close();

    result = pm.template LoadPackageFromZip<ScriptedDataType>(path);
  }

  if (result.is_error()) {
    Logger::Logf(LogLevel::critical, "Failed to download package with package ID %s: %s", packageId.c_str(), result.error_cstr());

    // There was a problem creating the file
    SendDownloadComplete(false);
  }
}

template<typename PackageManagerType>
Poco::Buffer<char> DownloadScene::SerializePackageData(const std::string& packageId, NetPlaySignals header, PackageManagerType& packageManager)
{
  Poco::Buffer<char> buffer{ 0 };
  std::vector<char> fileBuffer;

  BufferWriter writer;
  size_t len = 0;
  
  auto result = packageManager.GetPackageFilePath(packageId);
  if (result.is_error()) {
    Logger::Logf(LogLevel::critical, "Could not serialize package: %s", result.error_cstr());

    // Give the remote client a headsup abort
    SendDownloadComplete(false);
  }
  else {
    std::string path = result.value();
    
    if (auto result = stx::zip(path, path + ".zip"); result.value()) {
      path = path + ".zip";

      std::ifstream fs(path, std::ios::binary | std::ios::ate);
      std::ifstream::pos_type pos = fs.tellg();
      len = pos;
      fileBuffer.resize(len);
      fs.seekg(0, std::ios::beg);
      fs.read(&fileBuffer[0], pos);
    }
  }

  // header
  writer.Write(buffer, header);

  // package name
  writer.WriteTerminatedString(buffer, packageId);

  // file size
  writer.Write<size_t>(buffer, len);

  // file contents
  writer.WriteBytes<char>(buffer, fileBuffer.data(), fileBuffer.size());

  return buffer;
}

void DownloadScene::Abort()
{
  if (!aborting) {
    for (auto& [key, value] : contentToDownload) {
      value = "Failed";
    }
    Logger::Logf(LogLevel::critical, "Aborting");
    SendDownloadComplete(false);
    aborting = true;
  }
}

void DownloadScene::onUpdate(double elapsed)
{
  if (!(packetProcessor->IsHandshakeAck() && remoteHandshake) && !aborting) return;

  if (!hasTradedData) {
    hasTradedData = true;

    this->TradeBlockPackageData(playerBlockPackageList);
    this->TradeCardPackageData(playerCardPackageList);
    this->TradePlayerPackageData(playerHash);
    return;
  }

  SendPing();

  if (aborting) {
    abortingCountdown -= from_seconds(elapsed);
    if (abortingCountdown <= frames(0)) {
      // abort match
      using effect = swoosh::types::segue<PixelateBlackWashFade>;
      getController().pop<effect>();
    }

    return;
  }
  
  if (AllTasksComplete()) {
    SendDownloadComplete(true);

    if (downloadSuccess && remoteSuccess) {
      getController().pop();
    }
  }
}

void DownloadScene::onDraw(sf::RenderTexture& surface)
{
  auto& packageManager = RemoteCardPartition();

  surface.draw(bg);
  blur.apply(surface);

  float w = static_cast<float>(getController().getVirtualWindowSize().x);
  float h = static_cast<float>(getController().getVirtualWindowSize().y);

  // 1. Draw the state status info
  if (AllTasksComplete()) {
    label.SetString("Complete, waiting...");

    auto bounds = label.GetLocalBounds();
    label.setOrigin(sf::Vector2f(bounds.width * label.getScale().x, 0));
    label.setPosition(w, 0);
    label.SetColor(sf::Color::Green);
    surface.draw(label);
    return;
  }

  auto bounds = label.GetLocalBounds();
  label.setOrigin(sf::Vector2f(bounds.width * label.getScale().x , 0));
  label.setPosition(w, 0);
  label.SetColor(sf::Color::White);
  surface.draw(label);

  sf::Sprite icon;

  for (auto& [key, value] : contentToDownload) {
    if (!packageManager.HasPackage(key)) continue;

    label.SetString(key + " - " + value);

    auto bounds = label.GetLocalBounds();
    float ydiff = bounds.height * label.getScale().y;

    if (auto iconTexture = packageManager.FindPackageByID(key).GetIconTexture()) {
      icon.setTexture(*iconTexture, true);
      float iconHeight = icon.getLocalBounds().height;
      icon.setOrigin(0, iconHeight);
    }

    icon.setPosition(sf::Vector2f(bounds.width + 5.0f, bounds.height));
    label.setOrigin(sf::Vector2f(0, 0));
    label.setPosition(0, h);

    h += ydiff + 5.0f;

    surface.draw(label);
  }
}

void DownloadScene::onLeave()
{
}

void DownloadScene::onExit()
{
}

void DownloadScene::onEnter()
{
}

void DownloadScene::onStart()
{
  Logger::Logf(LogLevel::info, "onStart() sending handshake");
  SendHandshakeAck();
}

void DownloadScene::onResume()
{
}

void DownloadScene::onEnd()
{
  packetProcessor->SetPacketBodyCallback(nullptr);
  packetProcessor->SetKickCallback(nullptr);
}
