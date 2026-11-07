# Affix Architecture

Affix is layered as public API headers (`include/affix/`), platform shims (`src/platform/`), feature implementations (`src/*/`), and thin executables (`affix-cli`, `affix-bench`).

Placement state never persists past `ScopedPlacement` scope exit. Observability is read-only and hot-path safe; hardware PMU counters degrade gracefully to zero when unprivileged.
