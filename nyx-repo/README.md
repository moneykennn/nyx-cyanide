# Nyx / Cyanide experimental integration

This is an experimental derivative of
[kolbicz/Cyanide v1.17.0](https://github.com/kolbicz/Cyanide/tree/v1.17.0)
at commit `c6a838c723475e3d1af0154ba3919ea2681bb903`. The upstream license and
credits are retained in the repository.

`repotweaks.json` points to `esp-experimental.js`. The script requires this
custom Cyanide build; it does not work with an unmodified Cyanide app. The
integration, its process access, screen rendering, accuracy, and performance
have **not** been verified on an iPhone or in PUBG. Local tests use synthetic
data. It does not promise avoidance of detection or bans.

The GitHub Actions build creates an unsigned IPA artifact if successful. Signing
and installing the IPA are separate steps. Do not treat a successful compilation
as evidence that the overlay or process access works on a device.
