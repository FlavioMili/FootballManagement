/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Player12", "index.html", [
    [ "Save migrations and save protocol", "index.html", "index" ],
    [ "Contributing to Player12", "md_CONTRIBUTING.html", [
      [ "Choose a starting point", "md_CONTRIBUTING.html#autotoc_md12", null ],
      [ "Your first pull request", "md_CONTRIBUTING.html#autotoc_md13", null ],
      [ "Set up for code changes", "md_CONTRIBUTING.html#autotoc_md14", null ],
      [ "Check your change", "md_CONTRIBUTING.html#autotoc_md15", null ],
      [ "Code and documentation conventions", "md_CONTRIBUTING.html#autotoc_md16", null ],
      [ "Names, credit and licence", "md_CONTRIBUTING.html#autotoc_md17", null ]
    ] ],
    [ "Contributors", "md_CONTRIBUTORS.html", [
      [ "Add your name", "md_CONTRIBUTORS.html#autotoc_md19", null ]
    ] ],
    [ "AI Guidelines", "md_AI__GUIDELINES.html", [
      [ "1. The Core Philosophy", "md_AI__GUIDELINES.html#autotoc_md21", null ],
      [ "2. Mandatory Declaration", "md_AI__GUIDELINES.html#autotoc_md22", null ],
      [ "3. Expectations for Reviewers", "md_AI__GUIDELINES.html#autotoc_md23", null ],
      [ "4. Expectations for AI Agents", "md_AI__GUIDELINES.html#autotoc_md24", null ]
    ] ],
    [ "Architecture", "md_docs_2ARCHITECTURE.html", [
      [ "Build targets", "md_docs_2ARCHITECTURE.html#autotoc_md26", null ],
      [ "Core model", "md_docs_2ARCHITECTURE.html#autotoc_md27", null ],
      [ "Match engine and scheduling", "md_docs_2ARCHITECTURE.html#autotoc_md28", null ],
      [ "Persistence", "md_docs_2ARCHITECTURE.html#autotoc_md29", null ],
      [ "Controller", "md_docs_2ARCHITECTURE.html#autotoc_md30", null ],
      [ "GUI", "md_docs_2ARCHITECTURE.html#autotoc_md31", null ],
      [ "Balance lab", "md_docs_2ARCHITECTURE.html#autotoc_md32", null ],
      [ "Tests", "md_docs_2ARCHITECTURE.html#autotoc_md33", null ]
    ] ],
    [ "Reproducible builds", "md_docs_2development_2builds.html", [
      [ "Presets", "md_docs_2development_2builds.html#autotoc_md35", null ],
      [ "clang-tidy", "md_docs_2development_2builds.html#autotoc_md36", null ],
      [ "Dependency updates", "md_docs_2development_2builds.html#autotoc_md37", null ]
    ] ],
    [ "Design notes recovered from Spec Kitty", "md_docs_2development_2design-notes.html", [
      [ "Source provenance", "md_docs_2development_2design-notes.html#autotoc_md39", null ],
      [ "Rationale that still applies", "md_docs_2development_2design-notes.html#autotoc_md40", null ],
      [ "Planned contracts versus current implementation", "md_docs_2development_2design-notes.html#autotoc_md41", null ],
      [ "This documentation and extension pass", "md_docs_2development_2design-notes.html#autotoc_md42", null ]
    ] ],
    [ "Extending and modding", "md_docs_2development_2extending-and-modding.html", [
      [ "Edit data without rebuilding", "md_docs_2development_2extending-and-modding.html#autotoc_md44", null ],
      [ "Per-match settings for C++ tools", "md_docs_2development_2extending-and-modding.html#autotoc_md45", null ],
      [ "Choose the smallest behavior extension", "md_docs_2development_2extending-and-modding.html#autotoc_md46", null ],
      [ "Validate a behavior change", "md_docs_2development_2extending-and-modding.html#autotoc_md47", null ]
    ] ],
    [ "Match engine: implementation and rationale", "md_docs_2development_2match-engine.html", [
      [ "Ownership and data flow", "md_docs_2development_2match-engine.html#autotoc_md49", null ],
      [ "Coordinates and clocks", "md_docs_2development_2match-engine.html#autotoc_md50", null ],
      [ "One authoritative simulation step", "md_docs_2development_2match-engine.html#autotoc_md51", null ],
      [ "Choosing an advance API", "md_docs_2development_2match-engine.html#autotoc_md52", null ],
      [ "Ball flight, contacts and rules", "md_docs_2development_2match-engine.html#autotoc_md53", null ],
      [ "Determinism, replay and observation", "md_docs_2development_2match-engine.html#autotoc_md54", null ],
      [ "Implementation map and checks", "md_docs_2development_2match-engine.html#autotoc_md55", null ]
    ] ],
    [ "Player behavior: from attributes to actions", "md_docs_2development_2player-behavior.html", [
      [ "Persistent player versus match slot", "md_docs_2development_2player-behavior.html#autotoc_md57", null ],
      [ "Behavior between matches", "md_docs_2development_2player-behavior.html#autotoc_md58", null ],
      [ "Team phase and coordinated assignments", "md_docs_2development_2player-behavior.html#autotoc_md59", null ],
      [ "Shape, roles and duties", "md_docs_2development_2player-behavior.html#autotoc_md60", null ],
      [ "Off-ball intentions and physical movement", "md_docs_2development_2player-behavior.html#autotoc_md61", null ],
      [ "On-ball choice and execution", "md_docs_2development_2player-behavior.html#autotoc_md62", null ],
      [ "Defending, keepers and external control", "md_docs_2development_2player-behavior.html#autotoc_md63", null ],
      [ "Reproducing a suspicious decision", "md_docs_2development_2player-behavior.html#autotoc_md64", null ]
    ] ],
    [ "Building and releasing desktop packages", "md_docs_2development_2release.html", [
      [ "Cutting a release", "md_docs_2development_2release.html#autotoc_md69", null ],
      [ "What each job does", "md_docs_2development_2release.html#autotoc_md70", null ],
      [ "Package layout and runtime paths", "md_docs_2development_2release.html#autotoc_md71", [
        [ "Crash reports and save recovery", "md_docs_2development_2release.html#autotoc_md72", null ]
      ] ],
      [ "Building a package locally (Linux)", "md_docs_2development_2release.html#autotoc_md73", null ],
      [ "Known limitations", "md_docs_2development_2release.html#autotoc_md74", null ]
    ] ],
    [ "Tests and tools", "md_docs_2development_2testing.html", [
      [ "Run one area first", "md_docs_2development_2testing.html#autotoc_md76", null ],
      [ "Balance lab", "md_docs_2development_2testing.html#autotoc_md77", null ],
      [ "Screenshots", "md_docs_2development_2testing.html#autotoc_md78", null ],
      [ "Performance benchmarks", "md_docs_2development_2testing.html#autotoc_md79", null ],
      [ "Known baseline failures", "md_docs_2development_2testing.html#autotoc_md80", null ]
    ] ],
    [ "Website documentation rendering", "md_docs_2development_2website.html", [
      [ "Lists and code examples", "md_docs_2development_2website.html#autotoc_md82", null ],
      [ "Diagram sizing and themes", "md_docs_2development_2website.html#autotoc_md83", null ],
      [ "Rendering repair walkthrough", "md_docs_2development_2website.html#autotoc_md84", null ],
      [ "Media and publication size", "md_docs_2development_2website.html#autotoc_md85", null ]
    ] ],
    [ "Controls", "md_docs_2user_2controls.html", [
      [ "Anywhere in the game", "md_docs_2user_2controls.html#autotoc_md87", null ],
      [ "Main menu and club choice", "md_docs_2user_2controls.html#autotoc_md88", null ],
      [ "Management screens", "md_docs_2user_2controls.html#autotoc_md89", [
        [ "Back and Forward", "md_docs_2user_2controls.html#autotoc_md90", null ],
        [ "Sidebar hubs", "md_docs_2user_2controls.html#autotoc_md91", null ],
        [ "Mouse on management screens", "md_docs_2user_2controls.html#autotoc_md92", null ]
      ] ],
      [ "Match day", "md_docs_2user_2controls.html#autotoc_md93", [
        [ "Playback", "md_docs_2user_2controls.html#autotoc_md94", null ],
        [ "Substitutions board and tactics", "md_docs_2user_2controls.html#autotoc_md95", null ],
        [ "Touchline shouts", "md_docs_2user_2controls.html#autotoc_md96", null ],
        [ "3D cameras", "md_docs_2user_2controls.html#autotoc_md97", null ],
        [ "Mouse in the match view", "md_docs_2user_2controls.html#autotoc_md98", null ],
        [ "Play mode", "md_docs_2user_2controls.html#autotoc_md99", null ]
      ] ],
      [ "Where your files are", "md_docs_2user_2controls.html#autotoc_md100", null ]
    ] ],
    [ "Saves, settings and troubleshooting", "md_docs_2user_2files-and-troubleshooting.html", [
      [ "Recover a save", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md102", null ],
      [ "Report a problem", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md103", null ],
      [ "Use an isolated test career", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md104", null ],
      [ "Player12 and earlier careers", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md105", null ]
    ] ],
    [ "Getting started", "md_docs_2user_2getting-started.html", [
      [ "1. Start a career", "md_docs_2user_2getting-started.html#autotoc_md107", null ],
      [ "2. Your first day", "md_docs_2user_2getting-started.html#autotoc_md108", null ],
      [ "3. Get to know the squad", "md_docs_2user_2getting-started.html#autotoc_md109", null ],
      [ "4. Lineup and tactics", "md_docs_2user_2getting-started.html#autotoc_md110", null ],
      [ "5. Training and staff", "md_docs_2user_2getting-started.html#autotoc_md111", null ],
      [ "6. Scouting and transfers", "md_docs_2user_2getting-started.html#autotoc_md112", null ],
      [ "7. The youth academy", "md_docs_2user_2getting-started.html#autotoc_md113", null ],
      [ "8. Advancing the calendar", "md_docs_2user_2getting-started.html#autotoc_md114", null ],
      [ "9. Match day", "md_docs_2user_2getting-started.html#autotoc_md115", [
        [ "3D camera controls", "md_docs_2user_2getting-started.html#autotoc_md116", null ]
      ] ],
      [ "10. Beyond the first season", "md_docs_2user_2getting-started.html#autotoc_md117", null ],
      [ "Tips", "md_docs_2user_2getting-started.html#autotoc_md118", null ]
    ] ],
    [ "Install and run", "md_docs_2user_2installation.html", [
      [ "Download and play", "md_docs_2user_2installation.html#autotoc_md120", null ],
      [ "Build from source", "md_docs_2user_2installation.html#autotoc_md121", [
        [ "Linux", "md_docs_2user_2installation.html#autotoc_md122", null ],
        [ "macOS", "md_docs_2user_2installation.html#autotoc_md123", null ],
        [ "Windows", "md_docs_2user_2installation.html#autotoc_md124", null ],
        [ "Other build options", "md_docs_2user_2installation.html#autotoc_md125", null ]
      ] ],
      [ "Running", "md_docs_2user_2installation.html#autotoc_md126", null ]
    ] ],
    [ "Feature tour", "md_docs_2project_2features.html", [
      [ "Screenshots", "md_docs_2project_2features.html#autotoc_md128", null ]
    ] ],
    [ "Roadmap", "md_docs_2project_2roadmap.html", [
      [ "First priority: improve the interface and player experience", "md_docs_2project_2roadmap.html#autotoc_md130", null ],
      [ "Build a community around the game", "md_docs_2project_2roadmap.html#autotoc_md131", null ],
      [ "In-game editing: build your own football world", "md_docs_2project_2roadmap.html#autotoc_md132", [
        [ "Start with safe, approachable editing", "md_docs_2project_2roadmap.html#autotoc_md133", null ],
        [ "Add countries and football structures", "md_docs_2project_2roadmap.html#autotoc_md134", null ],
        [ "Organise friendlies and summer tournaments", "md_docs_2project_2roadmap.html#autotoc_md135", null ]
      ] ],
      [ "Continue improving the simulation and matchday", "md_docs_2project_2roadmap.html#autotoc_md136", null ],
      [ "Help shape the next step", "md_docs_2project_2roadmap.html#autotoc_md137", null ]
    ] ],
    [ "Add yourself as a player", "md_docs_2contributing_2player-cameo.html", [
      [ "1. Choose your club and an unused player ID", "md_docs_2contributing_2player-cameo.html#autotoc_md139", null ],
      [ "2. Create a player file", "md_docs_2contributing_2player-cameo.html#autotoc_md140", null ],
      [ "3. Check it in a new career", "md_docs_2contributing_2player-cameo.html#autotoc_md141", null ],
      [ "4. Include it in your contribution", "md_docs_2contributing_2player-cameo.html#autotoc_md142", null ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", "namespacemembers_vars" ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"SQLLoader_8cpp.html",
"buyer__negotiation_8h.html#ae14b96fbe35e926af0b1222a9aeab048a13727661cffa8b83aef4332c92dfad1e",
"classAudio_1_1OnePole.html#adcc5d0a621c72c05d017d59b9bd13f09",
"classCallUpScene.html#a63725f76a3dd281db9221726d47dca4c",
"classDatabaseConnection.html#ad7f9a09e8d63929b39c7c2535a57d2df",
"classGUIView.html#aef8b9291f46a97b275a2d9f05ebd6831",
"classGameController.html#a5ee45c61052915bcdb46cb64fb365330",
"classGameController.html#af624de44f0823210b71741347a20bfd3",
"classInput_1_1ActionRegistry.html#a465389df1025c37687f2f6de412e2232",
"classLineupScene.html#aad99b7489bf6867554a7218a47d76ba9",
"classManagerCareer.html#abb3109d01a99a3dee97356e71920a7af",
"classMatchChanges_1_1SubstitutionPlan.html#a1c2aaebbd3ff64c17fb4c8b44584a063",
"classMatchEngine.html#a875b07c56cd44b0d4b00d0bb124b0971",
"classMatchPlayController.html#a13c349ae9f9c9b4a530381ab2d87ac0a",
"classMatchScene.html#af91b4e9d9e7bd53679073d1dbf472e01",
"classNationalTeams.html#a308a6210011b8091030930aca8bbdb42",
"classPlayer.html#ac330e12ffd6ae63e89dc8ca5e7167e05",
"classRenderMath_1_1ShirtNumbers.html#af97d3ea8fd62b538e0dd9d85687a24e3",
"classScoutingScene.html#ad7029b287b74c65cb71984177e3206ec",
"classSquadSurnames.html#a203bc4cc4b845dde090a80e1fbb097a2",
"classTeam.html#a9bdecbf757a79bcd566d324f5fec61e9",
"classTransferMarket.html#a917bde3a6e81dd1a488ab466a390402a",
"classWorldSimulation.html#a3ac4ad92559421fee26e6712b147122b",
"club__economy_8cpp.html#aa59178afdb37256dd30c013f64764e93",
"draw__ceremony__dialog_8h_source.html",
"globals_y.html",
"input__actions_8h.html#a4703eab25445d3dbfe49e1165d915f96",
"lab__runner_8h.html",
"manager__career_8h.html#a4c72aaa98b6dec8cb28a2faf31fe6c0ea222a267cc5778206b253be35ee3ddab5",
"match__engine_8h.html#aca6448151e924d2cb062632532d9081fa007a3c4175ca4f86d9dc49e4f2e9cf8a",
"match__rules_8cpp.html",
"md_docs_2user_2getting-started.html#autotoc_md107",
"namespaceCareerTimeline.html#aaacc94008ff614afeb6c688409be1d49",
"namespaceInteractions.html#a2dcf056e5ef48beb6cac20e1d722fe2e",
"namespaceMatchRules.html#a0aa9e08d57d3e9b3ce2dcabb2bbaa845",
"namespacePreseason.html#a1a646e43dd06c53cf40d2fede7f99fd1",
"namespaceStandings.html#ac2bda5e5da85f073617beff752d3f1f5af79cda57d76127809cf73cb01c470167",
"namespaceTransferNegotiation.html#ac16348065bc0972cf21588fc7f36cd50a8342181f4a611ab840450b347523b9f3",
"namespacemembers_o.html",
"onboarding_8h.html#a90ff1381c79d6b3eb3a853e22f612426a7381655f6e41ebb51d11cfe92f4ded71",
"save__manager_8h.html#a527988df62e56a927ae098f0a301efa2a4307e7e7986aa21a4b7c3ef2b5e948f6",
"sqlite__rows_8h.html#ac13cf5065908ede8c0a47c290f499ebb",
"strategy__scene_8h.html",
"structBuyerNegotiation_1_1BuyerContext.html#a166071031c35407ef1d2a13de5d1d1e0",
"structCompetitions_1_1LeagueMovement.html",
"structDropdownStyle.html#a89d73d219f6dc5778d9d79131f50183b",
"structGameController_1_1VacancyView.html#ac8a2d85acba698e59a774a50259c2989",
"structInput_1_1ActionDef.html#aef4ae2b3bb0a5d98969afdf31d220a16",
"structKeyMoment.html#af21eb649096f067b066981e02c36aa78",
"structLab_1_1SeasonVitals.html#aa4cd0d3bda84e684dbde2aa042558218",
"structManagementScene_1_1PaletteEntry.html#a3e63718e6a391448c52afa5c42976a82a33080c3d870c862a9fee5a861e43be86",
"structMatchCameraInput.html#ae6a1d6428991bd8db9ce34bf83edaa25",
"structMatchPlayer.html#ad562fc8f421a223c3dcb7357c1cd092a",
"structMatchRender3DTuning_1_1Crowd.html#aeebb8f0fb011d144f7f0d5daa1b7a029",
"structMatchRender3DTuning_1_1Player.html#a32f586916a44405d2ed40d2350efb6b9",
"structMatchRenderPlayer.html#acccf117c550b4463fcb783ef788c94b1",
"structMatchReportScene_1_1PlayerRow.html#ab54d776b804ec09cc2b7f3100e92406f",
"structMatchStats.html#a24b8d56e5ad9bb795d47c94f9a57c31f",
"structMatchTuning_1_1Defending.html#a54634ef5838c78501485f21e23d0d2b5",
"structMatchTuning_1_1Pitch.html#a2d9a9142b7b6859232b51d61a344abe1",
"structMatchTuning_1_1Shape.html#acb4d2d0ad6278c1224a759dee36a8b6e",
"structMedicalScene_1_1StaffLine.html#a32881e443d273eee0594dc9a93eb99c0",
"structNextActionFacts.html#a3f5b7522478108942e6386162e9a348b",
"structPlayerMatchLine.html",
"structPlayerTraits.html#a585dfb1b6a6ee6b9ad04e642dc56b7b1",
"structReserveQuota.html#a74edf13e96337b348eb1d944a44eb586",
"structScoutingScene_1_1ReportLine.html#a8fb248a334176e95df6a78363ca2005c",
"structSquadNumbers_1_1Entry.html#af4b4653a62ac14c845753751fdae5998",
"structStaffScene_1_1PendingAction.html",
"structTeamSelectionScene_1_1ClubSummary.html",
"structTransferMarketSceneTuning_1_1Layout.html#a169d3cc762ce05147c993dab281be91a",
"structTransferNegotiation_1_1LoanTerms.html#a41ef1c4b59fcaedf69da201bbd513520",
"structTransferTuning_1_1Market.html#a0c61cb81dae553fcf3cd8c20b7619343",
"structWorldTuning_1_1Finance.html#a423da229bbe898dfd430530461537ba9",
"tactics_8h.html#a0e8e0709c928a7dcca4b389c1213e83c",
"training_8h.html#a4f7541ba864f118a40c9dabed727f5d0a7fd5d63187cb76b997d78b1d2f16fbbe",
"transfer__terms__editor_8h.html#ab1d6bd85c8d4c409eac7de70537dd4d1",
"world__simulation_8h.html#aac1ce0b351573c60ceaa4f8f2774c713ab6cb75cc54a5888e685f4ee814b0876f"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';