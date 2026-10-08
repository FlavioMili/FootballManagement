# Roadmap

Player12 already has a playable management career and live matches. This roadmap
sets out future directions, including ideas that need design and playtesting.
**Everything below is planned work, not a claim that it is available today.**
There are no promised release dates; priorities and scope can change with player
feedback and contributor availability. See the [feature tour](features.md) for
current capabilities and the [changelog](../../CHANGELOG.md) for shipped changes.

## First priority: improve the interface and player experience

Make everyday management easier before expanding the number of screens:

- Make navigation consistent across squad, tactics, scouting, transfers and
  matchday. Keep the current club, competition and season clear, with useful back
  navigation and shortcuts between related records.
- Improve layouts for different window sizes and display scales. Make tables,
  labels, chart legends and tooltips readable, with enough spacing and contrast
  in both light and dark themes.
- Make sorting, filtering and searching consistent. Let players find relevant
  attributes and decisions without working through unnecessary clicks.
- Explain consequences before actions: transfer costs, contract commitments,
  competition eligibility and tactical choices. Add clearer empty states,
  validation messages and confirmations for destructive actions.
- Improve onboarding with a short first-career guide, contextual help and
  understandable descriptions of the simulation's statistics.
- Improve keyboard navigation, focus visibility and ways to understand states
  without relying on colour alone.
- Test common journeys with newcomers: start a career, select a team, change
  tactics, make a signing and play a match. Use their feedback to choose the
  next improvements.

## Build a community around the game

This is a separate, nontechnical initiative: making Player12 welcoming and
helping people enjoy the game together, rather than adding a software feature.

- Choose a manageable place for players and contributors to share ideas,
  careers, tactics, screenshots and custom worlds. Start small rather than
  opening several channels that nobody can maintain.
- Publish clear participation guidelines and agree how moderation, credit and
  respectful feedback should work before inviting a wider audience.
- Share development updates and invite people to playtest specific improvements.
  Explain what feedback would be helpful and report what changed because of it.
- Welcome help with translations, guides, game data, accessibility feedback,
  videos and community activities as well as code.
- Explore community challenges, friendly competitions and showcases of player
  creations. Recognise contributors and make the first contribution approachable.

## In-game editing: build your own football world

Current [user-made data](../development/extending-and-modding.md) is a foundation,
not a complete in-game world editor. Expand editing in stages so changes remain
understandable and valid.

### Start with safe, approachable editing

- Browse, search, create and edit clubs, players, staff and venues through the
  game interface, with previews and clear explanations of each field.
- Validate unique IDs, required fields, team assignments and references before
  saving. Support undo or a recoverable draft, and show what an import will change.
- Export and import reusable data packs with authorship, descriptions and version
  information, so creators can share their work without editing database files.
- Explain which changes apply to a new career and which are safe in an existing
  save. Keep an unmodified career recoverable when trying a custom world.

### Add countries and football structures

- Add and edit countries, regions and national associations, including flags,
  nationalities, season calendars and relationships to continental structures.
- Create clubs and national teams within those structures and define player
  nationality and national-team eligibility rules.
- Create club competitions: leagues, cups, continental tournaments, promotion
  and relegation pathways, qualifying rounds and multi-stage formats.
- Create national-team competitions: qualifiers, regional tournaments and
  international finals, including group stages and knockout brackets.
- Configure participants, qualification routes, calendar windows, squad rules,
  scheduling constraints, tie-breakers, extra time, penalties and prize money.
  Preview a generated schedule and flag conflicts before a career begins.

### Organise friendlies and summer tournaments

- Invite another club to a friendly, choose a date and venue, and negotiate
  practical details such as travel, gate receipts and availability. Let clubs
  accept, decline or propose a different date.
- Organise a friendly for a stated cause, such as a charity fundraiser, memorial
  or community event. Show the purpose and planned allocation of in-game proceeds.
- Create summer and preseason friendly competitions with invited clubs,
  round-robin or knockout formats, host venues and scheduling around competitive
  fixtures, travel and recovery.
- Manage invitations, replies, participant changes and cancellations, with a
  clear overview of confirmed fixtures and outstanding invitations.

These are design directions. They do not imply online multiplayer, real-world
fundraising or automatic compatibility between arbitrary custom competition rules.

## Continue improving the simulation and matchday

The following directions also build on known limitations of version 1.0:

- Play mode: a practice match and more direct control over set pieces.
- More realism: shot conversion, the possession gap between strong and weak
  sides, and offsides closer to real football.
- Southern-hemisphere and American season calendars for Brazil, Argentina and
  the United States.
- Watching and directing your national team's matches live, players with more
  than one nationality, and appearance clauses in loans for other clubs' players.
- Whole worlds that stay bit-identical across platforms.
- Match sound with club chants and a commentary voice; Italian commentary with
  the clubs' articles.
- Signed and notarised macOS packages, and validated downloadable Windows builds.

## Help shape the next step

Start with the [contribution guide](../../CONTRIBUTING.md) and
[existing issues](https://github.com/FlavioMili/FootballManagement/issues).
For an idea, describe the player problem, a concrete example and a small first
improvement. Large editor features should begin with a focused design discussion;
community initiatives should start with people, ownership and achievable activities.
