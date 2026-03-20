#include <Windows.h>
#include <wrl.h>
#include <string>
#include <Shlwapi.h>
#include <dwmapi.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <codecvt>
#include <locale.h>
#include <stdlib.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>

#include "wv2/build/native/include/WebView2.h"
#include "wv2/build/native/include/WebView2EnvironmentOptions.h"
#include "resources.h"

using namespace Microsoft::WRL;

#define FOCUSER_HOTKEY_ID 6969

HWND g_hwnd = nullptr;
ComPtr<ICoreWebView2Controller> g_wvController;
const wchar_t g_mainSource[] = L"minispot://mainapp/";
const wchar_t g_mainSourceFilter[] = L"minispot://*";

struct SpoCredentialConfig
{
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    bool hasRefreshToken;
    bool isLoaded;
};

struct SpoCredentialConfig g_spoCredentials{};

void BoxInfo(LPCSTR text)
{
    MessageBoxA(g_hwnd, text, "Info", MB_OK);
}

void BoxInfoW(LPCWSTR text)
{
    MessageBoxW(g_hwnd, text, L"Info", MB_OK);
}

std::string Base64Encode(const std::string &input)
{
    DWORD size = 0;
    CryptBinaryToStringA(
        reinterpret_cast<const BYTE *>(input.data()),
        input.size(),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        nullptr,
        &size);

    std::string output(size, '\0');

    CryptBinaryToStringA(
        reinterpret_cast<const BYTE *>(input.data()),
        input.size(),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
        &output[0],
        &size);

    return output;
}

// https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
std::string URLEncode(std::string &value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i < n; ++i)
    {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            escaped << c;
            continue;
        }

        if (c == '\0')
            continue;

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT wmsg, WPARAM wParam, LPARAM lParam)
{
    switch (wmsg)
    {
    case WM_SIZE:
        if (g_wvController)
        {
            RECT newBounds;
            GetClientRect(hwnd, &newBounds);
            g_wvController->put_Bounds(newBounds);
        }
        break;
    case WM_HOTKEY:
        if (wParam == FOCUSER_HOTKEY_ID)
            ShowWindow(hwnd, SW_RESTORE);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, wmsg, wParam, lParam);
}

// load web html from embedded resource
std::string LoadHTML(int rcId)
{
    HRSRC res = FindResource(nullptr, MAKEINTRESOURCE(rcId), RT_RCDATA);
    if (!res)
        return "";

    HGLOBAL data = LoadResource(nullptr, res);
    if (!data)
        return "";

    DWORD size = SizeofResource(nullptr, res);
    void *resPtr = LockResource(data);

    std::string asciiHTML((char *)resPtr, size);

    return asciiHTML;
}

std::filesystem::path CredentialsPath()
{
    // relative to the executable path
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return exePath.parent_path() / "credentials";
}

void LoadCredentials()
{
    std::ifstream file(CredentialsPath());

    g_spoCredentials.isLoaded = true;
    if (!std::getline(file, g_spoCredentials.clientId))
        goto fail;
    if (!std::getline(file, g_spoCredentials.clientSecret))
        goto fail;
    g_spoCredentials.hasRefreshToken = static_cast<bool>(std::getline(file, g_spoCredentials.refreshToken));

    return;
fail:
    g_spoCredentials.isLoaded = false;
}

// get spotify user token from the refresh code if its in g_spocredentials otherwise with provided auth code
std::string spoGetAuthToken(std::wstring code_wc)
{
    HINTERNET session = WinHttpOpen(
        L"minispothttpclient/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET conn = WinHttpConnect(session, L"accounts.spotify.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = WinHttpOpenRequest(conn, L"POST", L"/api/token", NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    std::string postData = "redirect_uri=http%3A%2F%2F127.0.0.1%3A20956%2F";
    postData += "&grant_type=";

    if (g_spoCredentials.hasRefreshToken)
    {
        postData += "refresh_token&refresh_token=";
        postData += g_spoCredentials.refreshToken;
    }
    else
    {
        std::string mbCode(code_wc.length() + 1, 0);
        std::wcstombs(mbCode.data(), code_wc.data(), code_wc.length() + 1);
        postData += "authorization_code&code=";
        postData += URLEncode(mbCode);
    }

    DWORD postDataSize = postData.size();

    std::string spoAuth = g_spoCredentials.clientId + ":" + g_spoCredentials.clientSecret;
    std::string spoAuthB64 = Base64Encode(spoAuth);
    std::wstring authHeader =
        L"Authorization: Basic " +
        std::wstring(spoAuthB64.begin(), spoAuthB64.end()) +
        L"\r\n";
    std::wstring headers = L"Content-Type: application/x-www-form-urlencoded\r\n" + authHeader;

    WinHttpSendRequest(
        request,
        headers.data(), -1,
        LPVOID(postData.c_str()),
        postDataSize, postDataSize, 0);
    WinHttpReceiveResponse(request, 0);

    std::string response;
    char buffer[8192];

    DWORD bytesRead = 0;
    while (WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
        response.append(buffer, bytesRead);

    if (!g_spoCredentials.hasRefreshToken)
    {
        // add refresh token to credentials
        std::ofstream file(CredentialsPath(), std::ios::app);
        file << std::endl;
        size_t startIndex = response.find("\"refresh_token\"") + 17;
        std::string refreshToken = response.substr(startIndex, response.find("\",\"scope\":\"") - startIndex);
        file << refreshToken;
    }

    std::string accessToken = response.substr(17, response.find("\",") - 17);
    return accessToken;
}

void WV2NavigateToIndex()
{
    ComPtr<ICoreWebView2> wv;
    g_wvController->get_CoreWebView2(&wv);
    std::wstring newUrl =
        std::wstring(g_mainSource) +
        L"?clientId=" +
        (g_spoCredentials.isLoaded
             ? std::wstring(g_spoCredentials.clientId.begin(), g_spoCredentials.clientId.end())
             : L"false");
    if (g_spoCredentials.hasRefreshToken)
    {
        std::string token = spoGetAuthToken(L"");
        std::wstring tokenWcs(token.begin(), token.end());
        newUrl += L"&token=";
        newUrl += tokenWcs;
    }
    wv->Navigate(newUrl.data());
}

HRESULT WebMessageReceived(ICoreWebView2 *wv, ICoreWebView2WebMessageReceivedEventArgs *args)
{
    LPWSTR rawMessage;
    args->TryGetWebMessageAsString(&rawMessage);

    // set clientid/secret from webui
    if (wcsstr(rawMessage, L"setcred") == rawMessage)
    {
        LPWSTR credential = rawMessage + 8;
        wchar_t *colon = wcschr(credential, L':');
        size_t len = wcslen(credential);

        std::ofstream credfile(CredentialsPath());
        // weird utf8 stuff
        for (wchar_t *charPtr = credential; charPtr < colon; charPtr++)
        {
            char c = static_cast<char>(*charPtr);
            credfile << c;
        }

        credfile << std::endl;
        for (wchar_t *charPtr = colon + 1; charPtr < credential + len; charPtr++)
        {
            char c = static_cast<char>(*charPtr);
            credfile << c;
        }

        credfile.close();
        LoadCredentials();
        WV2NavigateToIndex();
    }

    CoTaskMemFree(rawMessage);
    return S_OK;
}


HRESULT WV2SetupNetworkHandling(ComPtr<ICoreWebView2> wv, ComPtr<ICoreWebView2Environment> env)
{
    ComPtr<ICoreWebView2_22> webview22;
    wv->QueryInterface(IID_PPV_ARGS(&webview22));
    if (!webview22)
        return E_FAIL;

    // check callback url from spotify authentication
    wv->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2 *wv, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT
            {
                LPWSTR rawUrl;
                args->get_Uri(&rawUrl);
                std::wstring url(rawUrl);

                if (url.rfind(L"http://127.0.0.1:20956", 0) != 0)
                {
                    CoTaskMemFree(rawUrl);
                    return S_OK;
                }

                args->put_Cancel(true);

                std::wstring code = url.substr(29);
                std::string token = spoGetAuthToken(code).data();
                std::string newUrl = "minispot://mainapp/?token=" + URLEncode(token) +
                                     "&clientId=" + g_spoCredentials.clientId;
                wv->Navigate(std::wstring(newUrl.begin(), newUrl.end()).data());

                CoTaskMemFree(rawUrl);
                return S_OK;
            })
            .Get(),
        nullptr);

    webview22->AddWebResourceRequestedFilterWithRequestSourceKinds(
        g_mainSourceFilter,
        COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL,
        COREWEBVIEW2_WEB_RESOURCE_REQUEST_SOURCE_KINDS_ALL);

    webview22->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [env](ICoreWebView2 *wv, ICoreWebView2WebResourceRequestedEventArgs *args)
            {
                ComPtr<ICoreWebView2WebResourceRequest> request;
                args->get_Request(&request);

                LPWSTR urlBuf = nullptr;
                request->get_Uri(&urlBuf);

                std::wstring url(urlBuf);
                CoTaskMemFree(urlBuf);

                if (url.find(L"mainapp"))
                {
                    std::string html = LoadHTML(IDR_HTML_FILE);
                    IStream *stream = SHCreateMemStream(
                        reinterpret_cast<const BYTE *>(html.data()),
                        static_cast<UINT>(html.size()));

                    ComPtr<ICoreWebView2WebResourceResponse> resp;
                    env->CreateWebResourceResponse(stream, 200, L"OK", L"Content-Type: text/html\r\n", &resp);
                    args->put_Response(resp.Get());

                    stream->Release();
                }

                return S_OK;
            })
            .Get(),
        nullptr);

    return S_OK;
}

HRESULT WV2EnvironmentCreated(HRESULT hResult, ICoreWebView2Environment *env)
{
    env->CreateCoreWebView2Controller(
        g_hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [env](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT
            {
                if (!controller)
                    return E_FAIL;
                g_wvController = controller;

                RECT winBounds;
                GetClientRect(g_hwnd, &winBounds);
                controller->put_Bounds(winBounds);

                // otherwise translucent window background is hidden
                ComPtr<ICoreWebView2Controller2> controller2;
                controller->QueryInterface(IID_PPV_ARGS(&controller2));
                controller2->put_DefaultBackgroundColor({0, 0, 0, 0});

                ComPtr<ICoreWebView2> wv;
                g_wvController->get_CoreWebView2(&wv);
                wv->add_WebMessageReceived(
                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(WebMessageReceived).Get(),
                    nullptr);
                WV2SetupNetworkHandling(wv, env);

                WV2NavigateToIndex();

                return S_OK;
            })
            .Get());

    return S_OK;
}

void InitWV2()
{
    auto options = Make<CoreWebView2EnvironmentOptions>();
    // idk if these are all necessary but it saves like 120mb of RAM
    options->put_AdditionalBrowserArguments(
        L"--disable-features=AudioServiceOutOfProcess,WebRtcHWAEC,"
        L"Extensions,TranslateUI,BackForwardCache,MediaRouter,Autofill,RendererCodeIntegrity"
        L"--disable-gpu --disable-software-rasterizer --disable-gpu-compositing --disable-gpu-sandbox "
        L"--disable-extensions "
        L"--disable-component-extensions-with-background-pages "
        L"--disable-background-timer-throttling "
        L"--disable-renderer-backgrounding "
        L"--disable-client-side-phishing-detection "
        L"--disable-sync "
        L"--disable-features=msSmartScreenProtection "
        L"--no-default-browser-check "
        L"--no-first-run "
        L"--disable-logging "
        L"--disable-dev-shm-usage "
        L"--disable-breakpad "
        L"--disable-crash-reporter "
        L"--process-per-site");

    // register the custom minispot:// scheme for fake HTTPS to access spotify api
    ComPtr<ICoreWebView2EnvironmentOptions4>
        options4;
    options.As(&options4);

    ComPtr<ICoreWebView2CustomSchemeRegistration> scheme =
        Make<CoreWebView2CustomSchemeRegistration>(L"minispot");
    scheme->put_TreatAsSecure(TRUE);
    scheme->put_HasAuthorityComponent(TRUE);

    const WCHAR *allowedOrigins[1];
    allowedOrigins[0] = g_mainSource;
    scheme->SetAllowedOrigins(1, allowedOrigins);

    ICoreWebView2CustomSchemeRegistration *schemes[1] = {scheme.Get()};
    options4->SetCustomSchemeRegistrations(1, schemes);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(WV2EnvironmentCreated)
            .Get());
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // makes it not blurry
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    LoadCredentials();

    const wchar_t MAIN_CLASS_NAME[] = L"minispot";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_CLASS_NAME;
    wc.hbrBackground = nullptr;

    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP,
        MAIN_CLASS_NAME,
        L"minispot",
        WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, 512, 256,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_hwnd, nCmdShow);

    // translucent window background
    DWORD backdrop = DWMSBT_TRANSIENTWINDOW;
    DwmSetWindowAttribute(g_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

    RegisterHotKey(g_hwnd, FOCUSER_HOTKEY_ID, MOD_CONTROL | MOD_ALT, 'M');

    InitWV2();

    MSG wmsg;
    while (GetMessage(&wmsg, nullptr, 0, 0))
    {
        TranslateMessage(&wmsg);
        DispatchMessage(&wmsg);
    }

    CoUninitialize();
    return 0;
}