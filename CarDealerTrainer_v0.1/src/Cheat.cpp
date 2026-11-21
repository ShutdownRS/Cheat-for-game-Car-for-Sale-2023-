// Car For Sale Simulator 2023 - fun trainer (single player only)
// Unity IL2CPP v39 build. All game interaction goes through exported il2cpp_* API,
// so no hardcoded RVA offsets are needed for fields/methods.
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#pragma comment(lib, "libMinHook.x64.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ---------------------------------------------------------------------------
// IL2CPP ABI
// ---------------------------------------------------------------------------
struct Vec3 { float x, y, z; };

using FnDomainGet = void* (*)();
using FnDomainGetAssemblies = void** (*)(void*, size_t*);
using FnAssemblyGetImage = void* (*)(void*);
using FnImageGetName = const char* (*)(void*);
using FnClassFromName = void* (*)(void*, const char*, const char*);
using FnClassGetParent = void* (*)(void*);
using FnClassGetField = void* (*)(void*, const char*);
using FnFieldGetOffset = int (*)(void*);
using FnFieldStaticSet = void (*)(void*, void*);
using FnFieldStaticGet = void (*)(void*, void*);
using FnClassGetMethod = void* (*)(void*, const char*, int);
using FnRuntimeInvoke = void* (*)(void*, void*, void**, void**);
using FnStringNew = void* (*)(const char*);
using FnClassGetType = void* (*)(void*);
using FnTypeGetObject = void* (*)(void*);
using FnArrayLength = int (*)(void*);

struct Il2cppApi {
    HMODULE module = nullptr;
    FnDomainGet domainGet = nullptr;
    FnDomainGetAssemblies domainGetAssemblies = nullptr;
    FnAssemblyGetImage assemblyGetImage = nullptr;
    FnImageGetName imageGetName = nullptr;
    FnClassFromName classFromName = nullptr;
    FnClassGetParent classGetParent = nullptr;
    FnClassGetField classGetField = nullptr;
    FnFieldGetOffset fieldGetOffset = nullptr;
    FnFieldStaticSet fieldStaticSet = nullptr;
    FnFieldStaticGet fieldStaticGet = nullptr;
    FnClassGetMethod classGetMethod = nullptr;
    FnRuntimeInvoke runtimeInvoke = nullptr;
    FnStringNew stringNew = nullptr;
    FnClassGetType classGetType = nullptr;
    FnTypeGetObject typeGetObject = nullptr;
    FnArrayLength arrayLength = nullptr;

    bool Load() {
        module = GetModuleHandleA("GameAssembly.dll");
        if (!module) return false;
        auto g = [&](const char* n) { return GetProcAddress(module, n); };
        domainGet = (FnDomainGet)g("il2cpp_domain_get");
        domainGetAssemblies = (FnDomainGetAssemblies)g("il2cpp_domain_get_assemblies");
        assemblyGetImage = (FnAssemblyGetImage)g("il2cpp_assembly_get_image");
        imageGetName = (FnImageGetName)g("il2cpp_image_get_name");
        classFromName = (FnClassFromName)g("il2cpp_class_from_name");
        classGetParent = (FnClassGetParent)g("il2cpp_class_get_parent");
        classGetField = (FnClassGetField)g("il2cpp_class_get_field_from_name");
        fieldGetOffset = (FnFieldGetOffset)g("il2cpp_field_get_offset");
        fieldStaticSet = (FnFieldStaticSet)g("il2cpp_field_static_set_value");
        fieldStaticGet = (FnFieldStaticGet)g("il2cpp_field_static_get_value");
        classGetMethod = (FnClassGetMethod)g("il2cpp_class_get_method_from_name");
        runtimeInvoke = (FnRuntimeInvoke)g("il2cpp_runtime_invoke");
        stringNew = (FnStringNew)g("il2cpp_string_new");
        classGetType = (FnClassGetType)g("il2cpp_class_get_type");
        typeGetObject = (FnTypeGetObject)g("il2cpp_type_get_object");
        arrayLength = (FnArrayLength)g("il2cpp_array_length");
        return domainGet && domainGetAssemblies && assemblyGetImage && classFromName &&
               classGetField && fieldGetOffset && fieldStaticSet && classGetMethod &&
               runtimeInvoke && typeGetObject && arrayLength;
    }
};
static Il2cppApi il2cpp;

static void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf_s(buf, fmt, ap);
    va_end(ap);
    HANDLE f = CreateFileA("CFS23_cheat.log", GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        SetFilePointer(f, 0, nullptr, FILE_END);
        DWORD written = 0;
        WriteFile(f, buf, (DWORD)strlen(buf), &written, nullptr);
        CloseHandle(f);
    }
}

// ---------------------------------------------------------------------------
// Reflection cache
// ---------------------------------------------------------------------------
struct ClassInfo {
    void* klass = nullptr;
    int fSpeed = -1, fJumpHeight = -1, fRb = -1, fJumpReq = -1, fGrounded = -1;
    int fViewYaw = -1, fSmoothedYaw = -1;
    void* cashField = nullptr;
};
static ClassInfo g_game;
static void* g_cameraClass = nullptr, *g_transformClass = nullptr, *g_rbClass = nullptr;
static void* g_componentClass = nullptr, *g_objectClass = nullptr, *g_timeClass = nullptr;
static void* g_renderSettingsClass = nullptr, *g_carClass = nullptr;
static void* g_cashManagerClass = nullptr;
static bool g_ready = false;

static bool g_loggedAssemblies = false;

static void* FindClass(const char* assembly, const char* ns, const char* name) {
    if (!il2cpp.domainGetAssemblies) return nullptr;
    size_t n = 0;
    void** assemblies = il2cpp.domainGetAssemblies(il2cpp.domainGet(), &n);
    bool logOnce = !g_loggedAssemblies;
    if (logOnce) Log("domain assemblies=%zu\n", n);
    for (size_t i = 0; i < n; ++i) {
        void* image = il2cpp.assemblyGetImage(assemblies[i]);
        const char* imgName = il2cpp.imageGetName ? il2cpp.imageGetName(image) : nullptr;
        if (!imgName) continue;
        if (logOnce && i < 60) Log("  asm[%zu] = %s\n", i, imgName);
        // Accept both "Assembly-CSharp" and "Assembly-CSharp.dll" style names.
        if (_strnicmp(imgName, assembly, strlen(assembly)) != 0) continue;
        void* c = il2cpp.classFromName(image, ns, name);
        if (c) { Log("  found %s in %s\n", name, imgName); return c; }
    }
    g_loggedAssemblies = true;
    return nullptr;
}

static int FieldOffset(void* klass, const char* name) {
    void* f = il2cpp.classGetField(klass, name);
    if (!f && il2cpp.classGetParent) {
        void* p = il2cpp.classGetParent(klass);
        while (p) {
            f = il2cpp.classGetField(p, name);
            if (f) break;
            p = il2cpp.classGetParent(p);
        }
    }
    return f ? il2cpp.fieldGetOffset(f) : -1;
}

static void* GetMethod(void* klass, const char* name, int argc) {
    return klass && il2cpp.classGetMethod ? il2cpp.classGetMethod(klass, name, argc) : nullptr;
}

// The first member of MethodInfo is the native methodPointer on IL2CPP v39.
static void* MethodPointer(void* klass, const char* name, int argc) {
    void* mi = GetMethod(klass, name, argc);
    return mi ? *(void**)mi : nullptr;
}

static void* Invoke(void* method, void* self, std::vector<void*> args) {
    if (!method || !il2cpp.runtimeInvoke) return nullptr;
    void* exc = nullptr;
    void** arr = args.empty() ? nullptr : args.data();
    return il2cpp.runtimeInvoke(method, self, arr, &exc);
}

static void* Call(void* klass, const char* name, int argc, void* self, std::vector<void*> args) {
    return Invoke(GetMethod(klass, name, argc), self, std::move(args));
}

static Vec3 CallVec3(void* klass, const char* name, void* self) {
    Vec3 result{};
    void* ret = Call(klass, name, 0, self, {});
    if (ret) std::memcpy(&result, ret, sizeof(Vec3));
    return result;
}

static bool CallBool(void* klass, const char* name, void* self) {
    void* ret = Call(klass, name, 0, self, {});
    return ret ? (*(unsigned char*)ret != 0) : false;
}

static float CallFloat(void* klass, const char* name, void* self) {
    void* ret = Call(klass, name, 0, self, {});
    return ret ? *(float*)ret : 0.f;
}

// ---------------------------------------------------------------------------
// Shared state between overlay/input threads and game hooks
// ---------------------------------------------------------------------------
struct Config {
    std::atomic<bool> menuOpen{false};
    std::atomic<bool> bhop{false};
    std::atomic<bool> surf{false};
    std::atomic<bool> autoStrafe{false};
    std::atomic<bool> footSpeed{false};
    std::atomic<bool> fly{false};
    std::atomic<bool> esp{false};
    std::atomic<bool> noFog{false};
    std::atomic<int> carSpeedIdx{0};
    std::atomic<int> footSpeedIdx{0};
    std::atomic<int> fovIdx{0};
    std::atomic<int> timeIdx{0};
    std::atomic<bool> requestMoney{false};
    std::atomic<bool> moneyApplied{false};
    std::atomic<double> cash{0.0};
};
static Config cfg;

struct EspItem { float x, y, w, h, dist; };
static std::vector<EspItem> g_esp;
static std::mutex g_espMutex;

static const int CAR_SPEEDS[] = {1, 2, 5, 10, 25, 50};
static const float FOVS[] = {0.f, 80.f, 95.f, 110.f, 130.f};
static const float TIME_SCALES[] = {1.f, 0.5f, 2.f, 0.1f};
static const float FOOT_MULTS[] = {1.f, 1.8f, 3.f, 6.f, 12.f};

static float g_originalSpeed = 0.f;
static float g_originalJump = 0.f;
static bool g_originalKinematic = false;
static bool g_originalGravity = true;
static bool g_flyWasOn = false;
static float g_originalFov = 0.f;

// ---------------------------------------------------------------------------
// Helpers operating on managed objects
// ---------------------------------------------------------------------------
static char* FieldPtr(void* obj, int offset) {
    return obj && offset >= 0 ? (char*)obj + offset : nullptr;
}

static void* GetPlayerRigidbody(void* fpm) {
    char* p = FieldPtr(fpm, g_game.fRb);
    return p ? *(void**)p : nullptr;
}

static void SetRbVelocity(void* rb, Vec3 v) {
    Call(g_rbClass, "set_velocity", 1, rb, {&v});
}

static void SetRbPosition(void* rb, Vec3 v) {
    Call(g_rbClass, "set_position", 1, rb, {&v});
}

static void ApplyFoot(void* self) {
    if (!g_ready || !self) return;
    if (g_originalSpeed <= 0.f && g_game.fSpeed >= 0) {
        char* p = FieldPtr(self, g_game.fSpeed);
        if (p && *(float*)p > 0.f) g_originalSpeed = *(float*)p;
    }
    if (g_originalJump <= 0.f && g_game.fJumpHeight >= 0) {
        char* p = FieldPtr(self, g_game.fJumpHeight);
        if (p && *(float*)p > 0.f) g_originalJump = *(float*)p;
    }
    if (cfg.footSpeed.load()) {
        float mult = FOOT_MULTS[cfg.footSpeedIdx.load() % 5];
        char* p = FieldPtr(self, g_game.fSpeed);
        if (p && g_originalSpeed > 0.f) *(float*)p = g_originalSpeed * mult;
    } else if (g_originalSpeed > 0.f) {
        char* p = FieldPtr(self, g_game.fSpeed);
        if (p) *(float*)p = g_originalSpeed;
    }

    if (cfg.bhop.load()) {
        char* p = FieldPtr(self, g_game.fJumpReq);
        if (p) *p = 1;
    }

    if (cfg.autoStrafe.load()) {
        void* rb = GetPlayerRigidbody(self);
        if (rb) {
            Vec3 v = CallVec3(g_rbClass, "get_velocity", rb);
            float horizontal = v.x * v.x + v.z * v.z;
            if (horizontal > 1.f) {
                float yaw = std::atan2(v.x, v.z) * 57.2957795f;
                char* y1 = FieldPtr(self, g_game.fViewYaw);
                char* y2 = FieldPtr(self, g_game.fSmoothedYaw);
                if (y1) *(float*)y1 = yaw;
                if (y2) *(float*)y2 = yaw;
            }
        }
    }
}

static void ApplyFootFixed(void* self) {
    if (!g_ready || !self) return;
    void* rb = GetPlayerRigidbody(self);
    if (!rb) return;

    if (cfg.bhop.load()) {
        char* groundedPtr = FieldPtr(self, g_game.fGrounded);
        bool grounded = groundedPtr ? (*groundedPtr != 0) : true;
        if (grounded) {
            Vec3 v = CallVec3(g_rbClass, "get_velocity", rb);
            float jump = g_originalJump > 0.f ? g_originalJump : 6.5f;
            v.y = std::sqrt(2.f * 9.81f * jump) * 1.05f;
            SetRbVelocity(rb, v);
        }
    }

    if (cfg.surf.load()) {
        Vec3 v = CallVec3(g_rbClass, "get_velocity", rb);
        float hs = std::sqrt(v.x * v.x + v.z * v.z);
        if (hs > 2.f && hs < 40.f) {
            float boost = 1.025f;
            v.x *= boost;
            v.z *= boost;
            SetRbVelocity(rb, v);
        }
    }
}

static void ApplyFly(void* self) {
    if (!g_ready || !self) return;
    char* rbPtr = FieldPtr(self, g_game.fRb);
    void* rb = rbPtr ? *(void**)rbPtr : nullptr;
    if (!rb) return;

    if (cfg.fly.load()) {
        if (!g_flyWasOn) {
            g_originalKinematic = CallBool(g_rbClass, "get_isKinematic", rb);
            g_originalGravity = CallBool(g_rbClass, "get_useGravity", rb);
            g_flyWasOn = true;
        }
        unsigned char on = 1;
        Call(g_rbClass, "set_isKinematic", 1, rb, {&on});
        Call(g_rbClass, "set_useGravity", 1, rb, {&on});

        void* tr = Call(g_componentClass, "get_transform", 0, rb, {});
        if (!tr) return;
        Vec3 pos = CallVec3(g_transformClass, "get_position", tr);
        Vec3 fwd = CallVec3(g_transformClass, "get_forward", tr);
        Vec3 right = CallVec3(g_transformClass, "get_right", tr);
        const float step = 1.4f;
        if (GetAsyncKeyState('W') & 0x8000) { pos.x += fwd.x * step; pos.y += fwd.y * step; pos.z += fwd.z * step; }
        if (GetAsyncKeyState('S') & 0x8000) { pos.x -= fwd.x * step; pos.y -= fwd.y * step; pos.z -= fwd.z * step; }
        if (GetAsyncKeyState('D') & 0x8000) { pos.x += right.x * step; pos.y += right.y * step; pos.z += right.z * step; }
        if (GetAsyncKeyState('A') & 0x8000) { pos.x -= right.x * step; pos.y -= right.y * step; pos.z -= right.z * step; }
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) pos.y += step;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) pos.y -= step;
        SetRbPosition(rb, pos);
        return;
    }

    if (g_flyWasOn) {
        unsigned char kin = g_originalKinematic ? 1 : 0;
        unsigned char grav = g_originalGravity ? 1 : 0;
        Call(g_rbClass, "set_isKinematic", 1, rb, {&kin});
        Call(g_rbClass, "set_useGravity", 1, rb, {&grav});
        g_flyWasOn = false;
    }

    int idx = cfg.carSpeedIdx.load();
    if (idx > 0) {
        float mult = (float)CAR_SPEEDS[idx];
        Vec3 v = CallVec3(g_rbClass, "get_velocity", rb);
        float hs = std::sqrt(v.x * v.x + v.z * v.z);
        if (hs > 1.f && hs < 120.f) {
            v.x *= mult;
            v.z *= mult;
            float cap = 35.f * mult;
            float nh = std::sqrt(v.x * v.x + v.z * v.z);
            if (nh > cap) { float s = cap / nh; v.x *= s; v.z *= s; }
            SetRbVelocity(rb, v);
        }
    }
}

static void ApplyVisuals() {
    if (!g_ready) return;
    void* cam = Call(g_cameraClass, "get_main", 0, nullptr, {});
    if (cam) {
        int fi = cfg.fovIdx.load();
        if (g_originalFov <= 0.f) {
            float cur = CallFloat(g_cameraClass, "get_fieldOfView", cam);
            if (cur > 0.f) g_originalFov = cur;
        }
        if (fi > 0) {
            float fov = FOVS[fi];
            Call(g_cameraClass, "set_fieldOfView", 1, cam, {&fov});
        } else if (g_originalFov > 0.f) {
            float fov = g_originalFov;
            Call(g_cameraClass, "set_fieldOfView", 1, cam, {&fov});
        }
        float farPlane = 3000.f;
        Call(g_cameraClass, "set_farClipPlane", 1, cam, {&farPlane});
    }
    unsigned char fog = cfg.noFog.load() ? 0 : 1;
    Call(g_renderSettingsClass, "set_fog", 1, nullptr, {&fog});
    float ts = TIME_SCALES[cfg.timeIdx.load() % 4];
    Call(g_timeClass, "set_timeScale", 1, nullptr, {&ts});
}

static void CollectEsp() {
    if (!g_ready || !g_objectClass || !g_carClass) return;
    void* typeObj = il2cpp.typeGetObject(il2cpp.classGetType(g_carClass));
    if (!typeObj) return;
    void* arr = Call(g_objectClass, "FindObjectsOfType", 1, nullptr, {&typeObj});
    if (!arr) return;

    void* cam = Call(g_cameraClass, "get_main", 0, nullptr, {});
    if (!cam) return;
    Vec3 camPos = Vec3{};
    void* camTr = Call(g_componentClass, "get_transform", 0, cam, {});
    if (camTr) camPos = CallVec3(g_transformClass, "get_position", camTr);

    std::vector<EspItem> items;
    int count = il2cpp.arrayLength(arr);
    char* data = (char*)arr + 0x20; // Il2CppArray header on x64
    for (int i = 0; i < count && i < 64; ++i) {
        void* car = *(void**)(data + (size_t)i * 8);
        if (!car) continue;
        char* rbPtr = FieldPtr(car, g_game.fRb);
        if (!rbPtr) continue;
        void* rb = *(void**)rbPtr;
        if (!rb) continue;
        Vec3 pos = CallVec3(g_rbClass, "get_position", rb);
        float dx = pos.x - camPos.x, dy = pos.y - camPos.y, dz = pos.z - camPos.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        void* ret = Call(g_cameraClass, "WorldToScreenPoint", 1, cam, {&pos});
        if (!ret) continue;
        Vec3 scr{};
        std::memcpy(&scr, ret, sizeof(Vec3));
        if (scr.z <= 0.f || scr.x < -200.f || scr.y < -200.f) continue;
        float size = 14000.f / (dist + 10.f);
        if (size < 10.f) size = 10.f;
        if (size > 220.f) size = 220.f;
        items.push_back({scr.x, scr.y, size, size * 0.6f, dist});
    }
    {
        std::lock_guard<std::mutex> lock(g_espMutex);
        g_esp = std::move(items);
    }
}

// Static field writes are safe from any thread, unlike Unity API calls.
// This lets the money cheat work even in the main menu where no gameplay
// hooks are ticking.
static void ApplyMoneyThread() {
    if (!g_ready || !g_game.cashField) return;

    if (cfg.requestMoney.exchange(false)) {
        double value = 999999999.0;
        double before = 0;
        if (il2cpp.fieldStaticGet) il2cpp.fieldStaticGet(g_game.cashField, &before);
        if (il2cpp.fieldStaticSet) {
            il2cpp.fieldStaticSet(g_game.cashField, &value);
            cfg.cash.store(value);
            cfg.moneyApplied.store(true);
        }
        Log("money: %.0f -> %.0f\n", before, value);
    } else if (il2cpp.fieldStaticGet) {
        double value = 0;
        il2cpp.fieldStaticGet(g_game.cashField, &value);
        cfg.cash.store(value);
    }
}

// Called from the game hook: refresh UI displayers once after the static
// field was changed from the background thread.
static void ApplyMoney() {
    if (!g_ready) return;
    if (cfg.moneyApplied.exchange(false)) {
        Call(g_cashManagerClass, "SetDisplayers", 0, nullptr, {});
    }
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------
using UnityFn = void(__fastcall*)(void*, void*);
static UnityFn oFpmUpdate = nullptr, oFpmFixed = nullptr, oCarFixed = nullptr, oCarUpdate = nullptr;

static void __fastcall hkFpmUpdate(void* self, void* m) {
    ApplyFoot(self);
    if (oFpmUpdate) oFpmUpdate(self, m);
}
static void __fastcall hkFpmFixed(void* self, void* m) {
    if (oFpmFixed) oFpmFixed(self, m);
    ApplyFootFixed(self);
}
static void __fastcall hkCarFixed(void* self, void* m) {
    if (oCarFixed) oCarFixed(self, m);
    ApplyFly(self);
}
static void __fastcall hkCarUpdate(void* self, void* m) {
    if (oCarUpdate) oCarUpdate(self, m);
    ApplyMoney();
    ApplyVisuals();
    if (cfg.esp.load()) CollectEsp();
    else { std::lock_guard<std::mutex> lock(g_espMutex); g_esp.clear(); }
    if (g_game.cashField && il2cpp.fieldStaticGet) {
        double value = 0;
        il2cpp.fieldStaticGet(g_game.cashField, &value);
        cfg.cash.store(value);
    }
}

// ---------------------------------------------------------------------------
// Overlay + input
// ---------------------------------------------------------------------------
static const char* MENU_ITEMS[] = {
    "  Infinite money (999 999 999)",
    "  BHop / auto jump",
    "  Surf boost",
    "  Auto strafe",
    "  Foot speed",
    "  Car speed",
    "  Fly / noclip",
    "  ESP cars",
    "  No fog",
    "  FOV",
    "  Time scale",
};
static const int MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static int g_selected = 0;

static void ToggleItem(int i) {
    Log("menu toggle item %d\n", i);
    switch (i) {
    case 0: cfg.requestMoney.store(true); break;
    case 1: cfg.bhop.store(!cfg.bhop.load()); break;
    case 2: cfg.surf.store(!cfg.surf.load()); break;
    case 3: cfg.autoStrafe.store(!cfg.autoStrafe.load()); break;
    case 4: {
        bool on = !cfg.footSpeed.load();
        cfg.footSpeed.store(on);
        if (!on) cfg.footSpeedIdx.store(0);
        break;
    }
    case 5: cfg.carSpeedIdx.store((cfg.carSpeedIdx.load() + 1) % 6); break;
    case 6: cfg.fly.store(!cfg.fly.load()); break;
    case 7: cfg.esp.store(!cfg.esp.load()); break;
    case 8: cfg.noFog.store(!cfg.noFog.load()); break;
    case 9: cfg.fovIdx.store((cfg.fovIdx.load() + 1) % 5); break;
    case 10: cfg.timeIdx.store((cfg.timeIdx.load() + 1) % 4); break;
    }
}

static DWORD WINAPI InputThread(LPVOID) {
    bool prevInsert = false, prevUp = false, prevDown = false, prevEnter = false;
    while (true) {
        bool ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (ins && !prevInsert) {
            bool open = !cfg.menuOpen.load();
            cfg.menuOpen.store(open);
            Log("menu %s\n", open ? "OPEN" : "CLOSE");
        }
        prevInsert = ins;

        if (cfg.menuOpen.load()) {
            bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
            bool down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
            bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
            if (up && !prevUp) g_selected = (g_selected - 1 + MENU_COUNT) % MENU_COUNT;
            if (down && !prevDown) g_selected = (g_selected + 1) % MENU_COUNT;
            if (enter && !prevEnter) ToggleItem(g_selected);
            prevUp = up; prevDown = down; prevEnter = enter;
        }
        Sleep(25);
    }
}

static void DrawOverlay(HDC hdc, int w, int h) {
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontA(17, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH, "Consolas");
    HGDIOBJ oldFont = SelectObject(hdc, font);

    // ESP
    if (cfg.esp.load()) {
        std::vector<EspItem> items;
        { std::lock_guard<std::mutex> lock(g_espMutex); items = g_esp; }
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 255, 160));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
        HGDIOBJ oldBrush = SelectObject(hdc, nullBrush);
        for (const auto& e : items) {
            int x = (int)(e.x - e.w * 0.5f);
            int y = (int)(e.y - e.h * 0.5f);
            Rectangle(hdc, x, y, (int)(x + e.w), (int)(y + e.h));
            char txt[64];
            sprintf_s(txt, "CAR  %.0fm", e.dist);
            SetTextColor(hdc, RGB(0, 255, 160));
            TextOutA(hdc, x, y - 20, txt, (int)strlen(txt));
        }
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
    }

    if (cfg.menuOpen.load()) {
        int mx = 30, my = 30, mw = 420, mh = 40 + MENU_COUNT * 26;
        HBRUSH bg = CreateSolidBrush(RGB(18, 18, 24));
        RECT r{mx, my, mx + mw, my + mh};
        FillRect(hdc, &r, bg);
        DeleteObject(bg);
        HPEN border = CreatePen(PS_SOLID, 2, RGB(0, 200, 255));
        HGDIOBJ oldPen = SelectObject(hdc, border);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
        HGDIOBJ oldBrush = SelectObject(hdc, nb);
        Rectangle(hdc, mx, my, mx + mw, my + mh);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(border);

        SetTextColor(hdc, RGB(0, 220, 255));
        TextOutA(hdc, mx + 14, my + 10, "CAR DEALER TRAINER  [INSERT]", 29);

        char cashTxt[64];
        sprintf_s(cashTxt, "Cash: %.0f", cfg.cash.load());
        SetTextColor(hdc, RGB(255, 210, 0));
        TextOutA(hdc, mx + mw - 190, my + 10, cashTxt, (int)strlen(cashTxt));

        for (int i = 0; i < MENU_COUNT; ++i) {
            bool on = false;
            char value[32] = "";
            switch (i) {
            case 1: on = cfg.bhop.load(); break;
            case 2: on = cfg.surf.load(); break;
            case 3: on = cfg.autoStrafe.load(); break;
            case 4: on = cfg.footSpeed.load(); sprintf_s(value, " x%.1f", FOOT_MULTS[cfg.footSpeedIdx.load() % 5]); break;
            case 5: on = cfg.carSpeedIdx.load() > 0; sprintf_s(value, " x%d", CAR_SPEEDS[cfg.carSpeedIdx.load()]); break;
            case 6: on = cfg.fly.load(); break;
            case 7: on = cfg.esp.load(); break;
            case 8: on = cfg.noFog.load(); break;
            case 9: on = cfg.fovIdx.load() > 0; sprintf_s(value, " %.0f", FOVS[cfg.fovIdx.load()]); break;
            case 10: on = cfg.timeIdx.load() != 0; sprintf_s(value, " x%.1f", TIME_SCALES[cfg.timeIdx.load() % 4]); break;
            }
            char line[128];
            sprintf_s(line, "%s%s%s", on ? "  [X] " : "  [ ] ", MENU_ITEMS[i] + 2, value);
            if (i == g_selected) {
                HBRUSH hl = CreateSolidBrush(RGB(35, 45, 60));
                RECT hr{mx + 4, my + 34 + i * 26, mx + mw - 4, my + 58 + i * 26};
                FillRect(hdc, &hr, hl);
                DeleteObject(hl);
            }
            SetTextColor(hdc, i == g_selected ? RGB(255, 255, 255) : RGB(160, 170, 185));
            TextOutA(hdc, mx + 14, my + 36 + i * 26, line, (int)strlen(line));
        }
        SetTextColor(hdc, RGB(120, 130, 145));
        TextOutA(hdc, mx + 14, my + mh - 22, "Arrows = select | Enter = toggle", 33);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

static DWORD WINAPI OverlayThread(LPVOID) {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "CarDealerTrainerOverlay";
    RegisterClassExA(&wc);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                                WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                wc.lpszClassName, "overlay", WS_POPUP,
                                0, 0, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_SHOW);

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    POINT pt{0, 0};
    SIZE sz{w, h};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = 0;

    Log("overlay window created %dx%d\n", w, h);
    MSG msg{};
    while (true) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (bits) std::memset(bits, 0, (size_t)w * h * 4);
        DrawOverlay(mem, w, h);
        UpdateLayeredWindow(hwnd, screen, &pt, &sz, mem, &pt, RGB(0, 0, 0), &blend, ULW_COLORKEY);
        Sleep(16);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
static bool InitReflection() {
    if (!il2cpp.Load()) return false;

    g_game.klass = FindClass("Assembly-CSharp", "", "FirstPersonMovement");
    g_carClass = FindClass("Assembly-CSharp", "PG", "CarController");
    g_cashManagerClass = FindClass("Assembly-CSharp", "", "CashManager");
    g_cameraClass = FindClass("UnityEngine.CoreModule", "UnityEngine", "Camera");
    g_transformClass = FindClass("UnityEngine.CoreModule", "UnityEngine", "Transform");
    g_componentClass = FindClass("UnityEngine.CoreModule", "UnityEngine", "Component");
    g_objectClass = FindClass("UnityEngine.CoreModule", "UnityEngine", "Object");
    g_timeClass = FindClass("UnityEngine.CoreModule", "UnityEngine", "Time");
    g_renderSettingsClass = FindClass("UnityEngine.CoreModule", "UnityEngine", "RenderSettings");
    g_rbClass = FindClass("UnityEngine.PhysicsModule", "UnityEngine", "Rigidbody");

    if (!g_game.klass || !g_carClass || !g_cashManagerClass || !g_cameraClass ||
        !g_transformClass || !g_rbClass || !g_objectClass) return false;

    g_game.fSpeed = FieldOffset(g_game.klass, "speed");
    g_game.fJumpHeight = FieldOffset(g_game.klass, "jumpHeight");
    g_game.fRb = FieldOffset(g_carClass, "<RigidBody>k__BackingField");
    g_game.fJumpReq = FieldOffset(g_game.klass, "_jumpRequested");
    g_game.fGrounded = FieldOffset(g_game.klass, "_isGrounded");
    g_game.fViewYaw = FieldOffset(g_game.klass, "_viewYaw");
    g_game.fSmoothedYaw = FieldOffset(g_game.klass, "_smoothedViewYaw");

    void* cashField = il2cpp.classGetField(g_cashManagerClass, "cash");
    g_game.cashField = cashField;

    Log("classes: fpm=%p car=%p cash=%p cam=%p rb=%p obj=%p\n",
        g_game.klass, g_carClass, g_cashManagerClass, g_cameraClass, g_rbClass, g_objectClass);
    Log("fields: speed=%d jump=%d rb=%d jumpReq=%d grounded=%d yaw=%d yawSm=%d cash=%p\n",
        g_game.fSpeed, g_game.fJumpHeight, g_game.fRb, g_game.fJumpReq,
        g_game.fGrounded, g_game.fViewYaw, g_game.fSmoothedYaw, g_game.cashField);
    return g_game.fSpeed >= 0 && g_game.fRb >= 0 && g_game.cashField != nullptr;
}

// RVAs for the exact build shipped with these files. They can be overridden
// from offsets.txt (one hex RVA per line) after a game update.
static uintptr_t g_rvaFpmUpdate = 0x674190;
static uintptr_t g_rvaFpmFixed = 0x673630;
static uintptr_t g_rvaCarFixed = 0x7871C0;
static uintptr_t g_rvaCarUpdate = 0x7897F0;

static void ReadOffsets() {
    FILE* f = fopen("Cheat/offsets.txt", "r");
    if (!f) return;
    uintptr_t* targets[] = {&g_rvaFpmUpdate, &g_rvaFpmFixed, &g_rvaCarFixed, &g_rvaCarUpdate};
    size_t idx = 0;
    char line[256];
    while (idx < 4 && fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0) continue;
        unsigned long long v = 0;
        if (sscanf_s(p, "%llx", &v) == 1 && v > 0x1000 && v < 0x9000000) {
            *targets[idx++] = (uintptr_t)v;
        }
    }
    fclose(f);
    Log("offsets loaded: %llX %llX %llX %llX\n",
        (unsigned long long)g_rvaFpmUpdate, (unsigned long long)g_rvaFpmFixed,
        (unsigned long long)g_rvaCarFixed, (unsigned long long)g_rvaCarUpdate);
}

static bool InstallHooks() {
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) return false;
    ReadOffsets();
    uintptr_t base = (uintptr_t)il2cpp.module;

    Log("api ptrs: FPM.Update=%p FPM.Fixed=%p Car.Fixed=%p Car.Update=%p\n",
        MethodPointer(g_game.klass, "Update", 0),
        MethodPointer(g_game.klass, "FixedUpdate", 0),
        MethodPointer(g_carClass, "FixedUpdate", 0),
        MethodPointer(g_carClass, "Update", 0));

    void* fpUpd = (void*)(base + g_rvaFpmUpdate);
    void* fpFix = (void*)(base + g_rvaFpmFixed);
    void* carFix = (void*)(base + g_rvaCarFixed);
    void* carUpd = (void*)(base + g_rvaCarUpdate);

    if (MH_CreateHook(fpUpd, &hkFpmUpdate, (void**)&oFpmUpdate) != MH_OK) return false;
    if (MH_CreateHook(fpFix, &hkFpmFixed, (void**)&oFpmFixed) != MH_OK) return false;
    if (MH_CreateHook(carFix, &hkCarFixed, (void**)&oCarFixed) != MH_OK) return false;
    if (MH_CreateHook(carUpd, &hkCarUpdate, (void**)&oCarUpdate) != MH_OK) return false;
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}

static DWORD WINAPI MoneyThread(LPVOID) {
    while (true) {
        ApplyMoneyThread();
        Sleep(100);
    }
    return 0;
}

static DWORD WINAPI InitThread(LPVOID) {
    Log("InitThread started\n");
    // Wait until the game runtime is fully up.
    for (int i = 0; i < 100; ++i) {
        if (GetModuleHandleA("GameAssembly.dll")) break;
        Sleep(200);
    }
    Log("GameAssembly handle=%p\n", GetModuleHandleA("GameAssembly.dll"));
    if (!InitReflection()) { Log("InitReflection FAILED\n"); return 0; }
    g_ready = true;
    Log("hooks installed: %d\n", InstallHooks());
    CreateThread(nullptr, 0, InputThread, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, OverlayThread, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, MoneyThread, nullptr, 0, nullptr);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Log("DllMain attach\n");
        HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        Log("CreateThread=%p\n", t);
    }
    return TRUE;
}
