# minispot

Small, self-contained Win32/WebView2 Spotify Connect client.

## compilation and installation

To compile it, you'll need the MSVC compiler. One way is with Visual Studio build tools and install MSVC build tools + Windows 11 SDK. Then make sure the `VS_INIT` path is right (you may need to customize the start of the path). Now you'll need to get the WebView2 headers (see the section below). From there you can just run `compile.py` and it'll output into `dist/main.exe`. Then you'll need to put your app credentials into the configuration file to make it actually run (up next)

### getting the `wv2` folder

Get the WebView2 NuGet package. Then open it in some NuGet package viewer, extract it, and copy the whole folder into the repo directory. Rename it to `wv2` once it's here. You can delete everything outside of `wv2/build/native` if you want.

## getting credentials

You'll need a Spotify development app to run it. To do that, go to [the Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and make a new app. Make sure one redirect URL is `http://127.0.0.1:20956/` (including the ending slash!) and Web Playback is enabled as an API. Save it and go to the top where your Client ID and Client Secret are.

Then, go to the minispot install path (should be in `dist` if you just compiled it) and make a file called `configuration`. Then put your Client ID, a colon, and your Client Secret. For example if my Client ID was `abc` and my Client Secret was `xyz` the configuration file would just be `abc:xyz`.

You should be good to go, just launch `main.exe`, log in, and connect to it from your phone.

## note about the code

For anybody looking through the code. This is my first non-Qt C++ project, I've only worked with C before. So, it's probably pretty bad.
