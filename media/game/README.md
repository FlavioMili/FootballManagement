# Player12 game captures

Captured 2026-10-08 through `ShowcaseCaptures.ReadmeScreens` in the source
checkout, from a real career generated with world seed 20250702. The managed
club is Parma, selected from the middle of the league by reputation; screen
text is English and the game's slate theme follows the club's purple accent.
The website uses the game's base green accent independently of club colors.

There are twelve gallery screens and two silent 15-second match clips.
Each clip consists of 450 consecutive actual-renderer frames at 30 fps and 1×
playback. The capture tool uses a copied engine to locate an upcoming chance,
then records the live match normally. No synthetic frames or gameplay were added.

To recreate from a test-enabled game checkout:

```sh
cmake --build build --target showcase_captures --parallel 4
FM_SHOWCASE_DIR=/tmp/player12-images FM_SHOWCASE_VIDEO_DIR=/tmp/player12-motion \
  FM_SHOWCASE_THEME=0 FM_SHOWCASE_LANGUAGE=English \
  ctest --test-dir build --output-on-failure -R '^showcase::'
```

Export screenshots to 1600×900 WebP. Encode the 3D clip at 1280×720 / 30 fps and
keep the 2D interface clip at 1600×900 / 30 fps for text legibility. MP4 is the
H.264 fallback; WebM uses VP9. MP4 files have their playback metadata moved to
the beginning with `-movflags +faststart`.

Videos use `preload="none"`. Mobile, reduced-motion and data-saving preferences
get a poster until the visitor chooses playback. The raw 3200×1800 screenshots
and 900 source frames are temporary build artifacts, not publication assets.
