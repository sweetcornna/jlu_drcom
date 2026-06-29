# Product

## Register

product

## Users

OpenWrt users who run DrCOM authentication on routers in dorms, labs, or campus network environments. They use LuCI to configure credentials and network parameters, restart the service, inspect runtime status, and diagnose logs without needing to keep a desktop client running.

## Product Purpose

This project provides a maintainable OpenWrt DrCOM client service based on dogcom, bundled with configuration templates, a procd service, release packaging, and a LuCI control panel. The LuCI interface should make the common loop—configure, save, restart, verify status, inspect logs, and recover from network/authentication errors—clear and low-stress on resource-constrained routers.

Success means a user can understand whether authentication is running, see what needs attention, safely edit `/etc/drcom.conf`, and use logs or diagnostics to identify campus-network issues such as static WAN configuration, UDP timeouts, stale sessions, or invalid values.

## Brand Personality

Calm, precise, readable. The interface should feel close to Apple system settings and network panels: quiet, trustworthy, polished, and task-focused rather than decorative.

## Anti-references

Do not make the LuCI page look like a neon terminal, a gaming dashboard, a heavy glassmorphism demo, or a generic SaaS landing page. Avoid over-saturated inactive states, ornamental gradients, animated spectacle, and visual effects that reduce readability on older router browsers.

## Design Principles

1. **Status first, decoration second** — service state, autostart state, validation, and logs must be immediately legible before any visual flourish.
2. **Apple-like restraint** — use familiar system typography, calm material surfaces, measured spacing, and semantic color only where it helps the task.
3. **Router-friendly polish** — keep CSS self-contained, dependency-free, responsive, and conservative enough for LuCI environments and embedded devices.
4. **Guide recovery** — errors and warnings should explain likely causes and next actions, not merely report failure.
5. **Respect density** — the page should feel modern without hiding the configuration, status, and log information power users need.

## Accessibility & Inclusion

Target WCAG AA contrast for text and controls. Preserve keyboard focus states, avoid color-only status communication, support system dark mode and reduced motion, and keep Chinese and English copy equally usable without relying on narrow fixed layouts.
