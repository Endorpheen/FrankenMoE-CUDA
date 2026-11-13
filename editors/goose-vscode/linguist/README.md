# Goose entry for github-linguist

Status: **NOT SUBMITTED**. GitHub.com does not highlight a language just because
a grammar exists in a repository; it only uses grammars vendored into
`github-linguist/linguist`. Local editor support is independent of that.

## Current Lingust requirements (checked 2026-11-13)

From `github-linguist/linguist` CONTRIBUTING.md:

1. **Usage threshold**: at least **2000 files per extension** indexed by GitHub
   Search in the last year, excluding forks, for an extension expected more than
   once per repository (`.goose` is such an extension). At least 200 files for
   once-per-repo names.
2. A reasonable distribution across unique `user/repo` combinations; results
   concentrated in the author's own repositories are discounted.
3. A syntax-highlighting grammar with one of Linguist's approved licenses.
4. Real-world samples in `samples/<Language>/`, not tutorial or hello-world code,
   with the license stated.
5. `languages.yml` entry without `language_id`; the ID is generated with
   `script/update-ids`.
6. The grammar is added with `script/add-grammar <git URL>`.
7. Pull request uses the Linguist template and links a GitHub Search result.

Goose currently has a few hundred `.goose` files in this project and no
independent adoption, so requirement 1 is not met. A pull request now would be
closed. This package is the part that can be prepared in advance.

## What is prepared here

| file | purpose |
|---|---|
| `languages.yml.fragment` | the entry to paste into `lib/linguist/languages.yml`, alphabetical position |
| `samples/Goose/*.goose` | real sources from this repository, Apache-2.0, for `samples/Goose/` |
| `../syntaxes/goose.tmLanguage.json` | the grammar that `script/add-grammar` would vendor |

The grammar lives in this package rather than a separate repository; when the
usage threshold is met, it can be moved to its own repository (Linguist expects
a grammar repository URL) or added directly with `script/add-grammar --replace`.

## Steps when the threshold is met

1. Publish the grammar as its own repository with an approved license (MIT or
   Apache-2.0), keeping the `source.goose` scope name.
2. In a clone of `github-linguist/linguist`, run
   `script/add-grammar https://github.com/<owner>/goose-grammar`.
3. Add the `languages.yml` entry from `languages.yml.fragment`.
4. Copy `samples/Goose/` into `samples/Goose/`.
5. Run `script/update-ids` and commit the generated `language_id`.
6. Open the pull request with the template, linking a GitHub Search URL such as
   `https://github.com/search?q=path%3A*.goose+NOT+is%3Afork&type=code` and
   stating the Apache-2.0 license of the samples.

Until that merge, `.goose` files on github.com are not highlighted as Goose. Do
not work around it with a `.gitattributes` override: mapping Goose to another
language would show wrong highlighting and misreport the language statistics.
