# autogit

A ~400-line C CLI that kills:

```
fatal: No configured push destination.
```

…and its cousin:

```
ERROR: Repository not found.
```

One command does `git remote add`, `git add`, `git commit`, `git push -u`. If the GitHub repo doesn't exist yet, `autogit` offers to create it — private or public, your choice.

## Why

Pushing a fresh repo by hand means:

1. `git remote add origin <url>`
2. `git add -A`
3. `git commit -m "…"`
4. `git push -u origin main`

…and remembering to `-u` the first time, and remembering to create the repo on GitHub first. Forget any step and git yells at you. `autogit` does all of it, plus writes a short description into `.git/description`.

## Features

- **Auto remote** — builds `git@<host>:<user>/<project>.git` from the current directory name, using `~/.autogitrc` for host + username
- **`.env` support** — per-project `GIT_REMOTE_URL` overrides the default
- **Project-name appending** — a base URL like `git@github.com:you` becomes `git@github.com:you/<project>.git` automatically
- **Repo auto-creation** — if the GitHub repo doesn't exist, offers to create it via `gh` CLI or the REST API (`GITHUB_TOKEN`)
- **Private or public** — `--public` / `--private` flags, interactive prompt, or `GITHUB_VISIBILITY` in `.env`
- **Safe commit messages** — passed via `git commit -F <tempfile>` so quotes, `$`, backticks never break the shell
- **Always pushes with `-u`** — sets upstream tracking once, never again
- **Windows-native** — builds and runs from cmd.exe or PowerShell

## Build

Requires `gcc` (MinGW-w64 on Windows, default `cc` on Linux/macOS).

```bash
make            # → build/autogit (or build\autogit.exe on Windows)
make run        # build + execute
make test       # build + print help
make clean      # remove build/
```

Install (POSIX only):

```bash
sudo make install     # → /usr/local/bin/autogit
```

On Windows, copy `build\autogit.exe` to any folder on `PATH`.

No `make`? One command does it:

```bat
if not exist build mkdir build
gcc -Wall -Wextra -O2 -std=c11 -o build\autogit.exe autogit.c
```

## Usage

```
autogit [options]
```

| Flag | Meaning |
|------|---------|
| `-u <url>` | Remote URL (skips prompt; rewrites an existing remote) |
| `-r <name>` | Remote name (default: `origin`) |
| `-b <branch>` | Branch to push (default: current) |
| `-d <desc>` | Short description → commit message + `.git/description` |
| `-n` | Non-interactive: no prompts, use defaults |
| `--public` | Create the GitHub repo as public (creation only) |
| `--private` | Create the GitHub repo as private (creation only, default) |
| `-h` | Show help |

## First push of a new project

```bash
$ cd ~/projects/my-cool-app
$ autogit

First-time setup (saved to /home/you/.autogitrc)
Username (github handle): octocat
Host [github.com]:
Remote URL [git@github.com:octocat/my-cool-app.git]:
Short description: initial import of the CLI

$ git remote add origin "git@github.com:octocat/my-cool-app.git"
$ git add -A
$ git commit -F "/tmp/autogit_1234_0.tmp"
```

If the repo doesn't exist yet:

```
Repository octocat/my-cool-app does not exist on GitHub.
Create it now? [Y/n]: y
Visibility [private/public] (private): public
Creating octocat/my-cool-app as public ...
Created https://github.com/octocat/my-cool-app (public)
$ git push -u origin main
```

## Everyday push

```bash
$ autogit
Short description: fix off-by-one in parser
$ git add -A
$ git commit -F ...
$ git push -u origin main
```

## Fully non-interactive

```bash
autogit -n -d "nightly build" \
        -u git@github.com:octocat/my-cool-app.git
```

## Configuration

### `~/.autogitrc` — created once, used by every repo

```
user=octocat
host=github.com
```

Read on first interactive run; drives the default URL suggestion `git@<host>:<user>/<project>.git`. Delete the file to be re-prompted.

### `.env` — per-project, committed by default

```env
# Base URL — autogit appends /<project>.git
GIT_REMOTE_URL=git@github.com:octocat

# Or a full URL — used verbatim
# GIT_REMOTE_URL=git@github.com:octocat/my-cool-app.git

# Optional: GitHub repo auto-creation
# GITHUB_TOKEN=ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
# GITHUB_VISIBILITY=private          # or public
```

`.env` holds only a public git URL, so it's safe to commit — cloning the repo means everyone's first `autogit` just works. Uncomment the `.env` line in `.gitignore` if you ever put real secrets there.

### URL precedence

1. `-u <url>` flag — always wins
2. `.env` `GIT_REMOTE_URL`
3. Interactive prompt (default built from `~/.autogitrc`)

A URL that doesn't end in `.git` gets `/<project>.git` appended:

| Input | Resolved to |
|---|---|
| `git@github.com:octocat` | `git@github.com:octocat/my-cool-app.git` |
| `git@github.com:octocat/` | `git@github.com:octocat/my-cool-app.git` |
| `git@github.com:octocat/my-cool-app.git` | *unchanged* |

## Auto-creating the GitHub repo

If the remote is a GitHub URL and the repository doesn't exist on the server yet, `autogit` offers to create it before pushing.

Two backends, tried in order:

1. **`gh` CLI** — install once, no token needed:
   ```bash
   winget install GitHub.cli        # Windows
   brew install gh                  # macOS
   sudo apt install gh              # Debian/Ubuntu
   gh auth login
   ```

2. **`GITHUB_TOKEN` in `.env`** — a classic Personal Access Token with `repo` scope:
   - GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic) → Generate new token → check `repo`
   - Paste into `.env`: `GITHUB_TOKEN=ghp_...`

### Choosing visibility — private or public

| Method | When it applies |
|---|---|
| `--public` / `--private` flag | Always wins — skips the visibility prompt |
| Interactive prompt | Asked before creating, unless `-n` or a flag is given |
| `.env` `GITHUB_VISIBILITY` | Default for `-n` runs, and the suggested value in the prompt |
| Built-in default | `private` — when nothing else is set |

Examples:

```bash
# Ask interactively (default suggestion: private)
autogit -d "first push"

# Explicit, no prompt
autogit --public  -d "first push"
autogit --private -d "first push"

# Non-interactive — uses .env GITHUB_VISIBILITY, or 'private'
autogit -n -d "first push"
```

**Flags only affect creation.** On an existing repo, `--public`/`--private` print a one-line note and are otherwise ignored — use GitHub's settings page to change visibility after the fact.

Non-GitHub hosts (GitLab, Bitbucket, self-hosted) are skipped silently — `autogit` still pushes normally.

## SSH setup (one-time)

`autogit` uses whatever URL you give it. For `git@github.com:...` URLs you need a working SSH key:

```bash
ssh-keygen -t ed25519 -C "you@example.com"
gh ssh-key add ~/.ssh/id_ed25519.pub --title "my machine"
ssh -T git@github.com                    # should say "Hi <username>!"
```

Or add the key manually at [https://github.com/settings/keys](https://github.com/settings/keys).

For HTTPS URLs (`https://github.com/...`) use a PAT as the password when git prompts, or configure a credential helper:

```bash
git config --global credential.helper manager
```

## How it works

On every run:

1. Verify we're inside a git work tree
2. Get the repo root and project name from `git rev-parse`
3. Read `.env` (`GIT_REMOTE_URL`, `GITHUB_TOKEN`, `GITHUB_VISIBILITY`), `~/.autogitrc`, and the current branch
4. If remote is missing → build URL (`-u` > `.env` > prompt) and `git remote add`
5. If remote URL is GitHub and repo doesn't exist → offer to create it (with visibility choice)
6. `git add -A` + `git commit -F <tempfile>` (only if there are changes)
7. `git push -u <remote> <branch>` — the `-u` is what kills the "No configured push destination" error permanently

## Files it touches

| Path | Purpose |
|------|---------|
| `~/.autogitrc` | `user=…` and `host=…` — created once |
| `.git/description` | Overwritten with your short description when the remote is added or `-d` is passed |
| `<tmpdir>/autogit_*.tmp` | Commit message; deleted immediately after commit |
| `.env` (repo root) | Read-only — never modified |

## Windows notes

- Builds natively with `mingw32-make` and MinGW-w64 `gcc`. No WSL, no Git Bash required.
- If you see a file named `NUL` in your project root, a POSIX shell is interpreting `>NUL`. The Makefile forces `SHELL := cmd.exe`, and the binary detects MSYS gcc via `__MSYS__` and switches to `/dev/null` automatically — both fixes are in place as of the current version.
- Delete a stray `NUL` with `rm NUL` (Git Bash) or `del \\.\<full-path>\NUL` (cmd.exe).

## License

MIT — see [LICENSE](LICENSE).