#pragma once
#include <stddef.h>
#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

#ifdef __cplusplus
extern "C" {
#endif

#define B21UI_ABI_VERSION 1u

typedef uint32_t B21UI_ClientId;
typedef void (*B21UI_TaskCallback)(void* user);

enum { B21UI_DEVICE_KEYBOARD_MOUSE = 0, B21UI_DEVICE_GAMEPAD = 1 };
enum { B21UI_KIND_MODAL = 0, B21UI_KIND_OVERLAY = 1 };
enum { B21UI_FLAG_PAUSES_GAME = 1u << 0 };
enum { B21UI_FRAME_DRAW_CURSOR = 1u << 0 };

enum B21UI_EventType {
    B21UI_EV_MOUSE_BUTTON = 1, /* code = button 0..4, down */
    B21UI_EV_MOUSE_WHEEL = 2,  /* y = +1 up / -1 down */
    B21UI_EV_KEY = 3,          /* code = Win32 VK, down */
    B21UI_EV_CHAR = 4,         /* code = UTF-16 unit */
    B21UI_EV_PAD_BUTTON = 5,   /* code = B21UI_PAD_*, down, x = analog value */
    B21UI_EV_PAD_STICK = 6,    /* code = B21UI_STICK_*, x, y in [-1, 1], +y = up */
    B21UI_EV_FOCUS = 7         /* down = 1 window gained focus, 0 lost */
};

enum B21UI_PadButton {
    B21UI_PAD_A = 0, B21UI_PAD_B, B21UI_PAD_X, B21UI_PAD_Y,
    B21UI_PAD_LB, B21UI_PAD_RB, B21UI_PAD_LT, B21UI_PAD_RT,
    B21UI_PAD_BACK, B21UI_PAD_START, B21UI_PAD_LS, B21UI_PAD_RS,
    B21UI_PAD_DPAD_UP, B21UI_PAD_DPAD_DOWN, B21UI_PAD_DPAD_LEFT, B21UI_PAD_DPAD_RIGHT,
    B21UI_PAD_COUNT
};

enum { B21UI_STICK_LEFT = 0, B21UI_STICK_RIGHT = 1 };

typedef struct B21UI_Event {
    uint32_t type;
    uint32_t code;
    uint32_t down;
    float x;
    float y;
} B21UI_Event;

typedef struct B21UI_Frame {
    uint32_t size;
    uint32_t flags;
    float displayWidth;
    float displayHeight;
    float deltaSeconds;
    float cursorX;
    float cursorY;
    uint32_t activeDevice;
    uint32_t focused;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    const B21UI_Event* events;
    uint32_t eventCount;
    uint32_t hudColor; /* player HUD color 0xAARRGGBB; alpha 0 (or a smaller `size`) = host did not send it */
} B21UI_Frame;

typedef struct B21UI_ClientDesc {
    uint32_t size;
    const char* name;
    uint32_t kind;
    uint32_t flags;
    void* user;
    void (*render)(void* user, const B21UI_Frame* frame);
    void (*deviceLost)(void* user);
    void (*focusChanged)(void* user, uint32_t focused);
} B21UI_ClientDesc;

typedef struct B21UI_HostApi {
    uint32_t size;
    uint32_t abiVersion;
    B21UI_ClientId (*registerClient)(const B21UI_ClientDesc* desc);
    uint32_t (*open)(B21UI_ClientId id);
    void (*close)(B21UI_ClientId id);
    uint32_t (*isOpen)(B21UI_ClientId id);
    uint32_t (*available)(void);
    uint32_t (*activeDevice)(void);
    void (*setCursor)(float x, float y);
    void (*setPausesGame)(B21UI_ClientId id, uint32_t pausesGame);
    /* Takes ownership of user; destroy runs in the caller's module after execution or discard. */
    void (*queueGameTask)(void* user, B21UI_TaskCallback run, B21UI_TaskCallback destroy);
} B21UI_HostApi;

typedef struct B21UI_Rendezvous {
    uint32_t size;
    uint32_t frameworkVersion;
    uint32_t abiVersion;
    const B21UI_HostApi* (*hostApi)(void);
} B21UI_Rendezvous;

typedef const B21UI_Rendezvous* (*B21UI_RendezvousFn)(void);

#define B21UI_RENDEZVOUS_EXPORT "B21UI_Rendezvous_v1"
#define B21UI_RENDEZVOUS_MIN_SIZE ((uint32_t)(offsetof(B21UI_Rendezvous, hostApi) + sizeof(void*)))
#define B21UI_HOSTAPI_MIN_SIZE ((uint32_t)(offsetof(B21UI_HostApi, setCursor) + sizeof(void*)))
#define B21UI_CLIENTDESC_MIN_SIZE ((uint32_t)(offsetof(B21UI_ClientDesc, focusChanged) + sizeof(void*)))

#ifdef __cplusplus
}
#endif
