# EposLib documentation

Source of the documentation site for [EposLib](https://github.com/Imcab/EposLib),
published at https://imcab.github.io/EposLibDocs/.

Kept apart from the library so that cloning EposLib onto a robot does not pull
the documentation with it.

## Preview locally

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/mkdocs serve          # http://127.0.0.1:8000
```

## Publish

```bash
.venv/bin/mkdocs gh-deploy      # builds and pushes to the gh-pages branch
```

## Layout

- `mkdocs.yml` - site configuration (MkDocs Material, light scheme only)
- `docs/index.md` - the landing page
- `docs/stylesheets/extra.css` - the look: white page, dark red, serif type
