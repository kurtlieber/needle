#!/bin/bash
# Install needle on Arch Linux or Omarchy from its pacman repository on GitHub Releases (README, "Install"):
#   curl -fsSL https://raw.githubusercontent.com/kurtlieber/needle/main/install.sh | bash
# It trusts needle's signing key, adds the [needle] repository, and installs needle. After that, needle updates
# with the rest of the system. Safe to run again.
set -euo pipefail

KEY=E5278B2148A2833AB898C69FC5A7379D20ECB792
REPO=https://github.com/kurtlieber/needle/releases/latest/download
CONF=/etc/pacman.d/needle.conf
HOOK="$HOME/.config/omarchy/hooks/pre-refresh-pacman.d/needle-repo"

main() {
  if ! command -v pacman >/dev/null; then
    echo "This installer is for Arch Linux and Omarchy. Other systems: https://github.com/kurtlieber/needle#install" >&2
    exit 1
  fi
  if [ "$(uname -m)" != x86_64 ]; then
    echo "needle is built for x86_64 only." >&2
    exit 1
  fi

  echo "==> Trusting needle's signing key $KEY"
  if ! sudo pacman-key --list-keys "$KEY" >/dev/null 2>&1; then
    local key
    key="$(mktemp)"
    curl -fsSL "$REPO/needle-signing-key.asc" -o "$key"
    sudo pacman-key --add "$key"
    rm -f "$key"
  fi
  sudo pacman-key --lsign-key "$KEY" >/dev/null

  echo "==> Adding the [needle] repository ($CONF)"
  printf '[needle]\nSigLevel = Required\nServer = %s\n' "$REPO" | sudo tee "$CONF" >/dev/null
  grep -qxF "Include = $CONF" /etc/pacman.conf || printf '\nInclude = %s\n' "$CONF" | sudo tee -a /etc/pacman.conf >/dev/null

  # Omarchy rewrites /etc/pacman.conf when it refreshes its package configuration; this hook adds needle back.
  if [ -d "$HOME/.config/omarchy" ]; then
    echo "==> Keeping it through Omarchy's pacman refreshes ($HOOK)"
    mkdir -p "$(dirname "$HOOK")"
    cat > "$HOOK" <<'EOF'
#!/bin/bash
# From needle's install.sh: puts the [needle] repository back after Omarchy rewrites /etc/pacman.conf.
[[ -r /etc/pacman.d/needle.conf ]] || exit 0
grep -qxF 'Include = /etc/pacman.d/needle.conf' /etc/pacman.conf && exit 0
printf '\nInclude = /etc/pacman.d/needle.conf\n' | sudo tee -a /etc/pacman.conf >/dev/null
EOF
  fi

  echo "==> Installing needle (pacman -Syu: Arch upgrades everything together)"
  sudo pacman -Syu --needed needle </dev/tty
  echo "Done. Next: connect needle to haystack (README, \"Connect to haystack\")."
}

main "$@"
