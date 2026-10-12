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
enum { B21UI_FLAG_PAUSES_GAME = 1u << 0, B21UI_FLAG_SETTINGS = 1u << 1 };
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
    const char* settingsLabel;
    const char* settingsIcon;
    const char* settingsCategory;
} B21UI_ClientDesc;

typedef struct B21UI_SettingsPanel {
    B21UI_ClientId id;
    const char* label;
    uint32_t selected;
    const char* icon;
} B21UI_SettingsPanel;

typedef struct B21UI_KeybindingProvider {
    uint32_t size;
    const char* id;
    const char* label;
    void* user;
    /* UTF-8 JSON array, copied immediately. Callback and user live for the plugin's lifetime. */
    const char* (*snapshot)(void* user);
} B21UI_KeybindingProvider;

/* Performance metrics (framework 14). Ids are "owner.group.name"; the owner is the text before the first dot. */
typedef uint32_t B21UI_MetricId;

enum B21UI_MetricUnit {
    B21UI_UNIT_MS = 0, B21UI_UNIT_COUNT = 1, B21UI_UNIT_MB = 2, B21UI_UNIT_PERCENT = 3,
    B21UI_UNIT_FPS = 4, B21UI_UNIT_RATIO = 5, B21UI_UNIT_TEXT = 6
};
enum { B21UI_METRIC_HEADLINE = 1u << 0 };
enum { B21UI_PERF_OVERLAY = 1u << 0, B21UI_PERF_CAPTURE = 1u << 1, B21UI_PERF_PAGE = 1u << 2 };
enum { B21UI_PERF_MODE_OFF = 0, B21UI_PERF_MODE_COMPACT = 1, B21UI_PERF_MODE_EXPANDED = 2, B21UI_PERF_MODE_FULL = 3 };
enum { B21UI_CAPTURE_IDLE = 0, B21UI_CAPTURE_WAITING = 1, B21UI_CAPTURE_RECORDING = 2, B21UI_CAPTURE_WRITING = 3 };

typedef struct B21UI_MetricDesc {
    uint32_t size;
    const char* id;
    const char* label;
    const char* group;
    uint32_t unit;
    uint32_t flags;
} B21UI_MetricDesc;

/* Strings live until the next perfMetrics call on the same thread. */
typedef struct B21UI_MetricInfo {
    uint32_t size;
    B21UI_MetricId id;
    const char* key;
    const char* owner;
    const char* label;
    const char* group;
    uint32_t unit;
    uint32_t flags;
    uint32_t pinned;
    uint32_t hasValue;
    double current, average, minimum, maximum, p50, p99;
    const char* text;
    uint32_t hasBaseline;
    double baselineAverage;
} B21UI_MetricInfo;

typedef struct B21UI_PerfSettings {
    uint32_t size;
    uint32_t mode;
    float anchorX, anchorY;
    float captureSeconds;
    float captureDelaySeconds;
} B21UI_PerfSettings;

typedef struct B21UI_CaptureStatus {
    uint32_t size;
    uint32_t state;
    float elapsedSeconds;
    uint32_t frames;
    const char* lastCsvPath;  /* UTF-8, empty until a capture finished; lives until the next call */
    const char* lastJsonPath;
    const char* baselinePath;
} B21UI_CaptureStatus;

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
    /* Returns the total count; writes up to capacity entries. Labels live for the host's lifetime. */
    uint32_t (*settingsPanels)(B21UI_SettingsPanel* panels, uint32_t capacity);
    uint32_t (*openSettings)(B21UI_ClientId id);
    const char* (*settingsCategory)(B21UI_ClientId id);
    uint32_t (*registerKeybindings)(const B21UI_KeybindingProvider* provider);
    /* Framework 14. Registering an existing id returns the same handle. Push is any-thread; values
       pushed between two Presents belong to the frame that the second Present ends. */
    B21UI_MetricId (*perfRegisterMetric)(const B21UI_MetricDesc* desc);
    void (*perfPush)(B21UI_MetricId id, double value);
    void (*perfPushText)(B21UI_MetricId id, const char* utf8);
    /* B21UI_PERF_* bits; zero means nobody is looking and providers must do no work. */
    uint32_t (*perfActive)(void);
    /* Returns the total count; writes up to capacity entries. */
    uint32_t (*perfMetrics)(B21UI_MetricInfo* out, uint32_t capacity);
    /* Oldest first; returns the count written. */
    uint32_t (*perfHistory)(B21UI_MetricId id, float* out, uint32_t capacity);
    void (*perfGetSettings)(B21UI_PerfSettings* out);
    void (*perfSetSettings)(const B21UI_PerfSettings* settings);
    void (*perfSetPinned)(B21UI_MetricId id, uint32_t pinned);
    /* A page that is drawn this frame keeps B21UI_PERF_PAGE set for about a second. */
    void (*perfPageVisible)(void);
    uint32_t (*perfCaptureStart)(void);
    void (*perfCaptureStop)(void);
    void (*perfCaptureStatus)(B21UI_CaptureStatus* out);
    uint32_t (*perfLoadBaseline)(const char* jsonPathUtf8);
    void (*perfClearBaseline)(void);
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
