#!/usr/bin/env bash
# Credential gate: fails if any tracked (or, with --staged, staged) file carries a credential.
#
# AI assistants index source files. The exclusion files (.aiexclude, .cursorignore, .codeiumignore,
# .copilotignore, .claude/settings.json) keep credential FILES away from them, but no ignore file can
# hide a key pasted inline into a .cpp or .md. The only guarantee is that none is ever committed:
# this script is the ctest `check_no_secrets` and the pre-commit hook in .githooks/.
#
# A false positive is silenced with a trailing `# no-secret` / `// no-secret` on that line.
set -euo pipefail
cd "$(dirname "$0")/.."

pattern='AKIA[0-9A-Z]{16}|ASIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{40,}|sk-(ant-|proj-)?[A-Za-z0-9_-]{20,}|xox[abprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}|glpat-[A-Za-z0-9_-]{20}|-----BEGIN ([A-Z]+ )?PRIVATE KEY-----|(api[_-]?key|secret|password|passwd|access[_-]?token|auth[_-]?token)["'"'"']?[[:space:]]*[:=][[:space:]]*["'"'"'][^"'"'"'[:space:]]{12,}["'"'"']'
banned_files='(^|/)(\.env(\..*)?|.*\.(pem|key|p12|pfx|jks|keystore|tfvars|tfstate)|id_(rsa|ed25519|ecdsa)[^/]*|\.netrc|\.pypirc|\.git-credentials)$'

if [[ "${1:-}" == "--staged" ]]; then
    files=$(git diff --cached --name-only --diff-filter=ACMR)
    grep_src=(git grep --cached -nIE)
else
    files=$(git ls-files)
    grep_src=(git grep -nIE)
fi

status=0
if bad=$(printf '%s\n' "$files" | grep -E "$banned_files" | grep -v '^external/'); then
    echo "check_no_secrets: credential files must not be committed:" >&2
    printf '  %s\n' $bad >&2
    status=1
fi
if [[ -n "$files" ]] && hits=$("${grep_src[@]}" "$pattern" -- $files ':!external' ':!tools/check_no_secrets.sh' | grep -v 'no-secret'); then
    echo "check_no_secrets: possible credentials (move them to the environment or a secret store):" >&2
    printf '%s\n' "$hits" | cut -d: -f1,2 | sed 's/^/  /' >&2   # location only: never echo the secret
    status=1
fi
[[ $status -eq 0 ]] && echo "check_no_secrets: clean"
exit $status
