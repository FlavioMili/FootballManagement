// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <string_view>

/**
 * @brief Italian definite articles for club names ("il Lecce", "la Roma",
 * "l'Acaya", "lo Stoke City").
 *
 * Italian templates write "{0:di}" instead of "del {0}" (see
 * formatLocalized()), so the article follows the club rather than being
 * hard-coded in the sentence. A club's article comes from the optional
 * "article_it" field of its data-pack entry; without it the masculine form
 * is chosen from the name's first sounds, which is how Italian treats clubs
 * named after a city.
 */
namespace ClubArticle
{

/** "il", "lo", "l'" or "la" for @p club_name. */
std::string_view italian(std::string_view club_name);

/**
 * @brief @p club_name preceded by the Italian @p preposition fused with its
 * article: ("di", "Roma") gives "della Roma", ("a", "Acaya") "all'Acaya",
 * ("il", "Lecce") just "il Lecce". A capitalised preposition ("Il", "Di")
 * capitalises the result for the start of a sentence. Unknown prepositions
 * return the name unchanged.
 */
std::string withPreposition(std::string_view club_name,
                            std::string_view preposition);

}  // namespace ClubArticle
