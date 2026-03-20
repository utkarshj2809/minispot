<script lang="ts">
    import { onMount } from "svelte";
    import Icon from "./Icon.svelte";

    type Props = {
        player: Spotify.Player;
        deviceId: string;
        token: string;
    };

    let { player, deviceId, token }: Props = $props();

    let currentTrack = $state<Spotify.Track | null>(null);
    let playing = $state(false);
    let selectingPlaylist = $state(false);

    const playerStateChanged = (state: Spotify.PlaybackState) => {
        if (!state) return;
        currentTrack = state.track_window.current_track;
        playing = !state.paused;
    };

    onMount(() => {
        player.addListener("player_state_changed", playerStateChanged);
        startPlayback();
        return () =>
            player.removeListener("player_state_changed", playerStateChanged);
    });

    const getUserPlaylists = async () => {
        const response = await fetch(
            "https://api.spotify.com/v1/me/playlists",
            {
                headers: {
                    Authorization: `Bearer ${token}`,
                },
            },
        );
        const json = await response.json();
        return json.items;
    };

    const startPlayback = async (uri?: string) => {
        currentTrack = null;
        selectingPlaylist = false;
        await fetch(
            `https://api.spotify.com/v1/me/player/play?device_id=${deviceId}`,
            {
                method: "PUT",
                headers: {
                    Authorization: `Bearer ${token}`,
                    "Content-Type": "application/json",
                },
                body: JSON.stringify(uri ? { context_uri: uri } : {}),
            },
        );
    };
</script>

{#if selectingPlaylist}
    <button type="button" onclick={() => (selectingPlaylist = false)}>
        Back
    </button>
    <strong>Select playlist</strong><br />
    {#await getUserPlaylists() then playlists}
        {#each playlists as pl}
            <button type="button" onclick={() => startPlayback(pl.uri)}>
                {pl.name}
            </button>
        {/each}
    {/await}
{:else if currentTrack}
    <div id="player">
        <img
            src={currentTrack.album.images[0].url}
            alt="Album cover for {currentTrack.name}"
        />
        <div id="player-ui">
            <div id="song-details">
                <header>{currentTrack.name}</header>
                <div id="song-artists">
                    {currentTrack.artists.map((x) => x.name).join(", ")}
                </div>
            </div>
            <div id="controls">
                <button onclick={() => player.previousTrack()}
                    ><Icon icon="prev" /></button
                >
                <button onclick={() => player.togglePlay()}>
                    <Icon icon={playing ? "playing" : "paused"} />
                </button>
                <button onclick={() => player.nextTrack()}
                    ><Icon icon="next" /></button
                >
            </div>
            <button type="button" onclick={() => (selectingPlaylist = true)}>
                Set playlist
            </button>
        </div>
    </div>
{:else}
    <div class="fs-error">Loading...</div>
{/if}

<style>
    #player {
        height: 100vh;
        display: flex;
        flex-direction: row;
        align-items: center;
        gap: 32px;
        padding: 0 16px;

        div#player-ui {
            height: 128px;
            flex: 1;
            display: flex;
            flex-direction: column;
            justify-content: center;

            header {
                font-weight: bold;
                margin-top: 8px;
            }

            div#song-details {
                flex: 1;
                display: flex;
                flex-direction: column;
                justify-content: center;

                div#song-artists {
                    color: #ccc;
                }
            }

            div#controls {
                margin-top: auto;
            }
        }

        > img {
            height: 128px;
            border-radius: 4px;
        }
    }
</style>
