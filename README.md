# sapphos wacky sdk13 pseudolibrary

### This is in alpha. Please don't use it yet


## HOW TO USE:
This is premade to hook up to VPC scripts.

- Clone repo as a submodule
- Stick it in `/src/game/shared/sdk13-sappholib`
- Stick this block somewhere in your client vpc, adjusting it as needed

```
// <sappholib>

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
$Macro BLACKLISTS_URL "https://tf2classic.com/api/bad.txt"
// A URL for your users to visit to report a server being incorrectly marked as malicious
$Macro BLACKLISTS_CONTACT_URL "https://tf2classic.com/"

// A valid https URL that points to a document containing a valid Sentry.IO DSN (including self hosted)
// This is literally a text page with the url of your sentry instance as the content, NOT the actual url of your sentry instance wholesale
// This is so you can change this value on the fly without recompiling your entire project
$Macro SENTRY_URL                    "https://tf2classic.org/sentry"
$Macro SENTRY_PRIVACY_POLICY_URL     "https://tf2classic.com/privacy"

$Include "$SRCDIR\game\shared\sapphoio\sapphoio.vpc"
$Configuration
{
    $Compiler
    {
        // Enable bytepatching engine binaries with various fixes and tweaks
        // $PreprocessorDefinitions         "$BASE;BIN_PATCHES"

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
        $PreprocessorDefinitions            "$BASE;SENTRY"
        $PreprocessorDefinitions            "$BASE;SENTRY_URL=$QUOTE$SENTRY_URL$QUOTE"
        $PreprocessorDefinitions            "$BASE;SENTRY_PRIVACY_POLICY_URL=$QUOTE$SENTRY_PRIVACY_POLICY_URL$QUOTE"
    }

    $Linker
    {
        $AdditionalDependencies         "$BASE wsock32.lib" [$WIN32]
    }
}
// </sappholib>
```

- Stick this block somewhere in your server vpc, adjusting it as needed
```
// <sappholib>
$Include "$SRCDIR\game\shared\sapphoio\sapphoio.vpc"
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
- If you're using the sentry integration, .dlls, .sos, and .exes need to be distributed into your clients' /mod/bin folder, next to client and server binaries
