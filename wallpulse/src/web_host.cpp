#include "web_host.h"

#include <exdisp.h>
#include <mshtmhst.h>
#include <oleidl.h>
#include <shlwapi.h>

namespace wp {

namespace {

constexpr const wchar_t* kWebHostClass = L"WallPulseWebHostChildClass";

class OleBrowserSite final : public IOleClientSite, public IOleInPlaceSite, public IDocHostUIHandler {
public:
    explicit OleBrowserSite(HWND hwnd) : hwnd_(hwnd) {}

    // IUnknown
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

    // IOleClientSite
    HRESULT STDMETHODCALLTYPE SaveObject() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer** pp) override {
        if (pp) *pp = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE ShowObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override { return E_NOTIMPL; }

    // IOleWindow / IOleInPlaceSite
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

    // IDocHostUIHandler (removes 3D border, scrollbars & right-click menu on wallpaper)
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

LRESULT CALLBACK WebHostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

struct WebWallpaperHost::Impl {
    OleBrowserSite site;
    ComPtr<IOleObject> oleObj;
    ComPtr<IWebBrowser2> browser;

    explicit Impl(HWND hwnd) : site(hwnd) {}
};

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

bool WebWallpaperHost::AttachAndNavigate(
    HINSTANCE hInstance, HWND parentHwnd, const RECT& bounds, const std::wstring& htmlPath) {
    Close();
    if (!parentHwnd || !IsWindow(parentHwnd) || !FileExists(htmlPath)) return false;

    EnsureHtml5BrowserFeatureFlags();
    OleInitialize(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WebHostWndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = CreateSolidBrush(RGB(10, 14, 24));
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

    impl_ = new Impl(hostHwnd_);

    HRESULT hr = CoCreateInstance(
        CLSID_WebBrowser, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
        __uuidof(IOleObject), reinterpret_cast<void**>(impl_->oleObj.GetAddressOf()));
    if (FAILED(hr) || !impl_->oleObj) {
        Close();
        return false;
    }

    impl_->oleObj->SetClientSite(&impl_->site);
    RECT rcClient{0, 0, w, h};
    hr = impl_->oleObj->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, &impl_->site, 0, hostHwnd_, &rcClient);
    if (FAILED(hr) || FAILED(impl_->oleObj.As(&impl_->browser)) || !impl_->browser) {
        Close();
        return false;
    }

    impl_->browser->put_Silent(VARIANT_TRUE);
    impl_->browser->put_Left(0);
    impl_->browser->put_Top(0);
    impl_->browser->put_Width(w);
    impl_->browser->put_Height(h);

    wchar_t uriBuf[4096] = {};
    DWORD cch = 4095;
    std::wstring uri = htmlPath;
    if (SUCCEEDED(UrlCreateFromPathW(htmlPath.c_str(), uriBuf, &cch, 0)) && uriBuf[0] != L'\0') {
        uri.assign(uriBuf, cch);
    }

    VARIANT vUrl{};
    vUrl.vt = VT_BSTR;
    vUrl.bstrVal = SysAllocString(uri.c_str());
    VARIANT vEmpty{};
    VariantInit(&vEmpty);
    impl_->browser->Navigate2(&vUrl, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
    SysFreeString(vUrl.bstrVal);

    return true;
}

void WebWallpaperHost::SetBounds(const RECT& bounds) {
    if (!hostHwnd_ || !IsWindow(hostHwnd_)) return;
    int w = std::max<int>(64, bounds.right - bounds.left);
    int h = std::max<int>(64, bounds.bottom - bounds.top);
    SetWindowPos(hostHwnd_, HWND_TOP, bounds.left, bounds.top, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (impl_ && impl_->browser) {
        impl_->browser->put_Width(w);
        impl_->browser->put_Height(h);
        ComPtr<IOleInPlaceObject> inPlace;
        if (SUCCEEDED(impl_->oleObj.As(&inPlace)) && inPlace) {
            RECT rc{0, 0, w, h};
            inPlace->SetObjectRects(&rc, &rc);
        }
    }
}

void WebWallpaperHost::Close() {
    if (impl_) {
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
        delete impl_;
        impl_ = nullptr;
    }
    if (hostHwnd_ && IsWindow(hostHwnd_)) {
        DestroyWindow(hostHwnd_);
    }
    hostHwnd_ = nullptr;
}

}  // namespace wp
