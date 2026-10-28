#include "web_host.h"

#include <exdisp.h>
#include <mshtmhst.h>
#include <oleauto.h>
#include <oleidl.h>
#include <shlwapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>

namespace wp {

namespace {

constexpr const wchar_t* kWebHostClass = L"WallPulseWebHostChildClass";
constexpr UINT WM_WEB_SYNC_INTERACTION = WM_APP + 42;

// ============================================================================
// Minimal Public COM Interfaces for Microsoft Edge WebView2 (Zero-DLL Loader)
// ============================================================================
struct EventRegistrationTokenWV2 {
    int64_t value;
};

struct ICoreWebView2AddScriptCompletedHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, LPCWSTR id) = 0;
};

struct ICoreWebView2ExecuteScriptCompletedHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, LPCWSTR resultJson) = 0;
};

struct ICoreWebView2 : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE get_Settings(void** settings) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR* uri) = 0;
    virtual HRESULT STDMETHODCALLTYPE Navigate(LPCWSTR uri) = 0;
    virtual HRESULT STDMETHODCALLTYPE NavigateToString(LPCWSTR htmlContent) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationStarting(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationStarting(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContentLoading(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContentLoading(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_SourceChanged(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_SourceChanged(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_HistoryChanged(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_HistoryChanged(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationCompleted(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationCompleted(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationStarting(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationStarting(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationCompleted(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationCompleted(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ScriptDialogOpening(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ScriptDialogOpening(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PermissionRequested(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PermissionRequested(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ProcessFailed(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ProcessFailed(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddScriptToExecuteOnDocumentCreated(
        LPCWSTR javaScript, ICoreWebView2AddScriptCompletedHandler* handler) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveScriptToExecuteOnDocumentCreated(LPCWSTR id) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteScript(
        LPCWSTR javaScript, ICoreWebView2ExecuteScriptCompletedHandler* handler) = 0;
};

struct ICoreWebView2Controller : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE get_IsVisible(BOOL* isVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsVisible(BOOL isVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Bounds(RECT* bounds) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Bounds(RECT bounds) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ZoomFactor(double* zoomFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ZoomFactor(double zoomFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ZoomFactorChanged(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ZoomFactorChanged(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBoundsAndZoomFactor(RECT bounds, double zoomFactor) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveFocus(int reason) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_MoveFocusRequested(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_MoveFocusRequested(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_GotFocus(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_GotFocus(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_LostFocus(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_LostFocus(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_AcceleratorKeyPressed(void*, EventRegistrationTokenWV2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_AcceleratorKeyPressed(EventRegistrationTokenWV2) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ParentWindow(HWND* parentWindow) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ParentWindow(HWND parentWindow) = 0;
    virtual HRESULT STDMETHODCALLTYPE NotifyParentWindowPositionChanged() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CoreWebView2(ICoreWebView2** coreWebView2) = 0;
};

struct ICoreWebView2ControllerCompletedHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) = 0;
};

struct ICoreWebView2Environment : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateCoreWebView2Controller(
        HWND parentWindow, ICoreWebView2ControllerCompletedHandler* handler) = 0;
};

struct ICoreWebView2EnvironmentCompletedHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) = 0;
};

static const IID kIID_WV2EnvCompleted =
    {0x4E8A3389, 0xC9D8, 0x4BD2, {0xB6, 0xB5, 0x12, 0x4F, 0xEE, 0x6C, 0xC1, 0x4D}};
static const IID kIID_WV2CtrlCompleted =
    {0x6C4819F3, 0xC9B7, 0x4260, {0x81, 0x27, 0xC9, 0xF5, 0xBD, 0xE7, 0xF6, 0x8C}};

std::wstring FindInstalledEdgeWebView2Dll() {
    constexpr const wchar_t* kSubKey =
        L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

    const HKEY roots[2] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
    for (HKEY root : roots) {
        HKEY hk = nullptr;
        if (RegOpenKeyExW(root, kSubKey, 0, KEY_READ | KEY_WOW64_32KEY, &hk) == ERROR_SUCCESS && hk) {
            wchar_t buf[MAX_PATH * 2] = {};
            DWORD cb = sizeof(buf);
            DWORD type = 0;
            LSTATUS st = RegQueryValueExW(hk, L"EBWebView", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &cb);
            RegCloseKey(hk);
            if (st == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && buf[0] != L'\0') {
                std::wstring dir = buf;
                if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
                std::wstring dll = dir + L"EBWebView\\x64\\EmbeddedBrowserWebView.dll";
                if (FileExists(dll)) return dll;
            }
        }
    }
    return {};
}

HRESULT StartWebView2Environment(PCWSTR userDataDir, ICoreWebView2EnvironmentCompletedHandler* handler) {
    // 1. Optional external WebView2Loader.dll if present
    if (HMODULE loaderMod = LoadLibraryW(L"WebView2Loader.dll")) {
        using CreateEnvFn = HRESULT(STDMETHODCALLTYPE*)(
            PCWSTR, PCWSTR, IUnknown*, ICoreWebView2EnvironmentCompletedHandler*);
        auto fn = reinterpret_cast<CreateEnvFn>(
            GetProcAddress(loaderMod, "CreateCoreWebView2EnvironmentWithOptions"));
        if (fn && SUCCEEDED(fn(nullptr, userDataDir, nullptr, handler))) {
            return S_OK;
        }
    }

    // 2. Built-in zero-DLL Evergreen WebView2 loader
    std::wstring clientDll = FindInstalledEdgeWebView2Dll();
    if (clientDll.empty()) return E_FAIL;

    HMODULE ebMod = LoadLibraryW(clientDll.c_str());
    if (!ebMod) return E_FAIL;

    using CreateInternalFn = HRESULT(STDMETHODCALLTYPE*)(
        bool, int, PCWSTR, IUnknown*, ICoreWebView2EnvironmentCompletedHandler*);
    auto fn = reinterpret_cast<CreateInternalFn>(
        GetProcAddress(ebMod, "CreateWebViewEnvironmentWithOptionsInternal"));
    if (!fn) return E_FAIL;

    return fn(true, 0 /* installed evergreen */, userDataDir, nullptr, handler);
}

// ============================================================================
// Classic OLE IWebBrowser2 Site (Fallback Engine)
// ============================================================================
class OleBrowserSite final : public IOleClientSite, public IOleInPlaceSite, public IDocHostUIHandler {
public:
    explicit OleBrowserSite(HWND hwnd) : hwnd_(hwnd) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IOleClientSite)) {
            *ppv = static_cast<IOleClientSite*>(this);
        } else if (riid == __uuidof(IOleWindow) || riid == __uuidof(IOleInPlaceSite)) {
            *ppv = static_cast<IOleInPlaceSite*>(this);
        } else if (riid == __uuidof(IDocHostUIHandler)) {
            *ppv = static_cast<IDocHostUIHandler*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE SaveObject() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer** pp) override {
        if (pp) *pp = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE ShowObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE GetWindow(HWND* phwnd) override {
        if (!phwnd) return E_POINTER;
        *phwnd = hwnd_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CanInPlaceActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnInPlaceActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUIActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetWindowContext(
        IOleInPlaceFrame** ppFrame,
        IOleInPlaceUIWindow** ppDoc,
        LPRECT lprcPosRect,
        LPRECT lprcClipRect,
        LPOLEINPLACEFRAMEINFO lpFrameInfo) override {
        if (ppFrame) *ppFrame = nullptr;
        if (ppDoc) *ppDoc = nullptr;
        if (lprcPosRect) GetClientRect(hwnd_, lprcPosRect);
        if (lprcClipRect) GetClientRect(hwnd_, lprcClipRect);
        if (lpFrameInfo) {
            lpFrameInfo->fMDIApp = FALSE;
            lpFrameInfo->hwndFrame = hwnd_;
            lpFrameInfo->haccel = nullptr;
            lpFrameInfo->cAccelEntries = 0;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Scroll(SIZE) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DiscardUndoState() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DeactivateAndUndo() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE ShowContextMenu(DWORD, POINT*, IUnknown*, IDispatch*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetHostInfo(DOCHOSTUIINFO* pInfo) override {
        if (!pInfo) return E_POINTER;
        pInfo->cbSize = sizeof(DOCHOSTUIINFO);
        pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_SCROLL_NO | DOCHOSTUIFLAG_DPI_AWARE;
        pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ShowUI(DWORD, IOleInPlaceActiveObject*, IOleCommandTarget*, IOleInPlaceFrame*, IOleInPlaceUIWindow*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE HideUI() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateUI() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResizeBorder(LPCRECT, IOleInPlaceUIWindow*, BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG, const GUID*, DWORD) override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE GetOptionKeyPath(LPOLESTR*, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDropTarget(IDropTarget*, IDropTarget**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetExternal(IDispatch** ppDispatch) override {
        if (ppDispatch) *ppDispatch = nullptr;
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE TranslateUrl(DWORD, OLECHAR*, OLECHAR**) override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE FilterDataObject(IDataObject*, IDataObject**) override { return S_FALSE; }

private:
    HWND hwnd_;
};

// Calls window.wallpulseInject(u, v, bass) via IDispatch on MSHTML document.Script
void InvokeMshtmlWallpulseInject(IWebBrowser2* browser, double u, double v, double bass) {
    if (!browser) return;
    ComPtr<IDispatch> docDisp;
    if (FAILED(browser->get_Document(docDisp.GetAddressOf())) || !docDisp) return;

    OLECHAR* scriptProp = const_cast<OLECHAR*>(L"Script");
    DISPID dispidScript = DISPID_UNKNOWN;
    if (FAILED(docDisp->GetIDsOfNames(IID_NULL, &scriptProp, 1, LOCALE_USER_DEFAULT, &dispidScript))) return;

    DISPPARAMS noArgs{};
    VARIANT varScript{};
    VariantInit(&varScript);
    if (FAILED(docDisp->Invoke(dispidScript, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_PROPERTYGET, &noArgs, &varScript, nullptr, nullptr))) {
        return;
    }

    if (varScript.vt == VT_DISPATCH && varScript.pdispVal) {
        IDispatch* winDisp = varScript.pdispVal;
        OLECHAR* fnName = const_cast<OLECHAR*>(L"wallpulseInject");
        DISPID dispidFn = DISPID_UNKNOWN;
        if (SUCCEEDED(winDisp->GetIDsOfNames(IID_NULL, &fnName, 1, LOCALE_USER_DEFAULT, &dispidFn))) {
            // IDispatch arguments are passed in reverse order!
            VARIANT args[3]{};
            args[0].vt = VT_R8; args[0].dblVal = bass;
            args[1].vt = VT_R8; args[1].dblVal = v;
            args[2].vt = VT_R8; args[2].dblVal = u;

            DISPPARAMS dp{};
            dp.rgvarg = args;
            dp.cArgs = 3;
            winDisp->Invoke(dispidFn, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dp, nullptr, nullptr, nullptr);
        }
    }
    VariantClear(&varScript);
}

}  // namespace

namespace {
void ExecuteWebViewScript(ICoreWebView2* view, LPCWSTR script);
}

struct WebWallpaperHost::Impl {
    HWND hwnd = nullptr;
    std::wstring targetUri;
    bool alive = true;
    bool oleInitialized = false;

    // Edge WebView2 (Primary Modern Engine)
    ComPtr<ICoreWebView2Environment> wv2Env;
    ComPtr<ICoreWebView2Controller> wv2Ctrl;
    ComPtr<ICoreWebView2> wv2View;
    bool usingWebView2 = false;

    // Classic MSHTML (Automatic Fallback Engine)
    OleBrowserSite site;
    ComPtr<IOleObject> oleObj;
    ComPtr<IWebBrowser2> browser;

    // Thread-safe Desktop Mouse + Audio Bridge state
    std::mutex stateMutex;
    float pendingU = 0.5f;
    float pendingV = 0.5f;
    AudioSpectrumSnapshot pendingAudio{};
    std::atomic<bool> syncQueued{false};
    ULONGLONG lastPostMs = 0;

    explicit Impl(HWND h) : hwnd(h), site(h) {}

    bool StartFallbackMshtml(int w, int h) {
        HRESULT hr = CoCreateInstance(
            CLSID_WebBrowser, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
            __uuidof(IOleObject), reinterpret_cast<void**>(oleObj.GetAddressOf()));
        if (FAILED(hr) || !oleObj) return false;

        oleObj->SetClientSite(&site);
        RECT rcClient{0, 0, w, h};
        hr = oleObj->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, &site, 0, hwnd, &rcClient);
        if (FAILED(hr) || FAILED(oleObj.As(&browser)) || !browser) return false;

        browser->put_Silent(VARIANT_TRUE);
        browser->put_Left(0);
        browser->put_Top(0);
        browser->put_Width(w);
        browser->put_Height(h);

        VARIANT vUrl{};
        vUrl.vt = VT_BSTR;
        vUrl.bstrVal = SysAllocString(targetUri.c_str());
        VARIANT vEmpty{};
        VariantInit(&vEmpty);
        browser->Navigate2(&vUrl, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
        SysFreeString(vUrl.bstrVal);
        return true;
    }

    void OnSyncInteractionOnUiThread() {
        syncQueued.store(false);
        if (!alive || !hwnd || !IsWindow(hwnd)) return;

        float u = 0.5f, v = 0.5f;
        AudioSpectrumSnapshot aud{};
        {
            std::lock_guard<std::mutex> lk(stateMutex);
            u = pendingU;
            v = pendingV;
            aud = pendingAudio;
        }

        if (usingWebView2 && wv2View) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            int px = static_cast<int>(u * std::max<LONG>(1, rc.right - rc.left));
            int py = static_cast<int>(v * std::max<LONG>(1, rc.bottom - rc.top));

            // Build stereo 64-band array for Wallpaper Engine wallpaperRegisterAudioListener + mouse event
            wchar_t script[1536];
            int pos = swprintf_s(
                script,
                L"(function(){if(window.wallpulseInject)window.wallpulseInject(%.4f,%.4f,%.3f);"
                L"try{window.dispatchEvent(new MouseEvent('mousemove',{clientX:%d,clientY:%d,bubbles:true}));}catch(e){}"
                L"if(window.__wpAudioCb){var a=[",
                u, v, aud.bass, px, py);

            if (pos > 0) {
                for (int i = 0; i < 64 && pos < 1450; ++i) {
                    float val = aud.bands[static_cast<size_t>(i % 32)];
                    pos += swprintf_s(script + pos, 1500 - static_cast<size_t>(pos),
                                      (i == 63) ? L"%.3f" : L"%.3f,", val);
                }
                swprintf_s(script + pos, 1536 - static_cast<size_t>(pos), L"];if(window.__wpAudioCb)window.__wpAudioCb(a);}})();");
                ExecuteWebViewScript(wv2View.Get(), script);
            }
        } else if (browser) {
            InvokeMshtmlWallpulseInject(browser.Get(), u, v, aud.bass);
        }
    }
};

namespace {

class WV2ScriptAddedCallback final : public ICoreWebView2AddScriptCompletedHandler {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        static const IID iid = {0xB99369F3, 0x9B11, 0x47B5, {0xBC, 0x6F, 0x8E, 0x78, 0x95, 0xFC, 0xEA, 0x17}};
        if (riid == __uuidof(IUnknown) || riid == iid) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = static_cast<ULONG>(InterlockedDecrement(&ref_)); if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT, LPCWSTR) override { return S_OK; }
private:
    long ref_ = 1;
};

class WV2ScriptExecutedCallback final : public ICoreWebView2ExecuteScriptCompletedHandler {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        static const IID iid = {0x49511172, 0xCC67, 0x4BCA, {0x99, 0x23, 0x13, 0x71, 0x12, 0xF4, 0xC4, 0xCC}};
        if (riid == __uuidof(IUnknown) || riid == iid) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = static_cast<ULONG>(InterlockedDecrement(&ref_)); if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT, LPCWSTR) override { return S_OK; }
private:
    long ref_ = 1;
};

void ExecuteWebViewScript(ICoreWebView2* view, LPCWSTR script) {
    if (!view || !script) return;
    ComPtr<WV2ScriptExecutedCallback> callback;
    callback.Attach(new WV2ScriptExecutedCallback());
    view->ExecuteScript(script, callback.Get());
}

class WV2CtrlCallback final : public ICoreWebView2ControllerCompletedHandler {
public:
    explicit WV2CtrlCallback(std::shared_ptr<WebWallpaperHost::Impl> impl) : impl_(std::move(impl)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == kIID_WV2CtrlCompleted) {
            *ppv = static_cast<ICoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = static_cast<ULONG>(InterlockedDecrement(&ref_));
        if (c == 0) delete this;
        return c;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Controller* ctrl) override {
        if (!impl_ || !impl_->alive || !impl_->hwnd || !IsWindow(impl_->hwnd)) return S_OK;
        RECT rc{};
        GetClientRect(impl_->hwnd, &rc);

        if (SUCCEEDED(hr) && ctrl) {
            impl_->wv2Ctrl = ComPtr<ICoreWebView2Controller>(ctrl);
            impl_->wv2Ctrl->put_Bounds(rc);
            impl_->wv2Ctrl->put_IsVisible(TRUE);
            if (SUCCEEDED(impl_->wv2Ctrl->get_CoreWebView2(impl_->wv2View.GetAddressOf())) && impl_->wv2View) {
                impl_->usingWebView2 = true;
                // Inject Wallpaper Engine audio listener polyfill before any user page script runs!
                ComPtr<WV2ScriptAddedCallback> added;
                added.Attach(new WV2ScriptAddedCallback());
                impl_->wv2View->AddScriptToExecuteOnDocumentCreated(
                    L"window.wallpaperRegisterAudioListener=function(cb){window.__wpAudioCb=cb;};",
                    added.Get());
                impl_->wv2View->Navigate(impl_->targetUri.c_str());
                LogInfo(L"WebWallpaperHost: Edge WebView2 Chromium controller active.");
                return S_OK;
            }
        }

        // Seamless fallback to MSHTML if WebView2 controller creation failed
        impl_->StartFallbackMshtml(std::max<int>(64, rc.right - rc.left), std::max<int>(64, rc.bottom - rc.top));
        return S_OK;
    }

private:
    long ref_ = 1;
    std::shared_ptr<WebWallpaperHost::Impl> impl_;
};

class WV2EnvCallback final : public ICoreWebView2EnvironmentCompletedHandler {
public:
    explicit WV2EnvCallback(std::shared_ptr<WebWallpaperHost::Impl> impl) : impl_(std::move(impl)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == kIID_WV2EnvCompleted) {
            *ppv = static_cast<ICoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = static_cast<ULONG>(InterlockedDecrement(&ref_));
        if (c == 0) delete this;
        return c;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment* env) override {
        if (!impl_ || !impl_->alive || !impl_->hwnd || !IsWindow(impl_->hwnd)) return S_OK;
        if (SUCCEEDED(hr) && env) {
            impl_->wv2Env = ComPtr<ICoreWebView2Environment>(env);
            ComPtr<WV2CtrlCallback> cb;
            cb.Attach(new WV2CtrlCallback(impl_));
            if (SUCCEEDED(env->CreateCoreWebView2Controller(impl_->hwnd, cb.Get()))) {
                return S_OK;
            }
        }

        RECT rc{};
        GetClientRect(impl_->hwnd, &rc);
        impl_->StartFallbackMshtml(std::max<int>(64, rc.right - rc.left), std::max<int>(64, rc.bottom - rc.top));
        return S_OK;
    }

private:
    long ref_ = 1;
    std::shared_ptr<WebWallpaperHost::Impl> impl_;
};

LRESULT CALLBACK WebHostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_WEB_SYNC_INTERACTION) {
        auto* impl = reinterpret_cast<WebWallpaperHost::Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (impl) impl->OnSyncInteractionOnUiThread();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

void EnsureHtml5BrowserFeatureFlags() {
    std::wstring exePath = GetExePath();
    size_t slash = exePath.find_last_of(L"\\/");
    std::wstring exeName = (slash == std::wstring::npos) ? exePath : exePath.substr(slash + 1);
    if (exeName.empty()) return;

    auto setDword = [&](const wchar_t* subKey, DWORD val) {
        HKEY hk = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0,
                            KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hk, exeName.c_str(), 0, REG_DWORD,
                           reinterpret_cast<const BYTE*>(&val), sizeof(val));
            RegCloseKey(hk);
        }
    };

    setDword(L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION", 11001);
    setDword(L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_GPU_RENDERING", 1);
}

WebWallpaperHost::WebWallpaperHost() = default;

WebWallpaperHost::~WebWallpaperHost() {
    Close();
}

bool WebWallpaperHost::IsUsingWebView2() const {
    return impl_ && impl_->usingWebView2;
}

bool WebWallpaperHost::AttachAndNavigate(
    HINSTANCE hInstance, HWND parentHwnd, const RECT& bounds, const std::wstring& htmlPath) {
    Close();
    if (!parentHwnd || !IsWindow(parentHwnd) || !FileExists(htmlPath)) return false;

    EnsureHtml5BrowserFeatureFlags();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WebHostWndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kWebHostClass;
    RegisterClassExW(&wc);

    int w = std::max<int>(64, bounds.right - bounds.left);
    int h = std::max<int>(64, bounds.bottom - bounds.top);

    hostHwnd_ = CreateWindowExW(
        0, kWebHostClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        bounds.left, bounds.top, w, h,
        parentHwnd, nullptr, hInstance, nullptr);
    if (!hostHwnd_) return false;

    HRESULT oleHr = OleInitialize(nullptr);
    if (FAILED(oleHr)) {
        DestroyWindow(hostHwnd_);
        hostHwnd_ = nullptr;
        return false;
    }
    impl_ = std::make_shared<Impl>(hostHwnd_);
    impl_->oleInitialized = true;
    SetWindowLongPtrW(hostHwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));

    wchar_t uriBuf[4096] = {};
    DWORD cch = 4095;
    impl_->targetUri = htmlPath;
    if (SUCCEEDED(UrlCreateFromPathW(htmlPath.c_str(), uriBuf, &cch, 0)) && uriBuf[0] != L'\0') {
        impl_->targetUri.assign(uriBuf, cch);
    }

    // Try Edge WebView2 (Chromium) first; fall back automatically to GPU MSHTML if unavailable
    std::wstring wv2DataDir = GetAppDataDir() + L"\\webview2_data";
    EnsureDirExists(wv2DataDir);

    ComPtr<WV2EnvCallback> envCb;
    envCb.Attach(new WV2EnvCallback(impl_));
    if (SUCCEEDED(StartWebView2Environment(wv2DataDir.c_str(), envCb.Get()))) {
        return true;
    }

    if (!impl_->StartFallbackMshtml(w, h)) {
        Close();
        return false;
    }
    return true;
}

void WebWallpaperHost::SetBounds(const RECT& bounds) {
    if (!hostHwnd_ || !IsWindow(hostHwnd_)) return;
    int w = std::max<int>(64, bounds.right - bounds.left);
    int h = std::max<int>(64, bounds.bottom - bounds.top);
    SetWindowPos(hostHwnd_, HWND_TOP, bounds.left, bounds.top, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (impl_) {
        if (impl_->wv2Ctrl) {
            RECT rc{0, 0, w, h};
            impl_->wv2Ctrl->put_Bounds(rc);
        }
        if (impl_->browser) {
            impl_->browser->put_Width(w);
            impl_->browser->put_Height(h);
            ComPtr<IOleInPlaceObject> inPlace;
            if (SUCCEEDED(impl_->oleObj.As(&inPlace)) && inPlace) {
                RECT rc{0, 0, w, h};
                inPlace->SetObjectRects(&rc, &rc);
            }
        }
    }
}

void WebWallpaperHost::PushDesktopInteraction(float mouseU, float mouseV, const AudioSpectrumSnapshot& audio) {
    if (!impl_ || !hostHwnd_ || !IsWindow(hostHwnd_)) return;

    ULONGLONG now = GetTickCount64();
    {
        std::lock_guard<std::mutex> lk(impl_->stateMutex);
        impl_->pendingU = mouseU;
        impl_->pendingV = mouseV;
        impl_->pendingAudio = audio;
        if (now - impl_->lastPostMs < 33) return;  // ~30 Hz smooth JS bridge rate
        impl_->lastPostMs = now;
    }

    bool expected = false;
    if (impl_->syncQueued.compare_exchange_strong(expected, true)) {
        PostMessageW(hostHwnd_, WM_WEB_SYNC_INTERACTION, 0, 0);
    }
}

void WebWallpaperHost::Close() {
    if (hostHwnd_ && IsWindow(hostHwnd_)) {
        SetWindowLongPtrW(hostHwnd_, GWLP_USERDATA, 0);
    }
    if (impl_) {
        impl_->alive = false;
        impl_->wv2View.Reset();
        if (impl_->wv2Ctrl) {
            impl_->wv2Ctrl->Close();
            impl_->wv2Ctrl.Reset();
        }
        impl_->wv2Env.Reset();

        if (impl_->browser) {
            impl_->browser->Stop();
            impl_->browser->Quit();
            impl_->browser.Reset();
        }
        if (impl_->oleObj) {
            impl_->oleObj->Close(OLECLOSE_NOSAVE);
            impl_->oleObj->SetClientSite(nullptr);
            impl_->oleObj.Reset();
        }
        if (impl_->oleInitialized) OleUninitialize();
        impl_.reset();
    }
    if (hostHwnd_ && IsWindow(hostHwnd_)) {
        DestroyWindow(hostHwnd_);
    }
    hostHwnd_ = nullptr;
}

}  // namespace wp
