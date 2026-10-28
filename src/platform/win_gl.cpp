// FRACTURE — WGL bootstrap: pixel format, GL3.3 core context, proc loader
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../ft/common.h"
#include "../ft/gl_min.h"

namespace ft {
// ft_getproc is defined in glbackend.cpp (shared)

static void* procGL(const char* name) {
    // core1.1 entry points live in opengl32.dll; extensions come from wgl
    HMODULE mod = GetModuleHandleA("opengl32.dll");
    if (mod) {
        void* p = (void*)GetProcAddress(mod, name);
        if (p) return p;
    }
    typedef PROC (WINAPI* wglGetProc)(LPCSTR);
    static wglGetProc fn = (wglGetProc)wglGetProcAddress("wglGetProcAddress");
    if (fn) {
        PROC p = fn(name);
        if (p && p != (PROC)0x1 && p != (PROC)0x2 && p != (PROC)0x3 && p != (PROC)-1)
            return (void*)p;
    }
    return nullptr;
}

struct WglState {
    HDC dc = nullptr;
    HGLRC rc = nullptr;
    HGLRC tmp = nullptr;
};

static bool setPixFmt(HDC dc) {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(dc, &pfd);
    if (!pf) return false;
    return SetPixelFormat(dc, pf, &pfd) == TRUE;
}

bool wglInit(HWND hwnd, WglState& st) {
    st.dc = GetDC(hwnd);
    if (!st.dc) return false;
    if (!setPixFmt(st.dc)) return false;
    // temporary legacy context to fetch modern entry points
    st.tmp = wglCreateContext(st.dc);
    if (!st.tmp) return false;
    if (!wglMakeCurrent(st.dc, st.tmp)) return false;
    ft_getproc = &procGL;
    typedef HGLRC (WINAPI* CreateCtx)(HDC, const int*);
    typedef BOOL (WINAPI* SwapInterval)(int);
    CreateCtx createCtx = (CreateCtx)procGL("wglCreateContextAttribsARB");
    SwapInterval swapInt = (SwapInterval)procGL("wglSwapIntervalEXT");
    HGLRC modern = nullptr;
    if (createCtx) {
        const int attrs[] = {
            0x2091, 3,      // WGL_CONTEXT_MAJOR_VERSION_ARB
            0x2092, 3,      // WGL_CONTEXT_MINOR_VERSION_ARB
            0x9126, 0x00000001,  // WGL_CONTEXT_PROFILE_MASK_ARB = CORE
            0, 0
        };
        modern = createCtx(st.dc, attrs);
    }
    if (modern) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(st.tmp);
        st.tmp = nullptr;
        st.rc = modern;
        if (!wglMakeCurrent(st.dc, st.rc)) return false;
    } else {
        // keep legacy context — GL3.3 shaders will fail and caller falls back
        st.rc = st.tmp;
        st.tmp = nullptr;
        ft::logf("WGL: core3.3 context unavailable, legacy context kept");
    }
    if (swapInt) swapInt(1);
    return true;
}

void wglShutdown(WglState& st) {
    if (st.rc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(st.rc); st.rc = nullptr; }
    if (st.tmp) { wglDeleteContext(st.tmp); st.tmp = nullptr; }
    if (st.dc) { ReleaseDC(WindowFromDC(st.dc), st.dc); st.dc = nullptr; }
    ft_getproc = nullptr;
}

} // namespace ft
#endif
