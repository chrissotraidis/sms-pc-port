/* Embedding the game in an app that owns its window, input and sound
 * (SunPad on iOS, iPadOS and tvOS). The app builds the port with
 * -DSMS_EMBED=ON, which makes libsms_embed.dylib in place of the sms program,
 * loads it, and drives it through these calls.
 *
 * Settings are the port's environment variables (SMS_GX_SCALE, SMS_FRAME_RATE,
 * SMS_WIDESCREEN, SMS_SAVE_DIR, ...), set by the app before sms_embed_start.
 * The log goes to stderr, which the app may point at a file. */
#ifndef SMS_EMBED_H
#define SMS_EMBED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMS_EMBED_VERSION 1

/* One controller in the GameCube's terms. buttons uses the PAD_BUTTON_* and
 * PAD_TRIGGER_* bits of dolphin/pad.h; sticks run from -100 to 100, right and
 * up positive; triggers from 0 to 255, and their click is the PAD_TRIGGER_L/R
 * bit. */
typedef struct SmsEmbedPad {
    uint16_t buttons;
    int8_t stickX, stickY;
    int8_t substickX, substickY;
    uint8_t triggerL, triggerR;
    uint8_t connected;
} SmsEmbedPad;

typedef struct SmsEmbedConfig {
    uint32_t version;           /* SMS_EMBED_VERSION */
    const char* discImage;      /* the player's GMSE01 image (.iso, .gcm or .ciso) */
    void* metalLayer;           /* the CAMetalLayer the game draws in */
    void* getInstanceProcAddr;  /* the app's vkGetInstanceProcAddr (MoltenVK) */
    int width, height;          /* the layer's drawable size, in pixels */
    /* The controller's motor, 0 (off) to 65535; may be NULL. Called on the game's thread. */
    void (*rumble)(unsigned short strength);
} SmsEmbedConfig;

typedef struct SmsEmbedStats {
    uint32_t frames;            /* frames shown so far */
    double lastPresentSeconds;  /* the last frame's present */
} SmsEmbedStats;

uint32_t sms_embed_version(void);
/* Starts the game on a thread of its own and returns at once: 0 when it
 * started, -1 when it could not (the version differs, it already runs, no
 * memory for its stack). The game runs until the process ends. */
int sms_embed_start(const SmsEmbedConfig* config);
/* Controller port 0 to 3, from any thread; NULL disconnects it. */
void sms_embed_set_pad(int port, const SmsEmbedPad* pad);
/* The layer's new drawable size, in pixels. */
void sms_embed_resize(int width, int height);
/* SMS_AUDIO_OUT=host (which sms_embed_start sets): bytes of 32 kHz
 * interleaved stereo 16-bit samples, for the app's audio unit. */
void sms_embed_audio_pull(void* stream, int bytes);
/* 1 holds the game at its next frame, with nothing more sent to the GPU
 * (before the app goes to the background); 0 lets it go on. */
void sms_embed_pause(int paused);
void sms_embed_stats(SmsEmbedStats* stats);

#ifdef __cplusplus
}
#endif

#endif
