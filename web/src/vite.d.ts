interface Window {
    onSpotifyWebPlaybackSDKReady: () => void;
    chrome: {
        webview: {
            postMessage: (message: string) => void;
            addEventListener: (
                eventType: string,
                handler: (event: Event) => void,
            ) => void;
        };
    };
}
