# LuCI Apple Console Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refresh the DrCOM LuCI page into a restrained Apple System Settings-style control console while preserving existing configuration, status, diagnostics, and log workflows.

**Architecture:** This is a self-contained LuCI view refinement. The CSS and small markup vocabulary inside `form.htm` remain dependency-free and backend-compatible; `scripts/tests/luci_static_smoke.py` enforces the static contract for required hooks, Apple design tokens, light/dark mode, and reduced-motion support.

**Tech Stack:** LuCI Lua template HTML, inline CSS, vanilla JavaScript, Python static smoke test.

## Global Constraints

- Primary implementation file is `drcom/files/usr/lib/lua/luci/view/drcom/form.htm`.
- Validation file is `scripts/tests/luci_static_smoke.py`.
- No new frontend build system.
- No external CSS or JavaScript dependencies.
- No large interaction redesign.
- No backend service behavior changes.
- No decorative animation sequence or landing-page hero treatment.
- Preserve existing IDs, service action buttons, translation keys, language strings, and core class hooks.
- Maintain light mode, dark mode, and `prefers-reduced-motion` CSS paths.
- Keep WCAG AA-oriented contrast by using darker metadata text and clear focus rings.

---

## File Structure

- `PRODUCT.md` — project design context written during the approved design flow; no further changes required for implementation.
- `docs/superpowers/specs/2026-06-29-luci-apple-console-design.md` — approved design spec; no further changes required unless implementation uncovers a contradiction.
- `docs/superpowers/plans/2026-06-29-luci-apple-console.md` — this implementation plan.
- `drcom/files/usr/lib/lua/luci/view/drcom/form.htm` — inline CSS and existing LuCI markup for the Apple-inspired visual refresh.
- `scripts/tests/luci_static_smoke.py` — static assertions that protect required UI hooks and new visual-system markers.

---

### Task 1: Lock the Apple visual-system contract

**Files:**
- Modify: `scripts/tests/luci_static_smoke.py:29-50`
- Test: `scripts/tests/luci_static_smoke.py`

**Interfaces:**
- Consumes: the current `form.htm` text loaded as `view` in the smoke test.
- Produces: static contract assertions that Task 2 must satisfy.

- [ ] **Step 1: Add failing assertions for the visual refresh contract**

Replace the design-token assertion block in `scripts/tests/luci_static_smoke.py` after the existing `data-theme-mode="system"` assertion with this exact block:

```python
    assert_contains(view, 'data-theme-mode="system"')
    assert_contains(view, "color-scheme: light;")
    assert_contains(view, "@media (prefers-color-scheme: dark)")
    assert_contains(view, "@supports ((-webkit-backdrop-filter: blur(18px)) or (backdrop-filter: blur(18px)))")
    assert_contains(view, "@media (prefers-reduced-motion: reduce)")
    assert_contains(view, 'font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text"')
    assert_contains(view, "--jl-radius-page: 22px")
    assert_contains(view, "--jl-radius-panel: 18px")
    assert_contains(view, "--jl-radius-control: 12px")
    assert_contains(view, "--jl-material:")
    assert_contains(view, "--jl-system-blue: #007aff")
    assert_contains(view, "--jl-surface-elevated:")
    assert_contains(view, "text-wrap: balance")
    assert_contains(view, "box-shadow: var(--jl-shadow-control)")
    assert_not_contains(view, "border-left: 3px solid")
```

- [ ] **Step 2: Run the smoke test and verify it fails before implementation**

Run:

```bash
python3 scripts/tests/luci_static_smoke.py
```

Expected: FAIL because `--jl-radius-page: 22px`, `--jl-surface-elevated:`, `text-wrap: balance`, `--jl-shadow-control`, and removal of `border-left: 3px solid` are not all true in the current view.

- [ ] **Step 3: Commit the contract test**

Run:

```bash
git add scripts/tests/luci_static_smoke.py docs/superpowers/specs/2026-06-29-luci-apple-console-design.md docs/superpowers/plans/2026-06-29-luci-apple-console.md PRODUCT.md
git commit -m "test: lock luci apple console design contract"
```

Expected: a commit is created containing the design context, spec, plan, and failing static contract. If the test file already contains equivalent assertions from previous work, commit only the actual changed files.

---

### Task 2: Apply restrained Apple System Settings styling

**Files:**
- Modify: `drcom/files/usr/lib/lua/luci/view/drcom/form.htm:4-616`
- Test: `scripts/tests/luci_static_smoke.py`

**Interfaces:**
- Consumes: the assertions from Task 1.
- Produces: a dependency-free LuCI view with Apple-like radii, calmer material surfaces, improved status grouping, accessible focus states, integrated editor/log panels, and no side-stripe callout border.

- [ ] **Step 1: Replace the CSS token block at the top of `.jludrcom-page`**

In `drcom/files/usr/lib/lua/luci/view/drcom/form.htm`, update `.jludrcom-page` so it includes these tokens and base values while preserving existing semantic color names:

```css
		.jludrcom-page {
			--jl-system-blue: #007aff;
			--jl-system-green: #34c759;
			--jl-system-orange: #ff9500;
			--jl-system-red: #ff3b30;
			--jl-system-cyan: #32ade6;
			--jl-bg: #f5f5f7;
			--jl-surface-elevated: #ffffff;
			--jl-material: rgba(255, 255, 255, 0.82);
			--jl-material-strong: rgba(255, 255, 255, 0.94);
			--jl-field: rgba(255, 255, 255, 0.90);
			--jl-panel-soft: rgba(242, 244, 248, 0.82);
			--jl-border: rgba(60, 60, 67, 0.16);
			--jl-border-strong: rgba(60, 60, 67, 0.26);
			--jl-text: #1d1d1f;
			--jl-muted: rgba(29, 29, 31, 0.70);
			--jl-ok: #248a3d;
			--jl-warn: #a65300;
			--jl-danger: #d70015;
			--jl-info: #0066cc;
			--jl-accent: var(--jl-system-blue);
			--jl-accent-soft: rgba(0, 122, 255, 0.11);
			--jl-ok-soft: rgba(52, 199, 89, 0.14);
			--jl-warn-soft: rgba(255, 149, 0, 0.16);
			--jl-danger-soft: rgba(255, 59, 48, 0.12);
			--jl-info-soft: rgba(50, 173, 230, 0.14);
			--jl-editor-bg: rgba(255, 255, 255, 0.96);
			--jl-terminal: #111318;
			--jl-terminal-text: #f5f5f7;
			--jl-radius-page: 22px;
			--jl-radius-panel: 18px;
			--jl-radius-control: 12px;
			--jl-shadow: 0 22px 54px rgba(0, 0, 0, 0.10), 0 2px 10px rgba(0, 0, 0, 0.06);
			--jl-shadow-soft: 0 10px 28px rgba(0, 0, 0, 0.08);
			--jl-shadow-control: 0 1px 1px rgba(0, 0, 0, 0.04), 0 6px 14px rgba(0, 0, 0, 0.06);
			--jl-ring: rgba(0, 122, 255, 0.30);
			color-scheme: light;
			background: linear-gradient(180deg, #fbfbfd 0%, var(--jl-bg) 58%, #eef1f6 100%);
			color: var(--jl-text);
			font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", sans-serif;
			padding: 22px;
			border-radius: var(--jl-radius-page);
		}
```

Also update the dark-mode token block with matching dark surface values:

```css
				--jl-bg: #0f1115;
				--jl-surface-elevated: #1c1c1e;
				--jl-material: rgba(28, 28, 30, 0.78);
				--jl-material-strong: rgba(36, 36, 38, 0.92);
				--jl-field: rgba(44, 44, 46, 0.86);
				--jl-panel-soft: rgba(58, 58, 60, 0.38);
				--jl-border: rgba(235, 235, 245, 0.14);
				--jl-border-strong: rgba(235, 235, 245, 0.24);
				--jl-text: #f5f5f7;
				--jl-muted: rgba(235, 235, 245, 0.72);
				--jl-editor-bg: rgba(28, 28, 30, 0.90);
				--jl-terminal: #05060a;
				--jl-terminal-text: #f5f5f7;
				--jl-shadow: 0 22px 60px rgba(0, 0, 0, 0.44), 0 2px 12px rgba(0, 0, 0, 0.34);
				--jl-shadow-soft: 0 10px 34px rgba(0, 0, 0, 0.34);
				--jl-shadow-control: 0 1px 1px rgba(0, 0, 0, 0.28), 0 8px 18px rgba(0, 0, 0, 0.28);
```

- [ ] **Step 2: Update shared panel/card surfaces and header treatment**

Use `border-radius: var(--jl-radius-panel);`, add calmer panel backgrounds, remove the top rainbow strip from `.jludrcom-console-head::before`, and add `text-wrap: balance` to headings:

```css
		.jludrcom-console-head,
		.jludrcom-card,
		.jludrcom-editor,
		.jludrcom-panel,
		.jludrcom-callout,
		.jludrcom-config-example {
			background: var(--jl-material);
			border: 1px solid var(--jl-border);
			border-radius: var(--jl-radius-panel);
			box-shadow: var(--jl-shadow);
		}

		.jludrcom-console-head {
			position: relative;
			overflow: hidden;
			display: flex;
			align-items: center;
			justify-content: space-between;
			gap: 18px;
			padding: 22px 24px;
			margin-bottom: 18px;
			background: linear-gradient(135deg, var(--jl-material-strong), var(--jl-material));
		}

		.jludrcom-console-head::before {
			content: "";
			position: absolute;
			inset: -80px -40px auto auto;
			width: 220px;
			height: 220px;
			border-radius: 999px;
			background: radial-gradient(circle, rgba(0, 122, 255, 0.13), transparent 68%);
			pointer-events: none;
		}

		.jludrcom-console-head h2,
		.jludrcom-card h3,
		.jludrcom-editor h3,
		.jludrcom-panel h3 {
			text-wrap: balance;
		}
```

- [ ] **Step 3: Replace status strip side/top stripes with Apple-style state dots**

Remove the current `.jludrcom-status-strip .jludrcom-card::before` top color strip rules and replace them with this dot-based system:

```css
		.jludrcom-status-strip .jludrcom-card {
			position: relative;
			overflow: hidden;
			box-shadow: var(--jl-shadow-soft);
			background: linear-gradient(180deg, var(--jl-material-strong), var(--jl-material));
		}

		.jludrcom-kpi h3::before {
			content: "";
			display: inline-block;
			width: 8px;
			height: 8px;
			margin-right: 8px;
			border-radius: 999px;
			background: var(--jl-border-strong);
			box-shadow: 0 0 0 4px var(--jl-panel-soft);
			vertical-align: 1px;
		}

		.jludrcom-status-strip .jludrcom-card:nth-child(1) .jludrcom-kpi h3::before { background: var(--jl-accent); }
		.jludrcom-status-strip .jludrcom-card:nth-child(2) .jludrcom-kpi h3::before { background: var(--jl-warn); }
		.jludrcom-status-strip .jludrcom-card:nth-child(3) .jludrcom-kpi h3::before { background: var(--jl-info); }
```

- [ ] **Step 4: Refine controls, editor, callout, and log surfaces**

Update buttons, selects, textarea, callout, config sample, and log output with the following principles in code:

```css
		.jludrcom-select,
		.jludrcom-button,
		.jludrcom-toggle,
		.jludrcom-refresh {
			border-radius: var(--jl-radius-control);
			box-shadow: var(--jl-shadow-control);
		}

		.jludrcom-button.cbi-button-apply {
			background: linear-gradient(180deg, #0a84ff, var(--jl-accent));
			border-color: rgba(0, 102, 204, 0.72);
			color: #ffffff;
			box-shadow: 0 10px 22px rgba(0, 122, 255, 0.24);
		}

		.jludrcom-button.is-danger {
			background: var(--jl-danger-soft);
			border-color: rgba(255, 59, 48, 0.28);
			color: var(--jl-danger);
			box-shadow: none;
		}

		.jludrcom-callout {
			padding: 16px;
			box-shadow: none;
			background: linear-gradient(180deg, var(--jl-material-strong), var(--jl-panel-soft));
		}

		#conf,
		.jludrcom-config-example-code,
		#jludrcom-log-output {
			border-radius: var(--jl-radius-control);
		}

		#conf {
			padding: 14px;
			background: var(--jl-editor-bg);
			box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.10), var(--jl-shadow-control);
		}

		#jludrcom-log-output {
			border-color: rgba(235, 235, 245, 0.12);
			box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.06);
		}
```

Ensure the old line `border-left: 3px solid var(--jl-info);` is removed completely.

- [ ] **Step 5: Run the smoke test and verify it passes**

Run:

```bash
python3 scripts/tests/luci_static_smoke.py
```

Expected: no output and exit code 0.

- [ ] **Step 6: Commit the styling implementation**

Run:

```bash
git add drcom/files/usr/lib/lua/luci/view/drcom/form.htm scripts/tests/luci_static_smoke.py
git commit -m "style: refine luci console with apple system design"
```

Expected: a commit is created for the implementation.

---

### Task 3: Verify release readiness and push

**Files:**
- Read/verify: `drcom/Makefile`
- Verify: `git status`, `git log`, `scripts/tests/luci_static_smoke.py`

**Interfaces:**
- Consumes: committed implementation from Task 2.
- Produces: pushed GitHub branch/main update and a clear release-readiness report.

- [ ] **Step 1: Run final static verification**

Run:

```bash
python3 scripts/tests/luci_static_smoke.py
```

Expected: no output and exit code 0.

- [ ] **Step 2: Review the working tree**

Run:

```bash
git status --short
git log --oneline -3
```

Expected: `git status --short` is empty after commits, and the last commits include the design contract and Apple console styling.

- [ ] **Step 3: Push to GitHub**

Because the user explicitly requested pushing to GitHub and sending a new version, push the current branch:

```bash
git push origin main
```

Expected: push succeeds.

- [ ] **Step 4: Trigger or confirm new version publication**

Check repository release automation. If this project publishes via tags or GitHub Actions on version bumps, follow the repository's existing release mechanism without inventing a new one. If no local release command exists, report that the code was pushed and identify the next required release step.

Run these inspection commands:

```bash
find .github -maxdepth 3 -type f -print
git tag --list --sort=-v:refname | head -5
```

Expected: release workflow and recent tags are visible, or a clear absence of automation is reported.
```
