HexedDiscordRPC - Discord Rich Presence for 64-bit GTA SA-MP Android clients (NOT TESTED ON A DEVICE)

Target: 64-bit (arm64-v8a) SA-MP APKs that run AML (64-bit) and a MoonLoader/MonetLoader Lua environment.

1. From the Discord Developer Portal > your app > Social SDK > Downloads, get the ANDROID
   package (1.10+). Do not use the Linux/desktop package: its .so is x86-64 and will not work.
   Copy libdiscord_partner_sdk.so (arm64-v8a) into jni/sdk/lib/arm64-v8a/.
   discordpp.h and cdiscord.h are already in jni/sdk/include/.
2. Edit jni/hexed_discord_rpc.cpp: set kAppId (your Discord Application ID) and
   kLargeImage (asset key you uploaded under Rich Presence > Art Assets).
3. Push to a PRIVATE GitHub repo, open Actions > "Build HexedDiscordRPC" > Run workflow.
4. Download the artifact (libs/arm64-v8a/).
5. Copy the .so files into AML's mods folder, and scripts/HexedDiscordRPC.lua into
   the MonetLoader scripts folder.
6. In game run /rpcinfo, and if the printed path differs from the plugin default,
   paste it into kFilePathOverride and rebuild.
7. Check logs: adb logcat -s HexedRPC

Lines marked VERIFY in the C++ depend on your AML version. InitAndroidBridge() is a stub.
The SDK header says Android 11+ apps need com.discord in the manifest <queries>; a mod cannot add that.
