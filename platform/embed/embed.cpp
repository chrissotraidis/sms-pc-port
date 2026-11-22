// The embedding app's side of the port (sms_embed.h): the game's thread, the
// controllers it reads and the calls the app makes.
#include "embed/sms_embed.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <mutex>
#include <string>

#include "port_platform.h"
#include <dolphin/pad.h>

extern void SMS_main(void);
extern "C" void port_audio_host_pull(void* stream, int bytes);
extern "C" void GXPC_SetEmbeddedSurface(void* metalLayer, void* getInstanceProcAddr, int width, int height);
extern "C" void GXPC_SetEmbeddedSize(int width, int height);
extern "C" void GXPC_SetEmbeddedRumble(void (*rumble)(unsigned short strength));
extern "C" void GXPC_SetEmbeddedPaused(int paused);
extern "C" uint32_t GXPC_FrameCount(void);
extern "C" double GXPC_LastPresentSeconds(void);

namespace {

std::atomic<bool> s_started{false};
std::string s_disc;
std::mutex s_padLock;
SmsEmbedPad s_pads[4];
// SMS_AUTOPRESS (scripted input for repeatable test routes, platform/pad):
// the port's own input path plays it, in place of the app's controllers.
bool s_scripted = false;

// The game keeps pointers to its locals in u32 slots: its thread runs on a
// stack below the game's 2 GiB (port_low_alloc), as src/port_main.cpp does.
void* runGame(void*) {
    char name[] = "sms";
    char* argv[] = {name, &s_disc[0], nullptr};
    port_init(2, argv);
    port_log("[embed] entering SMS_main\n");
    SMS_main();
    port_log("[embed] SMS_main returned\n");
    return nullptr;
}

}  // namespace

// platform/pad: the app's controllers in place of SDL's (0: the port reads its
// own input, which plays SMS_AUTOPRESS).
extern "C" int port_host_pad_read(PADStatus* status) {
    if (s_scripted) return 0;
    std::lock_guard<std::mutex> lock(s_padLock);
    for (int i = 0; i < 4; i++) {
        const SmsEmbedPad& pad = s_pads[i];
        memset(&status[i], 0, sizeof status[i]);
        status[i].err = pad.connected ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
        if (!pad.connected)
            continue;
        status[i].button = pad.buttons;
        status[i].stickX = pad.stickX;
        status[i].stickY = pad.stickY;
        status[i].substickX = pad.substickX;
        status[i].substickY = pad.substickY;
        status[i].triggerLeft = pad.triggerL;
        status[i].triggerRight = pad.triggerR;
        status[i].analogA = (pad.buttons & PAD_BUTTON_A) ? 255 : 0;
        status[i].analogB = (pad.buttons & PAD_BUTTON_B) ? 255 : 0;
    }
    return 1;
}

extern "C" {

uint32_t sms_embed_version(void) { return SMS_EMBED_VERSION; }

int sms_embed_start(const SmsEmbedConfig* config) {
    if (!config || config->version != SMS_EMBED_VERSION || !config->discImage) return -1;
    if (s_started.exchange(true)) return -1;
    s_disc = config->discImage;
    const char* script = getenv("SMS_AUTOPRESS");
    s_scripted = script && *script;
    setenv("SMS_AUDIO_OUT", "host", 1);
    setenv("SMS_GX_BACKEND", "vulkan", 1);
    GXPC_SetEmbeddedSurface(config->metalLayer, config->getInstanceProcAddr, config->width, config->height);
    GXPC_SetEmbeddedRumble(config->rumble);
    const unsigned long size = 8ul << 20;
    void* stack = port_low_alloc(size);
    if (!stack) {
        port_log("[embed] no memory below 2 GiB (of the game's address window) for the game's stack\n");
        return -1;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, size);
    pthread_t thread;
    const int failed = pthread_create(&thread, &attr, runGame, nullptr);
    pthread_attr_destroy(&attr);
    if (failed) {
        port_log("[embed] cannot start the game thread\n");
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

void sms_embed_set_pad(int port, const SmsEmbedPad* pad) {
    if (port < 0 || port >= 4) return;
    std::lock_guard<std::mutex> lock(s_padLock);
    if (pad) s_pads[port] = *pad;
    else s_pads[port] = SmsEmbedPad();
}

void sms_embed_resize(int width, int height) { GXPC_SetEmbeddedSize(width, height); }

void sms_embed_audio_pull(void* stream, int bytes) { port_audio_host_pull(stream, bytes); }

void sms_embed_pause(int paused) { GXPC_SetEmbeddedPaused(paused); }

void sms_embed_stats(SmsEmbedStats* stats) {
    if (!stats) return;
    stats->frames = GXPC_FrameCount();
    stats->lastPresentSeconds = GXPC_LastPresentSeconds();
}

}  // extern "C"
