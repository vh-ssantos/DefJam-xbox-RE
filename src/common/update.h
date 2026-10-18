// Atualização automática pelo GitHub Releases (ver update.cpp).
#pragma once
#include <string>

namespace defjam {

extern bool g_check_updates;     // settings.toml: check_updates
void StartUpdateCheck();         // uma vez, ao abrir
const char* Version();           // a versão deste exe
std::string UpdateAvailable();   // a versão nova, ou vazio
std::string UpdateStatus();      // texto do andamento da instalação
void InstallUpdate();            // baixa e instala em segundo plano

}  // namespace defjam
