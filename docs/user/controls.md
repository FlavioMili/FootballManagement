# Controls

Football Management is played with the mouse. Keyboard shortcuts speed up the
things you do all the time: moving between screens, advancing the calendar and
driving the match view.

## Anywhere in the game

| Key | Action |
|-----|--------|
| F12 | Save a screenshot as `captures/screenshot.bmp` in the user data directory (see [Where your files are](#where-your-files-are)). Set `FM_SCREENSHOT_PATH` to choose another file. |

## Management screens

These work on every screen of a career (Home, Squad, Transfers and so on).
Shortcuts that would clash with typing are ignored while a text field is
active or a dialog is open.

| Key | Action |
|-----|--------|
| Ctrl+K | Open the command palette: type a player, club or screen name. With an empty query it lists your pending tasks ("Next step"). |
| Up / Down (in the palette) | Move the selection |
| Enter (in the palette) | Open the selected result |
| Esc (in the palette) | Close the palette |
| Ctrl+S | Save the career to its slot |
| Space or Enter | Continue: advance the calendar to your next match, or open the match on match day. Ignored while keyboard navigation has a widget focused, so it never doubles as "press this button". |
| Alt+Left | Back to the previous screen you were on |
| Alt+Right | Forward again to the screen Back left |
| Esc | Close the current screen: a profile, report or comparison returns to the screen beneath it, any other screen (Finances included) to Home. Does nothing on Home. |
| Tab / arrow keys | Keyboard navigation between widgets (Dear ImGui) |

### Back and Forward

Career screens keep a history like a web browser. Back returns to the
screen you were on before, whether you got there from the sidebar, a
shortcut, the palette or a link on a page; Forward re-opens what Back left.
Opening a new screen after going back drops the screens ahead, as in a
browser. The history holds the last 50 screens and starts afresh whenever a
career is started or loaded. Screens that can no longer be opened are
skipped: the profile of a player who has left the game, or one of your
club's screens while you are out of work. The live match and the club choice are not part
of the history, so Back never leaves a match or the career.

| Input | Back | Forward |
|-------|------|---------|
| Keyboard | Alt+Left | Alt+Right |
| Mouse side buttons | Button 4 (back) | Button 5 (forward) |
| Touchpad | Two-finger swipe to the right | Two-finger swipe to the left |
| Top bar | *Back* button | Arrow button next to it |

One swipe is one step; a round arrow slides in from the edge of the window
while you swipe and the step happens once it is fully out. The swipe is
ignored while you scroll up or down, over a list that scrolls sideways,
while a text field is being edited and while a dialog is open. The top-bar
buttons are greyed out when there is nothing to go back or forward to.

### Sidebar hubs

The sidebar has seven hubs. The hub you are working in lists its screens
underneath its name, with the current screen highlighted; click one to open
it. Clicking a hub, or pressing its F-key, opens the hub's first screen.
Right after a sidebar click or an F-key, Up and Down move between the
screens of that hub; a click on the page gives the arrow keys back to the
page. Counters (unread messages, new scout reports) appear next to their
screen and, added up, next to the hub.

On narrow windows the sidebar shrinks to icons: hovering a hub's icon shows
its screens in a small menu beside it, and the current hub's screens also
appear as tabs above the page.

| Key | Hub | Screens |
|-----|-----|----------------|
| F1 | Home | Home |
| F2 | Inbox | Inbox |
| F3 | Squad | Squad, Lineup, Tactics, Squad planner, Medical, Compare players |
| F4 | Training | Training, Planning |
| F5 | Matches | Fixtures & Results, Competitions, Calendar, Opposition, International, Data hub |
| F6 | Recruitment | Transfers, Scouting, Youth |
| F7 | Club | Club, Finances, Staff, Delegation, Manager, Awards, Records |

Every screen can also be opened with Ctrl+K. While you are out of work, only
the screens that make sense without a club are available (Home, Inbox,
Competitions, International, Manager, Awards and Records), and the F-key of
a hub with none of them does nothing.

### Mouse on management screens

| Where | Action |
|-------|--------|
| Team selection | Click a club to inspect it, double-click to start the career with it straight away |
| Squad list | Click a player to show his details in the side panel, double-click to open his profile (on narrow windows a single click opens the profile) |
| Scouting and search lists | Click to select a player, double-click to open his profile |
| Calendar | Click a day to select it; double-click a fixture to open its match report (played) or the opponent's club page (upcoming) |
| Lineup pitch | Click a starter to select him; drag an outfield player to move him on the pitch (the goalkeeper stays in goal); double-click for his profile |
| Lineup bench | Click a substitute to select him, double-click for his profile, or drag him onto a starter to swap them. With one starter and one substitute selected, *Swap selected players* confirms the change. |
| Top bar | *Back* returns to the previous screen and the arrow next to it goes forward again; the search field opens the palette; the holiday button next to Continue opens the holiday planner |

## Match day

The match screen has its own keys. They are ignored while you type in a text
field.

### Playback

| Key | Action |
|-----|--------|
| Space | Pause or resume the match |
| V | Switch between the 2D and the 3D view (the same simulation, only the view changes) |
| F | Toggle pitch focus: the match view fills the window under a compact overlay |
| Esc | Leave pitch focus |
| Alt+Enter | Toggle a full-screen window (match screen only; elsewhere use *Settings > Fullscreen*) |

Speed (1x, 2x, 4x, 8x, 16x, 30x), *Highlights*, *Quick result*, *Finish
match*, substitutions and the assistant options are buttons in the match
toolbar. At 1x one second of match time takes one real second. With
*Highlights* on, the game skips instantly to the next key moment and plays
that moment at the chosen speed.

### 3D cameras

| Key | Camera |
|-----|--------|
| 1 | Broadcast |
| 2 | Tactical |
| 3 | End (behind the goal) |
| 4 | Player follow |
| 5 | Free |
| B | Follow the ball while keeping your current angle (switches to the free camera; press again to stop following) |
| R | Reset the view |

### Mouse in the match view

| Input | Action |
|-------|--------|
| Left-drag | Orbit around the camera target. Dragging with any preset hands the view over to the free camera from where it is. |
| Right-drag, middle-drag or Shift+left-drag | Pan across the pitch |
| Mouse wheel | Zoom |
| Double-click (free camera) | Look at the clicked spot |
| Double-click (other cameras, or the 2D view) | Toggle pitch focus |

In the 2D view the mouse only toggles pitch focus; orbit, pan and zoom apply
to the 3D view.

## Where your files are

Saves, settings, logs and screenshots never go next to the game. They live in
the user data directory:

| System | Folder |
|--------|--------|
| Linux | `$XDG_DATA_HOME/FlavioMili/FootballManagement` (usually `~/.local/share/FlavioMili/FootballManagement`) |
| macOS | `~/Library/Application Support/FlavioMili/FootballManagement` |
| Windows | `%APPDATA%\FlavioMili\FootballManagement` |
