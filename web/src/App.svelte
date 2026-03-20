<script lang="ts">
    import { onMount } from "svelte";
    import Player from "./Player.svelte";

    type Props = {
        player: Spotify.Player;
        token: string;
    };

    let { player, token }: Props = $props();

    let currentError = $state<string | null>("Loading...");
    let deviceId = $state("");

    onMount(() => {
        // Ready
        player.addListener("ready", ({ device_id }) => {
            console.log("Ready with Device ID", device_id);
            deviceId = device_id;
            currentError = null;
        });

        // Not Ready
        player.addListener("not_ready", ({ device_id }) => {
            currentError = "Device ID has gone offline" + device_id;
        });

        player.addListener("initialization_error", ({ message }) => {
            currentError = "initialization error: " + message;
        });

        player.addListener("authentication_error", ({ message }) => {
            currentError = "authentication error: " + message;
        });

        player.addListener("account_error", ({ message }) => {
            currentError = "account error: " + message;
        });

        player.connect();
    });
</script>

{#if currentError === null}
    <Player {player} {deviceId} {token} />
{:else}
    <strong class="fs-error">{currentError}</strong>
{/if}
