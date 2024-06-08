# FROMSOFTWARE ~~Anti-Anti~~ProDebug Library
Disable checks and measures that prevent runtime debugging in select Fromsoft titles.

### Motivation
On PC, most modern FromSoftware titles use \*\*\*\*\* to obfuscate assembly and
hinder the usability of debuggers, even force-closing the game if a debugger is detected.
This is done by trapping the attached debugger and/or by detecting its presence through
VEH (Vectored Exception Handling).

As the release of the ELDEN RING: Shadow of the Erdtree draws near (as of 08.06.2024),
I am releasing a DLL that tries to disable most of the checks,
leading to a more debugger-friendly experience.

### Usage
In one of the supported games, load the DLL through any method, at any time before debugging.

"How to load" examples:
- Mod Engine 2
- Lazy Loader
- Elden Mod Loader
- Cheat Engine's "Inject DLL" tool 

### Supported Games
DSR and DS3 may be supported at a later date (not earlier than after ER SOTE is released):

- [x] ARMORED CORE VI
- [x] ELDEN RING
- [ ] DARK SOULS III
- [ ] DARK SOULS REMASTERED


### Q&A
- Q: How does this work?

  A: By locating a common pattern in the antidebug code and neutralizing potential function calls.

- Q: How long will it work for?

  A: Until this (simple) way of patching it out is itself patched.

- Q: Will I get banned for using this?

  A: This is an anti-antidebug tool, not an anticheat toggler. Offline, it should not modify savefiles in any way, but use it online at your own risk.

- Q: Support for other games?

  A: Eventually ™

- Q: Other plans?

  A: Working around \*\*\*\*\* obfuscation and function encryption, also eventually ™