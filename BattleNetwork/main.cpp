#include "bnGame.h"
#include "battlescene/bnMobBattleScene.h"
#include "bindings/bnScriptedMob.h"
#include "bnPlayerPackageManager.h"
#include "bnMobPackageManager.h"
#include "bnGameOverScene.h"
#include "bnTitleScene.h"
#include "bnACDCBackground.h"
#include "bnMob.h"
#include "bnPlayer.h"
#include "bnEmotions.h"
#include "bnCardFolder.h"
#include "stx/string.h"
#include "stx/result.h"
#include "cxxopts/cxxopts.hpp"
#include "netplay/bnNetPlayConfig.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/URI.h>
#include <Poco/StreamCopier.h>

int LaunchGame(Game& g, const cxxopts::ParseResult& results);
int HandleBattleOnly(Game& g, TaskGroup tasks, const std::string& playerpath, const std::string& mobpath, bool isURL);

template<typename ScriptedDataType, typename PackageManager>
stx::result_t<std::string> DownloadPackageFromURL(const std::string& url, PackageManager& packageManager);

int main(int argc, char** argv) {
  DrawWindow win;
  win.Initialize("Open Net Battle v2.0a", DrawWindow::WindowMode::window);
  Game game{ win };

  cxxopts::Options options("ONB", "Open Net Battle Engine");
  options.add_options()
    ("e,errorLevel", "Set the level to filter error messages [silent|info|warning|critical|debug] (default is `critical`)", cxxopts::value<std::string>()->default_value("critical"))
    ("d,debug", "Enable debugging")
    ("s,singlethreaded", "run logic and draw routines in a single, main thread")
    ("b,battleonly", "Jump into a battle from a package")
    ("mob", "path to mob file on disk", cxxopts::value<std::string>())
    ("moburl", "path to mob file to download from a web address", cxxopts::value<std::string>())
    ("player", "path to player package on disk", cxxopts::value<std::string>())
    ("l,locale", "set flair and language to desired target", cxxopts::value<std::string>()->default_value("en"))
    ("p,port", "port for PVP", cxxopts::value<int>()->default_value(std::to_string(NetPlayConfig::OBN_PORT)))
    ("r,remotePort", "remote port for PVP", cxxopts::value<int>()->default_value(std::to_string(NetPlayConfig::OBN_PORT)))
    ("w,cyberworld", "ip address of main hub", cxxopts::value<std::string>()->default_value("127.0.0.1"))
    ("m,mtu", "Maximum Transmission Unit - adjust to send big packets", cxxopts::value<uint16_t>()->default_value(std::to_string(NetManager::DEFAULT_MAX_PAYLOAD_SIZE)));

  try {
    cxxopts::ParseResult parsedOptions = options.parse(argc, argv);

    // Go the the title screen to kick off the rest of the app
    if (LaunchGame(game, parsedOptions) == EXIT_SUCCESS) {
      // blocking
      game.Run();
    }
  }
  catch (std::exception& e) {
    Logger::Log(LogLevel::critical, e.what());
  }
  catch (...) {
    Logger::Log(LogLevel::critical, "Game encountered an unknown exception. Aborting.");
  }
  game.Exit();

  // finished
  return EXIT_SUCCESS;
}

void ParseErrorLevel(std::string in) {
  std::map<std::string, bool> settings;

  // Parse input tokens
  // Levels are separated by pipes | and lowercased
  in = stx::replace(in, " ", ""); // trimmed whitespace
  std::vector<std::string> tokens = stx::tokenize(in, '|');

  for (std::string& s : tokens) {
    settings[s] = true;
  }

  uint8_t level = LogLevel::silent;

  std::string msg = "Logs will be displayed below. Log level is set to ";
  std::vector<std::string> valid;

  auto processSettings = [&settings, &level, &valid](const std::string& key, uint8_t value) {
    if (settings[key]) {
      level |= value;
      valid.push_back(key);
    }
  };

  processSettings("critical", LogLevel::critical);
  processSettings("warning", LogLevel::warning);
  processSettings("debug", LogLevel::debug);
  processSettings("info", LogLevel::info);

  if (settings["all"]) {
    level = LogLevel::all;
    valid.clear();
    valid.push_back("all");
  }
  
  std::string validStr;

  for (size_t i = 0; i < valid.size(); i++) {
    validStr += valid[i];

    if (i + 1u < valid.size()) {
      validStr += "|";
    }
  }

  msg += "`" + validStr + "`";
  std::cout << msg << std::endl;

  msg = std::string(msg.size(), '-');

  std::cout << msg << std::endl;
  
  Logger::SetLogLevel(level);
}

int LaunchGame(Game& g, const cxxopts::ParseResult& results) {
  srand((unsigned int)time(0));
  g.SetCommandLineValues(results);

  ParseErrorLevel(g.CommandLineValue<std::string>("errorLevel"));

  g.PrintCommandLineArgs();

  if (g.GetEndianness() == Endianness::big) {
    Logger::Log(LogLevel::info, "System arch is Big Endian");
  }
  else {
    Logger::Log(LogLevel::info, "System arch is Little Endian");
  }

  if (g.CommandLineValue<bool>("battleonly")) {
    std::string playerpath = g.CommandLineValue<std::string>("player");
    std::string mobpath = g.CommandLineValue<std::string>("mob");
    std::string moburl = g.CommandLineValue<std::string>("moburl");
    bool url = false;

    if (playerpath.empty()) {
      Logger::Logf(LogLevel::critical, "Battleonly mode needs `player` input argument");
      return EXIT_FAILURE;
    }

    if (mobpath.empty() && moburl.empty()) {
      Logger::Logf(LogLevel::critical, "Battleonly mode needs `mob` or `moburl` input argument");
      return EXIT_FAILURE;
    }

    if (moburl.size()) {
      mobpath = moburl;
      url = true;
    }

    return HandleBattleOnly(g, g.Boot(results), playerpath, mobpath, url);
  }

  // If single player game, the last screen the player will ever see 
  // is the game over screen so it goes to the bottom of the stack
  // before the TitleSceene:
  // g.push<GameOverScene>(); // <-- uncomment

  g.push<TitleScene>(g.Boot(results));
  return EXIT_SUCCESS;
}

int HandleBattleOnly(Game& g, TaskGroup tasks, const std::string& playerpath, const std::string& mobpath, bool isURL) {
  std::string mobid = mobpath;

  if (isURL) {
    auto result = DownloadPackageFromURL<ScriptedMob>(mobpath, g.MobPackageManager());
    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
      return EXIT_FAILURE;
    }

    mobid = result.value();
  }

  // wait for resources to be available for us
  const unsigned int maxtasks = tasks.GetTotalTasks();
  while (tasks.HasMore()) {
    const std::string taskname = tasks.GetTaskName();
    const unsigned int tasknumber = tasks.GetTaskNumber();
    Logger::Logf(LogLevel::info, "Running %s, [%i/%i]", taskname.c_str(), tasknumber+1u, maxtasks);
    tasks.DoNextTask();
  }

  ResourceHandle handle;

  // Play the pre battle rumble sound
  handle.Audio().Play(AudioType::PRE_BATTLE, AudioPriority::high);

  // Stop music and go to battle screen 
  handle.Audio().StopStream();

  auto field = std::make_shared<Field>(6, 3);

  // Get the navi we selected
  auto& playermeta = g.PlayerPackageManager().FindPackageByID(playerpath);
  const std::string& image = playermeta.GetMugshotTexturePath();
  Animation mugshotAnim = Animation() << playermeta.GetMugshotAnimationPath();
  const std::string& emotionsTexture = playermeta.GetEmotionsTexturePath();
  auto mugshot = handle.Textures().LoadFromFile(image);
  auto emotions = handle.Textures().LoadFromFile(emotionsTexture);
  auto player = std::shared_ptr<Player>(playermeta.GetData());

  auto& mobmeta = g.MobPackageManager().FindPackageByID(mobid);
  Mob* mob = mobmeta.GetData()->Build(field);

  // Shuffle our new folder
  auto folder = std::make_unique<CardFolder>(); // TODO: Load from file?
  folder->Shuffle();

  // Queue screen transition to Battle Scene with a white fade effect
  // just like the game
  if (!mob->GetBackground()) {
    mob->SetBackground(std::make_shared<ACDCBackground>());
  }

  PA programAdvance;

  MobBattleProperties props{
    { player, programAdvance, std::move(folder), field, mob->GetBackground() },
    MobBattleProperties::RewardBehavior::take,
    { mob },
    sf::Sprite(*mugshot),
    mugshotAnim,
    emotions,
  };

  g.push<MobBattleScene>(std::move(props));
  return EXIT_SUCCESS;
}

template<typename ScriptedDataType, typename PackageManager>
stx::result_t<std::string> DownloadPackageFromURL(const std::string& url, PackageManager& packageManager)
{
  using namespace Poco::Net;
  Poco::URI uri(url);
  std::string path(uri.getPathAndQuery());
  if (path.empty()) {
    return stx::error<std::string>("`moburl` was empty. Aborting.");
  }

  HTTPClientSession session(uri.getHost(), uri.getPort());
  HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
  HTTPResponse response;

  session.sendRequest(request);
  std::istream& rs = session.receiveResponse(response);
  std::cout << "[Response Status] " << response.getStatus() << " " << response.getReason() << std::endl;

  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_MOVED_PERMANENTLY) {
    std::cout << "Redirect: " << response.get("Location") << std::endl;
  }

  if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED) {
    return stx::error<std::string>("Unable to download package. Result was HTTP_UNAUTHORIZED. Aborting.");
  }

  std::string outpath = "cache/" + stx::rand_alphanum(12) + ".zip";
  std::ofstream ofs(outpath, std::fstream::binary);
  Poco::StreamCopier::copyStream(rs, ofs);
  ofs.close();

  return packageManager.template LoadPackageFromZip<ScriptedDataType>(outpath);
}
