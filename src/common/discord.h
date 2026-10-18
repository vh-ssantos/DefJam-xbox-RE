// Discord Rich Presence ("Jogando Def Jam: Fight for NY") pelo IPC local do app do Discord.
#pragma once

namespace defjam {

extern bool g_discord;  // settings.toml: discord
void StartDiscord();    // uma vez, depois das opções carregadas
void SetDiscordDetails(const char* details);  // ex.: "No menu", "Lutando", "Modo historia"

}  // namespace defjam
