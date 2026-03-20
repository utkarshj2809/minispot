<script lang="ts">
    import App from "./App.svelte";
    import Credentials from "./Credentials.svelte";

    const params = new URLSearchParams(location.search);
    const clientId = params.get("clientId");

    // goes to spotify to get the auth code, redirect gets intercepted by c++
    const spoGetAuthCode = (clientId: string) => {
        const scope = "streaming\n user-read-email\n user-read-private\n playlist-read-private\n user-modify-playback-state";

        const auth_query_parameters = new URLSearchParams({
            response_type: "code",
            client_id: clientId,
            scope: scope,
            redirect_uri: "http://127.0.0.1:20956/",
        });

        window.location.href =
            "https://accounts.spotify.com/authorize/?" +
            auth_query_parameters.toString();
    };

    let player = $state<Spotify.Player | null>(null);
    const token = params.get("token");

    window.onSpotifyWebPlaybackSDKReady = () => {
        if (!clientId || clientId == "false") return;
        if (!token) return spoGetAuthCode(clientId);

        player = new Spotify.Player({
            name: "minispot client",
            getOAuthToken: (cb) => {
                cb(token);
            },
            volume: 0.5,
        });
    };
</script>

{#if clientId === "false" || clientId === null}
    <Credentials />
{:else if player !== null}
    <App {player} token={token!} />
{:else}
    <strong class="fs-error">Loading...</strong>
{/if}
