# Documentation citation gate

This library now carries the shared documentation-citation gate. The script is `tools/check_docs_claims.py`, and its self-test is `tools/test_check_docs_claims.sh`. Both live under `tools/`.

`config/cpp_details.txt` and `config/update_policy.txt` were created here to record the gate. The rule is that a commit changing a `*.md` file runs the gate and leaves MISSING at zero, or lists every remaining line with its justification in the commit message. The update-policy bullets sit under the documentation-updates checklist item.

Run the gate with `python3 tools/check_docs_claims.py`, which checks the Markdown lines added since the merge-base with this repository's own default branch. Use `python3 tools/check_docs_claims.py --base REV` to set the base revision, `python3 tools/check_docs_claims.py --all FILE.md` to check one whole document, and `python3 tools/check_docs_claims.py --json` for machine-readable output. Run the self-test with `bash tools/test_check_docs_claims.sh`. It has 53 checks, runs on a throwaway fixture repository, and needs no model, no build and no network.

The four buckets are:

- MISSING: a cited name that is not in the tracked files. This is fatal and gives exit status 1.
- DESIGN: a name inside a heading section or top-level list entry marked "Design (not built)". Reported, never fatal.
- FRAGMENT: two backticks paired across a sentence, which is a Markdown defect. Reported, never fatal.
- REVISION: a commit this repository cannot resolve. Reported, never fatal.

A REVISION is a span of 7 to 40 hex characters with at least one digit. A revision this repository resolves is not reported. One it cannot resolve belongs to another repository, and a mistyped local sha is therefore still reported.

The limit is that the lookup is textual. A pass proves that a cited name exists in the tracked files outside `docs/` and `*.md`. It does not prove that the sentence around the citation is true.

The copy of record is sensen's `tools/check_docs_claims.py`, and the copy here is byte-identical to it. Change
sensen's and copy it out; never edit this copy in place. Nothing here can compare the two, so the self-test is the only
local proof that this copy works.
