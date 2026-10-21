// Menu de desenvolvedor da EA. O módulo que o roda (CRunModuleAki: Init 0x77C40, Update 0x77A40, Render 0x77B20,
// Shutdown 0x77DD0) continua no default.xbe, mas a versão de loja perdeu a vtable, o construtor e o caso na fábrica
// de módulos (0x773A0): nada o cria. Aqui ele é montado por fora, com o alocador do jogo, e entregue ao gerenciador
// de módulos como "próximo módulo", do jeito que a fábrica faz. Mapa e endereços: vyanhursky/DefJamFFNY-recomp
// (MIT), docs/research/dev-module-revival.md; o mesmo default.xbe USA.

#include <atomic>
#include <mutex>
#include <string>

#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/memory.h"
#include "xdk/hle.h"
#include "common/devmenu.h"
#include "common/lang.h"

namespace {

constexpr uint32_t kRegistry = 0x2FB148;       // subsistemas do jogo por número
constexpr uint32_t kManager = 0x17;            // CRunModuleMan: +0x18 módulo atual, +0x1C próximo, +0x24 estado
constexpr uint32_t kTasks = 0x1;               // lista de tarefas: primeira em +4, próxima em nó+8
constexpr uint32_t kFrontEnd = 0xD;            // +0x9C = tela que o front end abre ao carregar
constexpr uint32_t kFrontEndTable = 0x29D784;  // vtable do módulo de front end (título, menus)
constexpr uint32_t kDevTask = 0x2DBF28;        // tarefa estática que o init do modo põe na lista
constexpr uint32_t kMainMenuScreen = 15;       // intMain/mainMenu.swf
constexpr uint32_t kFrontEndLoader = 9;        // tipo na fábrica: carregador que leva ao front end
constexpr uint32_t kModuleSize = 0x44;
constexpr uint32_t kAllocTag = 0x29D4B4;       // uma etiqueta de "RunModule" que a própria fábrica usa

std::atomic<int> g_request{0};  // 1 abrir, 2 sair
std::atomic<uint32_t> g_table{0};
std::mutex g_message_lock;
std::string g_message;

uint32_t Subsystem(uint32_t id) { return ::kernel::R32(kRegistry + id * 4); }

void Say(const char* pt, const char* en) {
    std::lock_guard lock(g_message_lock);
    g_message = defjam::T(pt, en);
    LOGI("defjam: menu de dev: %s", en);
}

uint32_t CurrentTable(uint32_t manager) {
    const uint32_t module = ::kernel::R32(manager + 0x18);
    return module ? ::kernel::R32(module) : 0;
}

// O init do modo põe a tarefa 0x2DBF28 na cabeça da lista sem olhar se já está lá, e o unload não a tira: na segunda
// abertura ela apontaria para si mesma e o passo das tarefas nunca acabaria (o jogo congela). Tirada a cada unload.
void UnlinkDevTask() {
    const uint32_t tasks = Subsystem(kTasks);
    if (!tasks) return;
    uint32_t link = tasks + 4;
    for (int guard = 0; guard < 4096; guard++) {
        const uint32_t node = ::kernel::R32(link);
        if (!node) return;
        if (node == kDevTask) {
            ::kernel::W32(link, ::kernel::R32(node + 8));
            ::kernel::W32(node + 8, 0);
            return;
        }
        link = node + 8;
    }
}

// a vtable que a versão de loja perdeu (base 0x29D5A4 preenchida, como a do módulo de jogo 0x29D5E4)
uint32_t Table() {
    if (const uint32_t t = g_table) return t;
    const uint32_t t = ::mem::AllocSystem(32);
    const uint32_t slots[8] = {
        0x76D30,  // destrutor da base (libera com 0x29740)
        0x77C40,  // Load = CRunModuleAki::Init (na thread de carga)
        0x76EF0,  // Start: nada (um ret)
        0x77A40,  // Update(dt)
        0x77B20,  // Render
        0x76EE0,  // CanLeave: sempre sim
        0x76EF0,  // Stop: nada
        0x77DD0,  // Unload = Shutdown
    };
    for (int i = 0; i < 8; i++) ::kernel::W32(t + i * 4, slots[i]);
    g_table = t;
    return t;
}

void Open(X86Context& ctx, uint32_t manager) {
    if (CurrentTable(manager) != kFrontEndTable) {
        Say("Abra pelo menu principal do jogo (fora das lutas e da historia).",
            "Open it from the game's main menu (not during a fight or Story).");
        return;
    }
    if (::kernel::R32(manager + 0x24) != 0) {
        Say("O jogo esta trocando de tela: tente de novo em um instante.", "The game is changing screens: try again in a moment.");
        return;
    }
    const uint32_t table = Table();
    UnlinkDevTask();
    const uint32_t module = ::kernel::CallGuest(ctx, 0x29680, {kModuleSize, kAllocTag, 0});
    if (!module) {
        Say("O jogo nao teve memoria para o menu de dev.", "The game had no memory for the developer menu.");
        return;
    }
    for (uint32_t off = 0; off < kModuleSize; off += 4) ::kernel::W32(module + off, 0);
    ::kernel::W32(module, table);
    // o resto do construtor (como o 0x76DC0 do módulo de jogo): o menu de pausa e o popup
    ctx.ecx = module;
    ::kernel::CallGuest(ctx, 0x779F0, {});
    ::kernel::W32(manager + 0x1C, module);
    ::kernel::W32(manager + 0x24, 1);
    Say("Abrindo o menu de desenvolvedor...", "Opening the developer menu...");
}

void Leave(X86Context& ctx, uint32_t manager) {
    if (!g_table || CurrentTable(manager) != g_table) {
        Say("O menu de desenvolvedor nao esta aberto.", "The developer menu is not open.");
        return;
    }
    if (const uint32_t front_end = Subsystem(kFrontEnd)) ::kernel::W32(front_end + 0x9C, kMainMenuScreen);
    ctx.ecx = manager;
    ::kernel::CallGuest(ctx, 0x773A0, {kFrontEndLoader});
    Say("Voltando ao menu principal...", "Back to the main menu...");
}

}  // namespace

// o passo do gerenciador de módulos (thiscall, dt): na thread do jogo, o lugar onde a própria fábrica troca módulos
X86_EXTERN_FUNC(__imp__sub_00077040);
X86_FUNC(sub_00077040) {
    if (const int request = g_request.exchange(0)) {
        const X86Context saved = ctx;
        if (request == 1) Open(ctx, saved.ecx);
        else Leave(ctx, saved.ecx);
        ctx = saved;
    }
    __imp__sub_00077040(ctx, base);
}

// Unload do módulo de dev (thiscall no módulo; +0x14 = o sistema de modos). O QUIT da pausa no meio de uma luta (ou
// cena) montada aqui troca de módulo sem o modo encerrar o que começou: as tarefas da luta ficavam na lista e, na
// próxima abertura, uma delas perguntava pelo modo "jogo" (0x44210 devolvia 0) e o jogo caía. Antes do original, a
// mesma limpeza que o modo faz no fim normal: 0x2DF10 (fim da luta, se +0x24 bit 1) e 0x2D570 (fim da cena,
// estado 3 em +0x2A0). Depois, a tarefa estática sai da lista, e o front end abre no menu principal, não no filme.
X86_EXTERN_FUNC(__imp__sub_00077DD0);
X86_FUNC(sub_00077DD0) {
    if (const uint32_t mode = ::kernel::R32(ctx.ecx + 0x14)) {
        const X86Context saved = ctx;
        if (::kernel::R32(mode + 0x24) & 1) {
            ctx.ecx = mode;
            ::kernel::CallGuest(ctx, 0x2DF10, {});
            LOGI("defjam: menu de dev: luta encerrada antes de sair");
        } else if (::kernel::R32(mode + 0x2A0) == 3) {
            ctx.ecx = mode;
            ::kernel::CallGuest(ctx, 0x2D570, {});
            LOGI("defjam: menu de dev: cena encerrada antes de sair");
        }
        ctx = saved;
    }
    __imp__sub_00077DD0(ctx, base);
    UnlinkDevTask();
    if (const uint32_t front_end = Subsystem(kFrontEnd)) ::kernel::W32(front_end + 0x9C, kMainMenuScreen);
}

namespace defjam {

void OpenDevMenu() { g_request = 1; }
void LeaveDevMenu() { g_request = 2; }

bool DevMenuActive() {
    const uint32_t table = g_table;
    if (!table) return false;
    const uint32_t manager = Subsystem(kManager);
    return manager && CurrentTable(manager) == table;
}

std::string DevMenuMessage() {
    std::lock_guard lock(g_message_lock);
    return g_message;
}

}  // namespace defjam
