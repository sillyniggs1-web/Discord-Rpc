// Hexed Discord RPC - AML plugin for GTA SA-MP Nezuko (Android)
//
// Reads HEXED/discord_rpc.txt (written by HexedDiscordRPC.lua) and publishes it
// as Discord Rich Presence through the Discord Social SDK 1.10+ unauthenticated
// RPC transport (Discord app must be installed and signed in).
//
// Card layout (matches the PC screenshot):
//   Line 1  name     "San Andreas Multiplayer"
//   Line 2  details  <server name>
//   Line 3  state    <nickname> • <ip>:<port>
//   Timer   start    time you joined the server
//
// NOT TESTED ON A DEVICE. Items marked VERIFY depend on your AML headers / SDK package.

#include <android/log.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <mod/amlmod.h> // VERIFY: header path/name in your AML checkout

#define DISCORDPP_IMPLEMENTATION // exactly one .cpp must define this
#include "discordpp.h"

// ---- Fill these in ---------------------------------------------------------
static const uint64_t    kAppId      = 0;                         // your Discord Application ID
static const char*       kLargeImage = "samp";                    // asset key uploaded in Developer Portal > Rich Presence > Art Assets (lowercase)
static const char*       kAppName    = "San Andreas Multiplayer"; // top line override
// Path of the file written by the Lua script. Run /rpcinfo in game and paste the
// printed path here if the default guess below is not where the Lua script writes.
static const char*       kFilePathOverride = "";
// ----------------------------------------------------------------------------

MYMOD(net.hexed.discordrpc, HexedDiscordRPC, 1.0, HEXED Studio) // VERIFY: macro signature

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "HexedRPC", __VA_ARGS__)

struct Snap {
    bool connected = false;
    std::string nick, server, ip, port;
    long start = 0, hb = 0;
};

static std::atomic<bool> g_run{false};

static std::string FilePath() {
    if (kFilePathOverride && *kFilePathOverride) return kFilePathOverride;
    const char* base = aml->GetAndroidDataPath(); // VERIFY: exact method name in amlmod.h
    return std::string(base ? base : "") + "/HEXED/discord_rpc.txt";
}

static bool ReadSnap(const std::string& path, Snap& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::map<std::string, std::string> kv;
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    out = Snap{};
    out.connected = kv["connected"] == "1";
    out.nick = kv["nick"];
    out.server = kv["server"];
    out.ip = kv["ip"];
    out.port = kv["port"];
    out.start = std::atol(kv["start"].c_str());
    out.hb = std::atol(kv["hb"].c_str());
    return true;
}

// Discord limits text fields to 128 characters; cut without splitting UTF-8.
static std::string Cut(std::string s) {
    if (s.size() <= 128) return s;
    size_t n = 128;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n);
}

static std::string Key(const Snap& s) {
    return std::to_string(s.connected) + "|" + s.nick + "|" + s.server + "|" + s.ip + "|" + s.port + "|" + std::to_string(s.start);
}

static discordpp::Activity Build(const Snap& s, long pluginStart) {
    discordpp::Activity a;
    a.SetType(discordpp::ActivityTypes::Playing);
    a.SetName(kAppName);

    discordpp::ActivityTimestamps ts;
    discordpp::ActivityAssets assets;
    assets.SetLargeImage(kLargeImage);
    assets.SetLargeText(kAppName);

    if (s.connected) {
        std::string addr = s.ip + ":" + s.port;
        a.SetDetails(Cut(s.server.size() >= 2 ? s.server : addr));
        a.SetState(Cut(s.nick.empty() ? addr : s.nick + " \xE2\x80\xA2 " + addr)); // "nick • ip:port"
        ts.SetStart(static_cast<uint64_t>(s.start));
    } else {
        a.SetDetails("Not connected");
        a.SetState("Browsing server list");
        ts.SetStart(static_cast<uint64_t>(pluginStart));
    }
    a.SetTimestamps(ts);
    a.SetAssets(assets);
    return a;
}

// VERIFY / TODO: the Android SDK may need a JavaVM and Application context before
// the Client is created (the SDK is JNI-based). Get them from AML/the game's
// JNI_OnLoad and hand them to the SDK here, following the Android section of the
// SDK package docs. Left as a stub on purpose: not verified.
static bool InitAndroidBridge() { return true; }

static void Worker() {
    if (kAppId == 0) { LOG("kAppId not set, plugin idle"); return; }
    if (!InitAndroidBridge()) { LOG("Android bridge init failed"); return; }

    auto client = std::make_shared<discordpp::Client>();
    client->AddLogCallback([](auto message, auto severity) {
        LOG("[sdk %s] %s", std::string(EnumToString(severity)).c_str(), std::string(message).c_str());
    }, discordpp::LoggingSeverity::Info);

    client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
        LOG("status: %s", std::string(discordpp::Client::StatusToString(status)).c_str());
        if (error != discordpp::Client::Error::None)
            LOG("sdk error: %s (detail %d)", std::string(discordpp::Client::ErrorToString(error)).c_str(), (int)errorDetail);
    });

    client->SetApplicationId(kAppId); // no Connect(): unauthenticated RPC

    const long pluginStart = static_cast<long>(std::time(nullptr));
    const std::string path = FilePath();
    LOG("watching %s", path.c_str());

    std::string lastKey = "\x01"; // force first send
    auto lastRead = std::chrono::steady_clock::time_point{};

    while (g_run) {
        discordpp::RunCallbacks();

        auto now = std::chrono::steady_clock::now();
        if (now - lastRead >= std::chrono::seconds(2)) {
            lastRead = now;
            Snap s;
            bool ok = ReadSnap(path, s);
            // Missing file or stale heartbeat (>30s) = treat as not connected.
            if (!ok || (s.hb > 0 && std::time(nullptr) - s.hb > 30)) s = Snap{};

            std::string k = Key(s);
            if (k != lastKey) {
                lastKey = k;
                if (s.connected) {
                    client->UpdateRichPresence(Build(s, pluginStart), [](const discordpp::ClientResult& r) {
                        LOG(r.Successful() ? "presence updated" : "presence update failed");
                    });
                } else {
                    client->ClearRichPresence(); // no card while not on a server
                    LOG("presence cleared");
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

extern "C" void OnModLoad() { // VERIFY: entry point name for your AML version
    g_run = true;
    std::thread(Worker).detach();
}
