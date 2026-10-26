# ArdPortal Wiki source

`wiki/` is the local Markdown and screenshot source for the published
[GitHub Wiki](https://github.com/rkubera/ArdPortal/wiki).

The Wiki is a separate Git repository:

```sh
git clone https://github.com/rkubera/ArdPortal.wiki.git
```

Edit the matching Markdown pages, `_Sidebar.md` and `_Footer.md`, copy the source
into the Wiki checkout, then commit and push its default branch. Copy `assets/`
when adding screenshots. Do not copy another checkout's `.git` directory.

Documentation is in English. Keep methods, feature flags, schemas, limits and
examples consistent with the library sources. Mark unreleased API additions.
Check JSON examples, code fences, page/anchor links and images before publishing.
A Wiki publication does not publish library source changes or a new release.

## Reproduce control screenshots

Run `python3 docs/tools/preview_controls.py` from the library checkout, then open
`http://127.0.0.1:8766/p/light` in a browser. Replace `light` with any field type.
The preview uses the actual `PortalPage.h` frontend, English catalog and built-in
control descriptors, with mock HTTP data and illustrative values. It never
connects to a physical board or broker. Capture the `#dynamic-<type>` section
into `wiki/assets/control-<type>.jpg`. Stop the local server when finished.

## Reproduce portal screenshots

Run `python3 docs/tools/preview_portal.py --profile dht --port 8767`, then open
`http://127.0.0.1:8767/`. The read-only preview uses the current generated frontend
with illustrative data; it does not contact devices or brokers. Capture `/device`,
`/wifi`, `/ap`, `/mqtt`, `/upgrade`, `/console` and `/p/settings` for the gallery.
Use `--profile zigbee` for `/zigbee`, `--profile matter` for `/matter`, or
`--profile thread` for Matter over Thread with the Wi-Fi portal status bar.
The QR fixture encodes the public development example `MT:Y.K9042C00KA0648G00`;
never replace it with production or private pairing credentials.

Save gallery images in `docs/wiki/assets/`; keep `media/Demo1.png` through
`media/Demo8.png` synchronized with the corresponding wiki images. Wait for
translations and status before capture, and frame the relevant controls.
