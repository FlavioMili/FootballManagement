// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

// Language files and the grammar helpers of the message formatter: club
// articles, plurals, dates, position and competition names.

#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "global/language_manager.h"
#include "global/paths.h"
#include "model/club_article.h"
#include "model/competition.h"
#include "model/inbox.h"
#include "model/league.h"
#include "model/role_utils.h"

namespace
{
nlohmann::json loadJson(const std::string& path)
{
  std::ifstream stream(path);
  return nlohmann::json::parse(stream);
}

nlohmann::json languageFile(const char* file)
{
  return loadJson(AssetPaths::root() + "assets/lang/" + file);
}

/** Positional placeholder indices ("{0}", "{1:di}", "{2|a|b}" -> 0, 1, 2). */
std::set<int> positionalIndices(const std::string& text)
{
  static const std::regex PLACEHOLDER(R"(\{(\d+)[}:|])");
  std::set<int> indices;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), PLACEHOLDER);
       it != std::sregex_iterator(); ++it)
    indices.insert(std::stoi((*it)[1].str()));
  return indices;
}

/** printf conversions in order, "%%" excluded. */
std::vector<std::string> printfConversions(const std::string& text)
{
  static const std::regex CONVERSION(
      R"(%[-+#0]*[0-9]*(?:\.[0-9]+)?(?:hh|h|ll|l|z|j|t)?([diouxXeEfgGcsp%]))");
  std::vector<std::string> conversions;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), CONVERSION);
       it != std::sregex_iterator(); ++it)
  {
    if ((*it)[1].str() != "%") conversions.push_back(it->str());
  }
  return conversions;
}

class LanguageFormatTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::EN));
  }

  void TearDown() override
  {
    LanguageManager::instance().loadLanguage(Language::EN);
  }

  static void useItalian()
  {
    ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::IT));
  }
};
}  // namespace

TEST(LanguageFilesTest, BothLanguagesHaveTheSameKeysAndPlaceholders)
{
  const nlohmann::json english = languageFile("English.json");
  const nlohmann::json italian = languageFile("Italian.json");
  ASSERT_EQ(english.size(), italian.size());
  static const std::regex SPEC(R"(\{\d+([:|])([^}]*)\})");
  static const std::set<std::string> PREPOSITIONS = {
      "il", "Il", "di", "Di", "a", "A", "da", "Da", "in", "In", "su", "Su"};
  for (const auto& [key, value] : english.items())
  {
    ASSERT_TRUE(italian.contains(key)) << key;
    const std::string en = value.get<std::string>();
    const std::string it = italian.at(key).get<std::string>();
    EXPECT_EQ(positionalIndices(en), positionalIndices(it)) << key;
    if (positionalIndices(en).empty())
      EXPECT_EQ(printfConversions(en), printfConversions(it)) << key;
    for (const std::string& text : {en, it})
    {
      for (auto match = std::sregex_iterator(text.begin(), text.end(), SPEC);
           match != std::sregex_iterator(); ++match)
      {
        if ((*match)[1].str() == ":")
          EXPECT_TRUE(PREPOSITIONS.contains((*match)[2].str())) << key;
        else
          EXPECT_NE((*match)[2].str().find('|'), std::string::npos) << key;
      }
    }
  }
}

TEST(LanguageFilesTest, EveryPackLeagueAndCupHasADisplayName)
{
  ASSERT_TRUE(LanguageManager::instance().loadLanguage(Language::EN));
  const nlohmann::json english = languageFile("English.json");
  const nlohmann::json italian = languageFile("Italian.json");
  for (const auto& item : loadJson(AssetPaths::leagues()))
  {
    const League league(item.at("id").get<LeagueID>(),
                        item.at("name").get<std::string>());
    const std::string arg = Competitions::leagueNameArg(league);
    ASSERT_EQ(arg.front(), '@') << league.getName();
    EXPECT_TRUE(english.contains(arg.substr(1))) << arg;
    EXPECT_TRUE(italian.contains(arg.substr(1))) << arg;
    // The English name is the data pack's own, so English is unchanged.
    EXPECT_EQ(english.at(arg.substr(1)).get<std::string>(), league.getName());
    if (!item.contains("parent_league"))
    {
      std::string cup = "CUP_NAME_" + arg.substr(13);
      EXPECT_TRUE(english.contains(cup)) << cup;
      EXPECT_TRUE(italian.contains(cup)) << cup;
    }
  }
}

TEST(InboxFormatTest, ItalianArticlesFollowTheClubName)
{
  EXPECT_EQ(ClubArticle::italian("Lecce"), "il");
  EXPECT_EQ(ClubArticle::italian("Roma"), "la");  // "article_it" in the pack
  EXPECT_EQ(ClubArticle::italian("Acaya"), "l'");
  EXPECT_EQ(ClubArticle::italian("Hull Town"), "l'");
  EXPECT_EQ(ClubArticle::italian("Stoke City"), "lo");
  EXPECT_EQ(ClubArticle::italian("Zaragoza"), "lo");
  EXPECT_EQ(ClubArticle::italian("Southampton"), "il");
  EXPECT_EQ(ClubArticle::withPreposition("Roma", "di"), "della Roma");
  EXPECT_EQ(ClubArticle::withPreposition("Acaya", "a"), "all'Acaya");
  EXPECT_EQ(ClubArticle::withPreposition("Lecce", "da"), "dal Lecce");
  EXPECT_EQ(ClubArticle::withPreposition("Stoke City", "in"), "nello Stoke City");
  EXPECT_EQ(ClubArticle::withPreposition("Roma", "Il"), "La Roma");
  EXPECT_EQ(ClubArticle::withPreposition("Acaya", "Il"), "L'Acaya");
  EXPECT_EQ(ClubArticle::withPreposition("Lecce", "per"), "Lecce");
}

TEST_F(LanguageFormatTest, ClubPlaceholdersRenderArticlesOnlyInItalian)
{
  const std::vector<std::string> roma = {"Mario Rossi", "Roma"};
  EXPECT_EQ(formatLocalized("INBOX_MANAGER_APPOINTED_BODY", roma),
            "Mario Rossi is the new manager of Roma.");
  useItalian();
  EXPECT_EQ(formatLocalized("INBOX_MANAGER_APPOINTED_BODY", roma),
            "Mario Rossi è il nuovo allenatore della Roma.");
  EXPECT_EQ(formatLocalized("INBOX_MANAGER_APPOINTED_BODY",
                            {"Mario Rossi", "Lecce"}),
            "Mario Rossi è il nuovo allenatore del Lecce.");
  EXPECT_EQ(formatLocalized("INBOX_JOB_REJECTED_TITLE", {"Acaya"}),
            "L'Acaya ha respinto la tua candidatura");
  // Key arguments are never given an article.
  EXPECT_EQ(formatLocalized("INBOX_SIGNING_BODY",
                            {"Mario Rossi", "@INBOX_FREE_AGENT", "€0"}),
            "Mario Rossi arriva svincolato per €0.");
}

TEST_F(LanguageFormatTest, PluralPlaceholdersPickTheWordForTheCount)
{
  EXPECT_EQ(formatLocalized("YOUTH_DAYS", {"1"}), "1 day");
  EXPECT_EQ(formatLocalized("YOUTH_DAYS", {"0"}), "0 days");
  useItalian();
  EXPECT_EQ(formatLocalized("YOUTH_DAYS", {"1"}), "1 giorno");
  EXPECT_EQ(formatLocalized("YOUTH_DAYS", {"12"}), "12 giorni");
  EXPECT_EQ(formatLocalized("INBOX_CONTRACTS_BODY", {"1", "Mario Rossi"}),
            "1 giocatore ha il contratto in scadenza a fine stagione: Mario "
            "Rossi.");
}

TEST_F(LanguageFormatTest, IsoDateArgumentsUseTheLocalisedMonth)
{
  const std::vector<std::string> args = {"Mario Rossi", "Lecce", "2027-06-30",
                                         "50"};
  EXPECT_NE(formatLocalized("INBOX_LOAN_OUT_BODY", args).find("30 Jun 2027"),
            std::string::npos);
  useItalian();
  const std::string italian = formatLocalized("INBOX_LOAN_OUT_BODY", args);
  EXPECT_NE(italian.find("al Lecce fino al 30 giu 2027"), std::string::npos)
      << italian;
  EXPECT_EQ(localizedDate("2027-06-30"), "30 giu 2027");
  // Anything that is not a whole ISO date stays as it is.
  EXPECT_EQ(localizedDate("not a date"), "not a date");
  EXPECT_EQ(formatLocalized("YOUTH_DAYS", {"2027-06-3"}), "2027-06-3 giorni");
}

TEST_F(LanguageFormatTest, PositionNamesFollowTheLanguage)
{
  EXPECT_STREQ(RoleUtils::shortName(PlayerRole::GK), "GK");
  EXPECT_STREQ(RoleUtils::longName(PlayerRole::CB), "Centre-back");
  EXPECT_EQ(RoleUtils::shortNameArg(PlayerRole::ST), "@ROLE_SHORT_ST");
  EXPECT_EQ(RoleUtils::toString(PlayerRole::GK), "GK");  // persistence code
  useItalian();
  EXPECT_STREQ(RoleUtils::shortName(PlayerRole::GK), "POR");
  EXPECT_STREQ(RoleUtils::shortName(PlayerRole::CAM), "COC");
  EXPECT_STREQ(RoleUtils::longName(PlayerRole::CB), "Difensore centrale");
  EXPECT_EQ(RoleUtils::toString(PlayerRole::GK), "GK");
  // A saved list of position keys is shown in the current language.
  const std::string preview = formatLocalized(
      "INBOX_YOUTH_PREVIEW_BODY",
      {"Mario Rossi", "@YOUTH_QUALITY_PROMISING", "@ROLE_SHORT_GK, @ROLE_SHORT_CB",
       "@YOUTH_PERS_CONTENT", "8"});
  EXPECT_NE(preview.find("in POR, DC"), std::string::npos) << preview;
}

TEST_F(LanguageFormatTest, LeagueNamesAreTranslatedAndCustomNamesKept)
{
  const League italian_league(1, "Italian League");
  const League custom(40, "Apulian Amateur League");
  EXPECT_EQ(Competitions::leagueName(italian_league), "Italian League");
  useItalian();
  EXPECT_EQ(Competitions::leagueName(italian_league), "Campionato italiano");
  EXPECT_EQ(Competitions::leagueName(custom), "Apulian Amateur League");
  EXPECT_EQ(Competitions::leagueNameArg(custom), "Apulian Amateur League");
}
