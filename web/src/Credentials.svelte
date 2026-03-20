<script lang="ts">
  let clientIdTxt = $state("");
  let clientSecretTxt = $state("");

  const sub = () => {
    const credStr = `${clientIdTxt}:${clientSecretTxt}`;
    window.chrome.webview.postMessage(`setcred ${credStr}`);
    // will do redirects and stuff on backend
  };
</script>

<label for="client-id-txt">Client ID: </label>
<input type="text" id="client-id-txt" bind:value={clientIdTxt} />
<br />
<label for="client-secret-txt">Client Secret: </label>
<input type="text" id="client-secret-txt" bind:value={clientSecretTxt} />
<br />
<button onclick={sub}>OK</button>
