# Website documentation rendering

The `website` branch holds the static Player12 showcase. Maintained contributor,
player and developer guides live on the source branch; regenerate their HTML
with `scripts/build_website_docs.py` so fixes reach both readers and contributors.
Use a separate checkout for the website. The generator refuses to overwrite the
source checkout, and leaves the showcase pages, stylesheet, JavaScript and game
captures under the website branch's control.

```sh
python3 scripts/build_website_docs.py --site /tmp/fm-website
```

This command requires Python's `markdown` package. Install Graphviz to render the
supported Mermaid flowcharts; when `dot` is unavailable, their source remains a
readable fenced code block. The `GUIDES` collection in the script defines the
documentation navigation and searchable index. Add a guide there to publish it.
Relative links between registered guides become website links; source references
link to GitHub, and game screenshots use the compressed website media.

## Lists and code examples

Leave a blank line between an introductory paragraph and its list. This makes
lists render consistently on GitHub and with Python Markdown:

```markdown
Before opening a pull request:

1. Run the relevant checks.
2. Explain the behavior you changed.
```

The generator also repairs missing blanks before top-level lists. It preserves
fenced examples and list indentation so commands and nested instructions keep
their meaning. Prefer correcting the maintained Markdown when a rendering
problem originates there.

## Diagram sizing and themes

The generator supports the simple `flowchart LR` and `flowchart TD` diagrams used
in the engine guides: named nodes with square-bracket labels and `-->` edges.
It wraps labels before asking Graphviz to size nodes, then embeds the generated
SVG in the page. SVG text uses the website's `--ink`, nodes use `--surface`, and
edges use `--accent`; switching themes therefore updates diagram contrast too.

Long flows keep their natural width in a horizontally scrollable
`.diagram-viewport`. Avoid fitting every diagram to the article width: doing so
makes a seven-stage flow unreadable on a phone. The viewport is keyboard
focusable, and each diagram includes a text description and expandable source.
Graphviz output is embedded directly, avoiding duplicate standalone assets.

When changing the renderer or website stylesheet, preview narrow and wide
viewports in both themes. Confirm the page itself does not scroll sideways,
diagram labels stay inside their nodes, and scrollable diagrams and tables can
be reached with the keyboard. Check the AI guidelines' reviewer and agent
expectations as real ordered lists, rather than a paragraph containing numbers.

## Rendering repair walkthrough

The October 2026 rendering repair corrected missing list boundaries in
`AI_GUIDELINES.md` and added conservative list normalization to the generator.
It replaced uniformly scaled diagram images with themed inline SVG and wrapped,
consistently sized labels. Focused checks verified the four agent expectations
render as four ordered items, fenced examples and nested lists retain their
structure, and both engine flowcharts have wrapped labels sized to 16 CSS pixels. Website
browser checks cover the final CSS layout and theme integration.

## Media and publication size

The website uses WebP screenshots and short H.264/VP9 clips exported from the
real game. Keep raw PNG sequences and capture tools outside the website
checkout. Retain legible interface text when compressing; a smaller file is
not useful if the screen can no longer be read. MP4 files use fast-start
metadata, and videos wait for playback on mobile, reduced-motion and
connection data-saving preferences.

The source branch's documentation workflow renders the guides and runs
`scripts/optimize_website.cjs` with pinned build-time HTML/CSS/JS tools. The
website's `scripts/check_site.py` validates published links, image descriptions
and the media budget. Push the prepared website branch before source changes
that need its new layout; the sync workflow checks for that site's validator
before publishing documentation.

The website and documentation retain the existing GitHub repository URL.
Player12's public executable and package names changed; the CMake target and
save-directory identities remain compatible with earlier builds.
