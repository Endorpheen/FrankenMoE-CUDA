# FrankenMoE Ember Goose

A dark theme for the llama-ui that ships with this runtime, plus the FrankenMoE mark. It is a skin: the interface, its layout,
its controls and its behaviour are the original ones. Nothing in this directory changes JavaScript, the API, the server or any
accepted build.

## What is in here

```
ui/franken-ember/
├── build.sh             builds site/ from the shipped bundle: decompress, add the theme link, add the mark
├── preview.sh           one command to look at it, with or without llama-server
├── theme/ember.css      the whole theme: colour tokens only
├── theme/ember-mark.svg the mark painted into the original logo box
├── assets/favicon.svg   the same mark as the favicon
├── shots/               screenshots of the result
└── site/                generated, git-ignored
```

## Look at it

```bash
bash ui/franken-ember/preview.sh server     # default: llama-server --path, no model loaded
bash ui/franken-ember/preview.sh dev        # a plain static server instead
```

Then open **http://127.0.0.1:9137/** — port 9137 by default, `PORT=…` to change it. No GGUF is loaded, no CUDA, no inference: the
start command is `--path site --models-dir <empty dir> --no-models-autoload`, so the interface runs against a server that has no
model, which is the state the acceptance asks for.

## How the theme is applied

`build.sh` copies the prebuilt bundle, decompresses it (the shipped copy is gzip), and injects one line into `index.html`:

```html
<link href="./_app/immutable/assets/ember.css" rel="stylesheet">
```

`ember.css` only overrides custom properties the interface already uses — `--background`, `--card`, `--primary`, `--border`,
`--sidebar*` and their neighbours — plus `::selection` and the scrollbar colour. It never sets a size, a margin or a display on
a control, so it cannot move one. The palette is the specification's: ground `#151719`, surfaces `#1D2023`, panels `#25292D`,
text `#E6E6E3`, secondary `#93999F`, copper `#C87942`, ember `#DF7138`, borders `#363B40`.

## The mark

The interface draws its logo as the only `svg` without a class, 16×16, inside the navigation button. The theme paints the goose
into that same box as a background and hides the original paths, so the box, the button and the row keep their original size.

`FrankenMoE` is written beside the emblem by an `::after` on the header row: absolutely positioned, pointer-transparent, clipped
by the sidebar. It appears when the panel is wide enough and disappears into the clip when collapsed; below 768 px only the
emblem is shown, which the specification allows.

## Proof that the layout is untouched

Every button, link, input and textarea was measured in the original page and in the themed one at the same viewport:

```
Add files, prompts, tools or MCP Servers | 297 |368| 32| 32   identical
Expand navigation                        |  16 | 16| 36| 36   identical
New chat                                 |  16 | 60| 36| 36   identical
Search                                   |  16 |100| 36| 36   identical
Settings                                 |  16 |140| 36| 36   identical
Open command picker                      | 284 |295|768|118   identical
Send                                     |1007 |368| 32| 32   identical
TEXTAREA                                 | 305 |308|726| 48   identical
```

Same coordinates, same sizes. The only difference between the two captures was application state — the first original load
showed a `Retry` control that the reloaded page did not — not geometry.

## Going back to the original

Nothing needs undoing: the shipped bundle was never modified. Delete `ui/franken-ember/site/` to drop the built copy, and serve
`vendor/flint/tools/ui/dist` — or start the server without `--path` — to get the original interface back.

## What this does not touch

The runtime, Goose, CUDA and CPU backends, the accepted builds and checkpoints, and any agent working on them. `build.sh` reads
the bundle and writes only inside `ui/franken-ember/`.
