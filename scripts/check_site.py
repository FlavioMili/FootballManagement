#!/usr/bin/env python3
"""Check static pages without a game build or browser download."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import sys

ROOT = Path(__file__).resolve().parents[1]


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []
        self.ids = set()
        self.heading_count = 0
        self.title_count = 0
        self.in_head = False
        self.images_without_alt = 0
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'h1': self.heading_count += 1
        if tag == 'head': self.in_head = True
        if tag == 'title' and self.in_head: self.title_count += 1
        if 'id' in attrs: self.ids.add(attrs['id'])
        if tag == 'img' and 'alt' not in attrs: self.images_without_alt += 1
        for key in ('href', 'src', 'poster'):
            if attrs.get(key): self.links.append(attrs[key])

    def handle_endtag(self, tag):
        if tag == 'head': self.in_head = False


def main():
    files = [p for p in ROOT.rglob('*.html') if not {'api', '.git'} & set(p.parts)]
    pages = {}
    errors = []
    checked = 0
    for file in files:
        page = Page()
        page.feed(file.read_text())
        pages[file.resolve()] = page
        if page.title_count != 1: errors.append(f'{file.relative_to(ROOT)}: expected one title')
        if page.heading_count != 1: errors.append(f'{file.relative_to(ROOT)}: expected one main heading')
        if page.images_without_alt: errors.append(f'{file.relative_to(ROOT)}: image without alt')
    for file, page in pages.items():
        for value in page.links:
            url = urlsplit(value)
            if url.scheme or url.netloc: continue
            target = (file.parent / unquote(url.path)).resolve() if url.path else file
            if target.is_dir(): target /= 'index.html'
            checked += 1
            if not target.exists(): errors.append(f'{file.relative_to(ROOT)}: missing {value}')
            elif url.fragment and target in pages and unquote(url.fragment) not in pages[target].ids:
                errors.append(f'{file.relative_to(ROOT)}: missing anchor {value}')
    media = sum(file.stat().st_size for file in (ROOT / 'media/game').glob('*') if file.is_file())
    if media > 9_000_000: errors.append(f'Media budget exceeded: {media} bytes')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'{len(files)} pages, {checked} local references and image accessibility checked; media {media / 1_000_000:.2f} MB')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
