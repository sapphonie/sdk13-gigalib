# sapphos wacky sdk13 pseudolibrary

## What's in it
- Engine bytepatches for common SDK13 problems, including a fix for high fov breaking skyboxes, and for servers getting lagged out by clients, reverse engineered from scratch with the exception of a few passed down from other programmers
- Engine hooks/detours for common SDK13 problems, including a new cvar `net_chan_proctime_limit_ms` that limits the amount of time that a client can make the server spend processing a packet, reverse engineered from scratch
- Quality of life cvars allowing clients to automatically flush custom content after leaving a server, enabled by the above engine hooks
- The ability to prevent clients from connecting to a list of malicious / "bad" servers, downloaded dynamically every game boot, enabled by engine detours
- [Sentry.IO](https://sentry.io) integration for crash reporting / telemetry / developer data etc
- Helper functions, including a function that allows you to hook when Steam has initialized on the client by spinning off a thread and checking every second
- More  coming soon


## HOW TO USE:
This is premade to hook up to VPC scripts.

- Clone repo as a submodule
- Stick it in `/src/game/shared/sdk13-sappholib`
- Stick this block somewhere in your client vpc, adjusting it as needed

### IF YOU ARE USING LIBCURL

You NEED to go into ./src/game/client/client_base.vpc and comment out all instances of
- libcurl
- curlssl
- libz
- libcrypto
- libbrotli
- libnghttp2
- libssh2
- libssl
- libstd

You might not have all of these, that's fine. Comment out the ones you do have.


Stick this block in your client vpc, adjusting as needed:

```
// A valid https URL that points to a .txt document that clients will download every game launch
// containing ip address that they will be prevented from connecting to
// Format:
/*
# comment
127.0.0.1
127.0.0.2
127.0.0.3
127.0.0.4
*/
// Example: https://tf2classic.com/api/bad.txt
$Macro BLACKLISTS_URL "CHANGEME"
// A URL for your users to visit to report a server being incorrectly marked as malicious
// Example: https://tf2classic.com/
$Macro BLACKLISTS_CONTACT_URL "CHANGEME"

// A valid https URL that points to a document containing a valid Sentry.IO DSN (including self hosted)
// This is literally a text page with the url of your sentry instance as the content, NOT the actual url of your sentry instance wholesale
// This is so you can change this value on the fly without recompiling your entire project
// Example: https://tf2classic.org/sentry
$Macro SENTRY_URL                    "CHANGEME"

// A valid https URL that points to your GDPR policy.
// Example: https://tf2classic.com/privacy
$Macro SENTRY_PRIVACY_POLICY_URL     "CHANGEME"

// The version you want to tag your game as in sentry reports.
// Example: 2.1.2
$Macro SENTRY_RELEASE_VERSION        "CHANGEME"
$Include "$SRCDIR\game\shared\sdk13-gigalib\sdk13-gigalib.vpc"
$Configuration
{
    $Compiler
    {
        // Enable bytepatching engine binaries with various fixes and tweaks
        $PreprocessorDefinitions         "$BASE;BIN_PATCHES"

        // Enable detouring engine functions with various fixes and tweaks, including an anti server lag measure
        // similar to tf2's net_chan_limit_msec
        // also required for hooking other engine funcs for misc functionality
        $PreprocessorDefinitions            "$BASE;ENGINE_DETOURS"

        // Enable blacklisting certain server IPs from all clients
        // REQUIRES engine detours
        $PreprocessorDefinitions            "$BASE;BLACKLISTS"
        $PreprocessorDefinitions            "$BASE;BLACKLISTS_URL=$QUOTE$BLACKLISTS_URL$QUOTE"
        $PreprocessorDefinitions            "$BASE;BLACKLISTS_CONTACT_URL=$QUOTE$BLACKLISTS_CONTACT_URL$QUOTE"

        // Enable optionally flushing server downloadables every time the client disconnects from a server
        // this includes sprays, all custom content, and map overrides, with the first two being controlled by client cvars,
        // and map overrides being done automatically to prevent servers from abusing clients
        // see cl_flush_sprays_on_dc (default 1) and cl_flush_downloads_on_dc (default 0)
        // REQUIRES engine detours
        $PreprocessorDefinitions            "$BASE;FLUSH_DLS"

        // Enable SentryIO telemetry / crash reporting
        // You *NEED* a privacy policy if you want to not run afoul of the GDPR
        // REQUIERS sdkcurl
        $PreprocessorDefinitions            "$BASE;SDKSENTRY"
        $PreprocessorDefinitions            "$BASE;SENTRY_URL=$QUOTE$SENTRY_URL$QUOTE"
        $PreprocessorDefinitions            "$BASE;SENTRY_PRIVACY_POLICY_URL=$QUOTE$SENTRY_PRIVACY_POLICY_URL$QUOTE"
        $PreprocessorDefinitions            "$BASE;SENTRY_RELEASE_VERSION=$QUOTE$SENTRY_RELEASE_VERSION$QUOTE"

        // Enable modern CURL
        $PreprocessorDefinitions            "$BASE;SDKCURL"

    }

    $Linker
    {
        $AdditionalDependencies         "$BASE wsock32.lib" [$WIN32]
    }
}
// </sdk13-gigalib>
```

- Stick this block somewhere in your server vpc, adjusting it as needed
```
// <sappholib>
$Include "$SRCDIR\game\shared\sdk13-gigalib\sdk13-gigalib.vpc"
$Configuration
{
    $Compiler
    {
        // Enable bytepatching engine binaries with various fixes and tweaks
        $PreprocessorDefinitions            "$BASE;BIN_PATCHES"

        // Enable detouring engine functions with various fixes and tweaks
        $PreprocessorDefinitions            "$BASE;ENGINE_DETOURS"
    }
}
// </sappholib>
```


## Distributing binaries

- If you're using the sentry integration, .dlls, .sos, and .exes need to be distributed into your clients' /mod/bin folder, next to client and server binaries
- If you're using the curl integration, .dlls need to be distributed into your clients' /mod/bin folder, next to client and server binaries
- You need to distribute the [fmtlib](https://github.com/fmtlib/fmt) binaries into your clients' /mod/bin folder, next to client and server binaries

I am going to make this easier to do, with a batch script. Give me a bit.
