# LuCI Apple Console Visual Design

Date: 2026-06-29
Status: approved direction, pending implementation plan

## Goal

Refresh `drcom/files/usr/lib/lua/luci/view/drcom/form.htm` so the DrCOM LuCI page feels like a calm Apple system-settings control surface: modern, precise, readable, and clearly task-oriented without becoming decorative or heavy.

The design must keep the existing LuCI workflow intact: users should still be able to edit configuration, save and restart, toggle autostart, inspect service status, diagnose validation issues, and read logs from one page.

## Chosen Approach

Use **Approach A: restrained Apple System Settings style**.

This means the interface should feel polished through hierarchy, spacing, material surfaces, semantic color, and consistent controls rather than through loud gradients, neon terminal styling, heavy glassmorphism, or landing-page-like hero treatment.

## Users and Context

Primary users are OpenWrt users managing DrCOM authentication on routers in dorms, labs, and campus network environments. They may be troubleshooting under time pressure, on older devices, and in Chinese or English LuCI sessions. The page must remain fast, legible, and self-contained.

## Design Principles

1. **Status first** — service state, autostart state, config health, and logs must be immediately scannable.
2. **Apple-like restraint** — use familiar system typography, quiet surfaces, and measured semantic color.
3. **Router-friendly polish** — no external dependencies, no animation-heavy effects, and conservative CSS for LuCI/browser compatibility.
4. **Recovery guidance** — warnings and errors should point users toward likely next checks.
5. **Respect density** — modernize the page without hiding the configuration and operational details power users need.

## Visual System

### Color

Keep the Apple semantic palette already present in the page:

- Blue for primary action and information.
- Green for healthy/running states.
- Orange for warnings or pending attention.
- Red for stopped, failed, or dangerous actions.
- System gray backgrounds for surfaces and page chrome.

Reduce decorative gradient usage. Backgrounds should use quiet system-gray depth rather than colorful surface effects. Accent color should appear on primary buttons, focus rings, badges, and state indicators only.

### Typography

Use the existing Apple/system font stack:

```css
-apple-system, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", sans-serif
```

Tighten the product UI scale:

- Page title: strong but not oversized.
- Panel headings: compact, consistent, and easy to scan.
- Labels and metadata: high enough contrast to pass WCAG AA.
- Long help copy: comfortable line height and readable width.

### Shape and Material

Move away from uniform `8px` rounding everywhere. Use a small hierarchy:

- Page shell / large panels: about 18–22px radius.
- Standard cards and editors: about 16–18px radius.
- Inputs and buttons: about 10–14px radius.
- Pills and badges: fully rounded.

Use material effects sparingly. Backdrop blur may remain where supported, but the design must still look good without it.

## Layout

Keep the current macro-layout:

1. Header / service console summary.
2. Status strip for service, autostart, and configuration health.
3. Sticky action toolbar.
4. Main workspace with configuration editor and side stack.
5. Observability area for diagnostics and logs.

Improve it by:

- Increasing rhythm between major sections.
- Making the header feel like an Apple settings pane rather than a dashboard hero.
- Reducing repeated card treatment where simple grouped rows communicate better.
- Preserving responsive behavior: desktop two-column layout, narrow screens collapse to one column.

## Components

### Header

The header should be calm and functional:

- Remove or soften decorative top color strips.
- Keep title, subtitle, and locale selector.
- Use a subtle material surface and optional ambient background glow only if it does not distract.

### Status Cards

Status cards should read as grouped system status summaries:

- Clear heading.
- Badge or state chip.
- Short supporting metadata.
- Avoid thick decorative color strips.
- Use semantic color on chips and small indicators, not on entire large surfaces.

### Toolbar and Buttons

Buttons should share one consistent vocabulary:

- Primary action: filled Apple blue.
- Secondary actions: neutral material buttons.
- Danger actions: restrained red tint, not aggressive by default.
- Hover/focus: visible focus ring and subtle lift or surface change.
- Disabled/loading: visually clear without relying only on opacity.

### Config Editor

The editor should feel stable and precise:

- Clear border and focus state.
- Comfortable monospace text rendering.
- Background distinct from the page but not stark.
- Preserve current save/restart workflow.

### Diagnostics and Logs

Diagnostics should use readable grouped rows and clear semantic states.

The log panel may remain dark because logs benefit from terminal contrast, but it should be visually integrated with the rest of the page through radius, header treatment, spacing, and controls.

## Accessibility and Motion

- Maintain WCAG AA contrast for body text, metadata, controls, and placeholders.
- Keep keyboard focus states obvious.
- Do not rely on color alone for state; retain text labels and badges.
- Respect `prefers-reduced-motion`.
- Use only short product-style transitions for state changes, hover, focus, and control feedback.

## Implementation Scope

Primary file:

- `drcom/files/usr/lib/lua/luci/view/drcom/form.htm`

Validation file likely needing updates if class or expected CSS/text markers change:

- `scripts/tests/luci_static_smoke.py`

Implementation should avoid backend/controller changes unless the existing markup requires a small class or structure adjustment for visual clarity.

## Testing Plan

1. Run the existing LuCI static smoke test.
2. Check that required IDs, actions, language strings, and core class hooks remain present.
3. Inspect the rendered page if feasible through a browser or local preview.
4. Manually review responsive behavior in desktop and narrow widths.
5. Verify light and dark mode CSS paths remain present.

## Non-goals

- No new frontend build system.
- No external CSS or JavaScript dependencies.
- No large interaction redesign.
- No backend service behavior changes.
- No decorative animation sequence or landing-page hero treatment.
