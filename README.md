# `README.md`

```markdown
# autogit

A tiny C wrapper around `git commit` + `git push` that never leaves you
staring at:

    fatal: No configured push destination.

On first run it asks for your git host + username (saved once to
`~/.autogitrc`), then builds `git@<host>:<user>/<project>.git` from the
current directory name and configures the remote for you. Every run after
that is a one-liner.

## Why

Pushing a fresh repo by hand means:

1. `git remote add origin <url>`
2. `git add -A`
3. `git commit -m "…"`
4. `git push -u origin main`

…and remembering to `-u` the first time. Miss step 1 or forget `-u`, and
git yells at you. `autogit` does all four, plus writes a short description
into `.git/description`.

## Build

Requires `gcc` (MinGW-w64 on Windows, or the default `cc` on Linux/macOS).

```bash
make            # builds ./autogit (or autogit.exe on Windows)
make run        # build + run it
make test       # build + print help
make clean
```

Install (POSIX only):

```bash
sudo make install     # → /usr/local/bin/autogit
```

On Windows, copy `autogit.exe` anywhere on your `PATH`.

## Usage

```bash
autogit [options]
```

| Flag | Meaning |
|------|---------|
| `-u <url>` | Use this remote URL (skips prompt; rewrites an existing remote) |
| `-r <name>` | Remote name (default: `origin`) |
| `-b <branch>` | Branch to push (default: current branch) |
| `-d <desc>` | Short description → commit message + `.git/description` |
| `-n` | Non-interactive: no prompts, use defaults |
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
$ git commit -F "/tmp/autogit_....tmp"
$ git push -u origin main
```

## Everyday push (remote already configured)

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

## How the URL is guessed

1. `git rev-parse --show-toplevel` → repo root (absolute path)
2. basename of that → project name
3. read `host` + `user` from `~/.autogitrc`
4. suggest `git@<host>:<user>/<project>.git`

Press **Enter** to accept the suggestion, or type a different URL
(GitLab, HTTPS, different org — anything git understands).

## Files it touches

| Path | Purpose |
|------|---------|
| `~/.autogitrc` | `user=…` and `host=…` — created once |
| `.git/description` | Overwritten with your short description when the remote is added or `-d` is passed |
| A temp file in `$TMPDIR` / `%TEMP%` | Commit message; deleted immediately after commit |

## Notes

- Commit messages are passed via `git commit -F <tempfile>`, so `"`, `'`,
  `$`, backticks and `&` in your description won't break the shell.
- The `push -u` is what prevents "No configured push destination" from
  ever happening again — after the first push, git remembers the tracking
  branch.
- If you're on Windows, run `mingw32-make`. `make` alone may not exist.

## License

MIT (or whatever you prefer — it's ~250 lines of C).
```

Drop that as `README.md` next to `autogit.c` and the `Makefile`, and you've got a self-documenting repo. If you're pushing it to GitHub, you might also want a one-line `LICENSE` file — MIT's text is standard, or just swap the license section for whatever you actually intend.