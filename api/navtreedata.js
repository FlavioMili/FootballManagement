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
      [ "Ball flight, contacts and rules", "md_docs_2development_2match-engine.html#autotoc_md53", [
        [ "Calibrating finishing without changing keeper reach", "md_docs_2development_2match-engine.html#autotoc_md54", null ]
      ] ],
      [ "Determinism, replay and observation", "md_docs_2development_2match-engine.html#autotoc_md55", null ],
      [ "Implementation map and checks", "md_docs_2development_2match-engine.html#autotoc_md56", null ]
    ] ],
    [ "Player behavior: from attributes to actions", "md_docs_2development_2player-behavior.html", [
      [ "Persistent player versus match slot", "md_docs_2development_2player-behavior.html#autotoc_md58", null ],
      [ "Behavior between matches", "md_docs_2development_2player-behavior.html#autotoc_md59", null ],
      [ "Team phase and coordinated assignments", "md_docs_2development_2player-behavior.html#autotoc_md60", null ],
      [ "Shape, roles and duties", "md_docs_2development_2player-behavior.html#autotoc_md61", null ],
      [ "Off-ball intentions and physical movement", "md_docs_2development_2player-behavior.html#autotoc_md62", null ],
      [ "On-ball choice and execution", "md_docs_2development_2player-behavior.html#autotoc_md63", null ],
      [ "Defending, keepers and external control", "md_docs_2development_2player-behavior.html#autotoc_md64", null ],
      [ "Reproducing a suspicious decision", "md_docs_2development_2player-behavior.html#autotoc_md65", null ]
    ] ],
    [ "Building and releasing desktop packages", "md_docs_2development_2release.html", [
      [ "Cutting a release", "md_docs_2development_2release.html#autotoc_md70", null ],
      [ "What each job does", "md_docs_2development_2release.html#autotoc_md71", null ],
      [ "Package layout and runtime paths", "md_docs_2development_2release.html#autotoc_md72", [
        [ "Crash reports and save recovery", "md_docs_2development_2release.html#autotoc_md73", null ]
      ] ],
      [ "Building a package locally (Linux)", "md_docs_2development_2release.html#autotoc_md74", null ],
      [ "Known limitations", "md_docs_2development_2release.html#autotoc_md75", null ]
    ] ],
    [ "Tests and tools", "md_docs_2development_2testing.html", [
      [ "Run one area first", "md_docs_2development_2testing.html#autotoc_md77", null ],
      [ "Balance lab", "md_docs_2development_2testing.html#autotoc_md78", null ],
      [ "Screenshots", "md_docs_2development_2testing.html#autotoc_md79", null ],
      [ "Performance benchmarks", "md_docs_2development_2testing.html#autotoc_md80", null ],
      [ "Historical calibration failures", "md_docs_2development_2testing.html#autotoc_md81", null ]
    ] ],
    [ "Website documentation rendering", "md_docs_2development_2website.html", [
      [ "Lists and code examples", "md_docs_2development_2website.html#autotoc_md83", null ],
      [ "Diagram sizing and themes", "md_docs_2development_2website.html#autotoc_md84", null ],
      [ "Rendering repair walkthrough", "md_docs_2development_2website.html#autotoc_md85", null ],
      [ "Media and publication size", "md_docs_2development_2website.html#autotoc_md86", null ]
    ] ],
    [ "Controls", "md_docs_2user_2controls.html", [
      [ "Anywhere in the game", "md_docs_2user_2controls.html#autotoc_md88", null ],
      [ "Main menu and club choice", "md_docs_2user_2controls.html#autotoc_md89", null ],
      [ "Management screens", "md_docs_2user_2controls.html#autotoc_md90", [
        [ "Back and Forward", "md_docs_2user_2controls.html#autotoc_md91", null ],
        [ "Sidebar hubs", "md_docs_2user_2controls.html#autotoc_md92", null ],
        [ "Mouse on management screens", "md_docs_2user_2controls.html#autotoc_md93", null ]
      ] ],
      [ "Match day", "md_docs_2user_2controls.html#autotoc_md94", [
        [ "Playback", "md_docs_2user_2controls.html#autotoc_md95", null ],
        [ "Substitutions board and tactics", "md_docs_2user_2controls.html#autotoc_md96", null ],
        [ "Touchline shouts", "md_docs_2user_2controls.html#autotoc_md97", null ],
        [ "3D cameras", "md_docs_2user_2controls.html#autotoc_md98", null ],
        [ "Mouse in the match view", "md_docs_2user_2controls.html#autotoc_md99", null ],
        [ "Play mode", "md_docs_2user_2controls.html#autotoc_md100", null ]
      ] ],
      [ "Where your files are", "md_docs_2user_2controls.html#autotoc_md101", null ]
    ] ],
    [ "Saves, settings and troubleshooting", "md_docs_2user_2files-and-troubleshooting.html", [
      [ "Recover a save", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md103", null ],
      [ "Report a problem", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md104", null ],
      [ "Use an isolated test career", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md105", null ],
      [ "Player12 and earlier careers", "md_docs_2user_2files-and-troubleshooting.html#autotoc_md106", null ]
    ] ],
    [ "Getting started", "md_docs_2user_2getting-started.html", [
      [ "1. Start a career", "md_docs_2user_2getting-started.html#autotoc_md108", null ],
      [ "2. Your first day", "md_docs_2user_2getting-started.html#autotoc_md109", null ],
      [ "3. Get to know the squad", "md_docs_2user_2getting-started.html#autotoc_md110", null ],
      [ "4. Lineup and tactics", "md_docs_2user_2getting-started.html#autotoc_md111", null ],
      [ "5. Training and staff", "md_docs_2user_2getting-started.html#autotoc_md112", null ],
      [ "6. Scouting and transfers", "md_docs_2user_2getting-started.html#autotoc_md113", null ],
      [ "7. The youth academy", "md_docs_2user_2getting-started.html#autotoc_md114", null ],
      [ "8. Advancing the calendar", "md_docs_2user_2getting-started.html#autotoc_md115", null ],
      [ "9. Match day", "md_docs_2user_2getting-started.html#autotoc_md116", [
        [ "3D camera controls", "md_docs_2user_2getting-started.html#autotoc_md117", null ]
      ] ],
      [ "10. Beyond the first season", "md_docs_2user_2getting-started.html#autotoc_md118", null ],
      [ "Tips", "md_docs_2user_2getting-started.html#autotoc_md119", null ]
    ] ],
    [ "Install and run", "md_docs_2user_2installation.html", [
      [ "Download and play", "md_docs_2user_2installation.html#autotoc_md121", null ],
      [ "Build from source", "md_docs_2user_2installation.html#autotoc_md122", [
        [ "Linux", "md_docs_2user_2installation.html#autotoc_md123", null ],
        [ "macOS", "md_docs_2user_2installation.html#autotoc_md124", null ],
        [ "Windows", "md_docs_2user_2installation.html#autotoc_md125", null ],
        [ "Other build options", "md_docs_2user_2installation.html#autotoc_md126", null ]
      ] ],
      [ "Running", "md_docs_2user_2installation.html#autotoc_md127", null ]
    ] ],
    [ "Feature tour", "md_docs_2project_2features.html", [
      [ "Screenshots", "md_docs_2project_2features.html#autotoc_md129", null ]
    ] ],
    [ "Roadmap", "md_docs_2project_2roadmap.html", [
      [ "Start from what already works", "md_docs_2project_2roadmap.html#autotoc_md131", null ],
      [ "First priority: make existing decisions easier", "md_docs_2project_2roadmap.html#autotoc_md132", null ],
      [ "Release confidence and football quality", "md_docs_2project_2roadmap.html#autotoc_md133", null ],
      [ "In-game editing: safe changes before arbitrary worlds", "md_docs_2project_2roadmap.html#autotoc_md134", [
        [ "1. Edit supported data for a new career", "md_docs_2project_2roadmap.html#autotoc_md135", null ],
        [ "2. Make creations shareable and recoverable", "md_docs_2project_2roadmap.html#autotoc_md136", null ],
        [ "3. Separate countries from today's league-pyramid convention", "md_docs_2project_2roadmap.html#autotoc_md137", null ],
        [ "4. Expose competition rules, then expand supported formats", "md_docs_2project_2roadmap.html#autotoc_md138", null ]
      ] ],
      [ "Friendlies, invitations and summer competitions", "md_docs_2project_2roadmap.html#autotoc_md139", null ],
      [ "Community building: people and ownership", "md_docs_2project_2roadmap.html#autotoc_md140", null ],
      [ "Turn the roadmap into a contribution", "md_docs_2project_2roadmap.html#autotoc_md141", null ]
    ] ],
    [ "Add yourself as a player", "md_docs_2contributing_2player-cameo.html", [
      [ "1. Choose your club and an unused player ID", "md_docs_2contributing_2player-cameo.html#autotoc_md143", null ],
      [ "2. Create a player file", "md_docs_2contributing_2player-cameo.html#autotoc_md144", null ],
      [ "3. Check it in a new career", "md_docs_2contributing_2player-cameo.html#autotoc_md145", null ],
      [ "4. Include it in your contribution", "md_docs_2contributing_2player-cameo.html#autotoc_md146", null ]
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
"md_docs_2user_2files-and-troubleshooting.html#autotoc_md104",
"namespaceCareerTimeline.html",
"namespaceInteractions.html#a0c3dcf610b431b903767bc0a36183388",
"namespaceMatchInsights.html#ac569768d71154fa0409e97673a0d4ec4",
"namespacePortableRandom.html#ab1841bc2e4f9dbc0fa5a221adced7b6c",
"namespaceStandings.html#ac2bda5e5da85f073617beff752d3f1f5a6da7303be1f764b9ee9509a3b515dd63",
"namespaceTransferNegotiation.html#ac16348065bc0972cf21588fc7f36cd50a5f65828b85ec1903e35b7d2163d3eeb6",
"namespacemembers_k.html",
"onboarding_8h.html#a4c48bf29158d92c91f240c194229aab7",
"save__manager_8h.html#a3531beada44788a5ac69957338dcc544a809b7a805a28884b364837536cdc38b7",
"sqlite__rows_8h.html#a238336a79a3b5f801c4858631339c767",
"strategy_8cpp.html",
"structButtonStyle.html#aa42e6de85cfa8b093bc39c8d0a563c84",
"structCompetitions_1_1KnockoutResolution.html#aee7a9ba54b78dc288e25989f7e4c1804",
"structDressingRoom.html#ac173a1c38aa861abbff5bf9af56fc8b4",
"structGameController_1_1VacancyView.html#a9106b1bb66689e70eeead18f24177ec1",
"structInput_1_1ActionDef.html#ab9877a3ec35cd42e30b6c92885425583",
"structKeyMoment.html#a832d059da8bae85ac5f2dd3399dbdac7",
"structLab_1_1SeasonVitals.html#a8d54c2ef42ebc406f9ac3ee9e2852490",
"structManagementScene_1_1PaletteEntry.html",
"structMatchCameraInput.html#a77760fb92688f26176328ab3f599e218",
"structMatchPlayer.html#aac062e80612cfca2603703c145c5a373",
"structMatchRender3DTuning_1_1Crowd.html#acf22cf73cb14dd8e88c9b58569f5c287",
"structMatchRender3DTuning_1_1Player.html#a1079176314992e54649ed88d87bfc636",
"structMatchRenderPlayer.html#a91ebae82aa29e2b3842eed7fc0da23a3",
"structMatchReportScene_1_1EventRow.html#a8ebc9f813ef962e85d9d4677fb0a9bfd",
"structMatchStats.html#a087bec7d3f42c16c6112a5c44342d0ba",
"structMatchTuning_1_1Defending.html#a48be2a283f07176f9ec3bf57f8050051",
"structMatchTuning_1_1Pitch.html#a1077859f72c85cec0dfea28a40327802",
"structMatchTuning_1_1Shape.html#ac62a77105802115272f508b0940a6257",
"structMedicalScene_1_1RiskLine.html#adfbb44b5158636a3aacb164f39a212b1",
"structNextAction.html#af3a82406b192ac71e238eae70edec335",
"structPlayerMatchConsequence.html",
"structPlayerTrainingState.html#a32b149604bf205e37893347d57734a1b",
"structRenderMath_1_1Vec4.html#ac15a0f5372a963bc808a5c0378c903ad",
"structScoutingScene_1_1ReportLine.html",
"structSquadNumbers_1_1Entry.html#a2e4c442c146286d49a07da6da972807f",
"structStaffMember.html#aa07e60652566865ff658e30c49bb4c9d",
"structTeamMatchStats.html#ad42916d41487c09a12e6a8e87ad9f708",
"structTransferMarketSceneTuning_1_1Filters.html#aacafa76ebde4bed7354a64e9578dd22c",
"structTransferNegotiation_1_1LoanTerms.html#a3d120d02013c7b49baa054f9875598aa",
"structTransferTuning_1_1Loan.html#a97293d4023f2680f1f495a8f69566984",
"structWorldTuning_1_1Finance.html",
"tactics_8h.html#a0def41ff5c7c3157ef5c508203fce19c",
"training_8h.html#a4f7541ba864f118a40c9dabed727f5d0a4905ac9d6a22bdfc1ae096094ce6248d",
"transfer__terms__editor_8h.html#a701825cdfff87d5ba8ad6e46e52e347a",
"world__simulation_8h.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';