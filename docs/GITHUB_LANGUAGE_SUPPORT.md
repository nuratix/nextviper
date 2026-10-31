# GitHub language recognition

## Current status

NextViper is not registered in GitHub Linguist. Checking the upstream language list on 2026-10-19 found no NextViper entry or `.nv` extension. No upstream pull request has been opened by this work.

The repository already provides a TextMate grammar at [`editors/vscode/syntaxes/nextviper.tmLanguage.json`](../editors/vscode/syntaxes/nextviper.tmLanguage.json). Its scope is `source.nextviper`, and its `fileTypes` metadata declares `nv`. The VS Code manifest also associates `.nv` files with NextViper. These files provide editor integration metadata; they do not register a language on GitHub. The grammar has not been validated by Linguist's grammar compiler.

## Adoption requirement

[Linguist's contribution policy](https://github.com/github-linguist/linguist/blob/main/CONTRIBUTING.md#language-extension-and-filename-usage-requirements) currently requires at least **2,000 indexed files** for an extension expected to appear multiple times per repository, excluding forks, indexed in the last year. Results must be distributed reasonably across independent users and repositories. Maintainers can filter out the language owner's files. The lower 200-file threshold applies to filenames or extensions expected only once per repository, such as Makefiles; it is not the appropriate target for `.nv` source files.

This checkout contains 122 tracked `.nv` files, all in the language's own repository. A repository search for `nextviper fork:false` found this repository only. Additional code searches did not establish independent adoption of this language. Search indexing has limitations, so these observations are not a complete census. They do not supply the evidence needed to meet the policy. Duplicating examples, making forks, or creating empty repositories would not establish independent adoption.

Linguist explicitly says it closes submissions for very new or hobby languages without sufficient usage. A submission must wait until genuine adoption can be demonstrated.

## Submission checklist once eligible

1. Recheck the current upstream policy and collect a [public code search](https://github.com/search?q=path%3A*.nv+NOT+is%3Afork&type=code) showing qualifying NextViper programs. Inspect results to exclude unrelated uses of `.nv` and document independent users and repositories.
2. Validate the TextMate grammar using Linguist's `script/add-grammar` tooling. The grammar is currently covered by this repository's Apache-2.0 license; verify upstream's accepted grammar licenses and preserve notices.
3. Add an alphabetically placed `NextViper` entry in `lib/linguist/languages.yml`, with `type: programming`, extension `.nv`, and `tm_scope: source.nextviper`. Use `script/update-ids` to generate the language ID; do not invent an ID.
4. Add representative real-world `.nv` samples, recording their source and license. Tutorial and hello-world examples are not accepted. Check for extension conflicts and add samples or heuristics if required by the current policy.
5. Run the upstream tests, then submit a pull request using the complete upstream template, including usage evidence and sample-license information.
6. Wait for maintainer review, acceptance, a Linguist release, and GitHub deployment. Opening or merging a pull request does not immediately enable recognition everywhere.

Do not add `linguist-language=NextViper` as an interim override: an override cannot create a language missing from Linguist's supported language list.

## Engineering recheck, 2026-10-31

The upstream contribution policy was re-read; the adoption threshold above still
applies. Independent usage was not established in this session. See the
[local upstream draft](linguist/submission-draft.md) and
[metadata draft](linguist/NextViper.yml). Grammar metadata and keyword/operator
patterns are checked by `tests/test_grammar.py`; this is not full TextMate or
upstream grammar validation. The grammar now recognizes `elif`, `loop` and
logical keywords and no longer marks unimplemented exception/class syntax as
language keywords. Historical file counts above describe the earlier checkout.
