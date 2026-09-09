# Version Control Conventions

## Branches

| Branch | Purpose | Protected |
|--------|---------|-----------|
| `main` | Stable, always buildable | Yes |
| `feat/<short-name>` | New feature in progress | No |
| `fix/<short-name>` | Bug fix in progress | No |
| `docs/<short-name>` | Docs only | No |

Branch off `main`. PR or merge back into `main` when:
- All tasks in the plan are committed
- PlatformIO build passes (`pio run`)
- Tests pass (`pio test` if applicable)
- Commit history is clean (squash fixups)

## Commits — Conventional Commits

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**

| Type | When |
|------|------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `refactor` | Code change that neither fixes bug nor adds feature |
| `test` | Add or fix tests |
| `chore` | Build, CI, deps, tooling |
| `perf` | Performance |

**Scope examples**: `ht7017`, `meter`, `web`, `mqtt`, `config`, `energystore`

**Subject**: imperative, lowercase, no period, ≤ 72 chars

## Examples

```bash
git commit -m "feat(ht7017): add UART read/write register API"
git commit -m "feat(meter): implement dual-channel U/I/P readout"
git commit -m "fix(ht7017): correct checksum byte order in write frame"
git commit -m "docs(prd): add dual-channel meter product requirements"
git commit -m "chore: update .gitignore for .pio artifacts"
```

## Task = Commit

Each plan task ends with one commit. The plan lists the exact commit message
inside the task body — copy it verbatim so history stays consistent.

## Tags

```
v0.1.0  M1: HT7017 driver complete
v0.2.0  M2: Config + EnergyStore complete
v0.3.0  M3: Web provisioning complete
v0.4.0  M4: MQTT complete
v0.5.0  M5: Integration stable
```

## Pre-commit Checklist (per commit)

- [ ] `pio run` builds clean (or note why not)
- [ ] No debug `Serial.println` left in production code
- [ ] No commented-out blocks (delete; git remembers)
- [ ] Header has matching `#endif // _HEADER_GUARD_` comment