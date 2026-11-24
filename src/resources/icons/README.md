# Built-in workbench icons

The eight SVGs in this directory are Microsoft's [Codicons](https://github.com/microsoft/vscode-codicons),
the same artwork gusu ships for its own icons. Keeping one icon family means the workbench chrome and
the application's tool bars share a 16x16 grid, stroke weight and corner treatment.

| File (`qcw::Icon`) | Codicon |
|---|---|
| `perspective.svg` (`Perspective`) | `layout` |
| `view.svg` (`View`) | `window` |
| `new_perspective.svg` (`NewPerspective`) | `new-collection` |
| `new_view.svg` (`NewView`) | `new-session` |
| `folder_maximize.svg` (`FolderMaximize`) | `chrome-maximize` |
| `folder_restore.svg` (`FolderRestore`) | `chrome-restore` |
| `folder_minimize.svg` (`FolderMinimize`) | `chrome-minimize` |
| `tab_list.svg` (`TabList`) | `list-unordered` |

The file names follow `qcw::Icon` rather than codicons, so `QcwThemeIcons` keeps a stable
resource path per enum value; the table above is the mapping to re-apply when the artwork is
refreshed.

## License and attribution

Codicons are licensed under **CC BY 4.0**; the font/build tooling of the upstream repository is
MIT. QCW's own code stays BSD-3-Clause (see the repository `LICENSE`); this directory is the
attribution record for the vendored artwork.

> Icons: *Codicons* by Microsoft Corporation, licensed under CC BY 4.0
> (<https://creativecommons.org/licenses/by/4.0/>).
> Source: <https://github.com/microsoft/vscode-codicons>, `src/icons/*.svg`.

Distributions that ship QCW must keep this notice. No modifications are made to the artwork:
the SVGs are copied byte for byte, and `qcw::themeIcon()` substitutes `currentColor` at paint
time to follow the palette, which is the usage the icons are designed for.

## Refreshing

Copy `<codicon>.svg` from upstream `src/icons/` over the file listed above, then rebuild. The
upstream archive is about 0.8 MB; do not add the repository as a submodule and do not vendor the
generated icon font.
