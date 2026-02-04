# Vim-Style Screenshot Tool

A **keyboard-only screenshot utility** for people who hate dragging rectangles with a mouse.

This tool lets you select a screen region using **Vim-like keybindings** and then take a screenshot.  
Fast, precise, repeatable. Built for terminal-first workflows.

---

## Why This Exists

Mouse-based screenshot tools have problems:

- Imprecise dragging
- Breaks keyboard flow
- Hard to adjust selection once started
- Annoying for tiling window managers and Wayland users

This tool treats screenshot selection as a **controlled operation**, not a gesture.

Think of it as:
> Vim, but for selecting pixels instead of text.

---

## Who Is This For

- Vim users
- Terminal-first users
- Tiling WM users (sway, hyprland, i3)
- Wayland users tired of GUI screenshot tools
- Anyone who values precision over convenience

If you rely on a mouse for everything, this tool is not for you.

---

## How It Works (Mental Model)

1. You start the tool from the terminal
2. A cursor appears (logical, not a mouse)
3. You move using Vim keys (`h j k l`)
4. You enter **Visual mode**
5. You expand/shrink the selection using keys
6. You confirm
7. Screenshot is taken

No dragging. No guessing.

---

## Modes

### Normal Mode
- Default mode
- Cursor movement only
- No selection yet

### Visual Mode
- Active selection
- Moving keys change the rectangle size
- Similar to Vim’s Visual mode

### Confirm
- Finalizes selection
- Passes geometry to screenshot backend
- Exits cleanly

---

## Keybindings

| Key | Action |
|----:|--------|
| h | Move left |
| j | Move down |
| k | Move up |
| l | Move right |
| v | Enter Visual mode |
| Enter | Confirm selection |
| Esc | Cancel and exit |

Bindings are intentionally minimal and predictable.

---

## Typical Workflow

```text
1. Run the tool
2. Move cursor to top-left corner
3. Press v (enter Visual mode)
4. Expand selection with h/j/k/l
5. Press Enter
6. Screenshot is captured
````

This workflow is fast once muscle memory kicks in.

---

## Screenshot Backend

This tool **does not reinvent screenshot capturing**.

It only does **selection**.

The final rectangle is passed to:

* Wayland tools (like grim-style geometry)
* X11 tools
* Or any custom script

This separation keeps the tool simple and composable.

---

## Why Keyboard Selection Is Better

* Pixel-perfect control
* No shaky hands
* Easy micro-adjustments
* Works the same every time
* Scriptable and automatable

Once you get used to it, mouse-based selection feels primitive.

---

## Platform Support

* Linux
* Wayland and X11 (backend-dependent)
* Terminal required

GUI toolkits are intentionally avoided.

---

## Status

This is an actively evolving utility.
Expect rough edges, intentional limitations, and a focus on correctness over polish.

---

## Final Thought

If you already think in Vim motions, this tool will feel obvious.
If you don’t, it will feel hostile at first.

That discomfort is the learning curve of precision.

