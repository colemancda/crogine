/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
http://trederia.blogspot.com

crogine application - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/core/String.hpp>

#include <cstdint>
#include <array>
#include <vector>
#include <string>

//don't modify this! It's read/written directly from saves and will break existing data.
struct LeaguePlayer final
{
    std::int32_t skill = 1; //indexes into the skill array (lower is smaller error offset == better)
    std::int32_t curve = 0; //selects skill curve of final result
    std::int32_t outlier = 0; //chance in 50 of making a mess.
    std::int32_t nameIndex = 0;

    float quality = 1.f; //quality of player result is multiplied by this to limit them from being Perfect

    std::int16_t currentScore = 0; //sum of holes converted to stableford
    std::int16_t previousPosition = 0; //from previous iteration
};

struct PreviousEntry final
{
    std::int32_t score = 0;
    std::int32_t handicap = 0;
    std::int32_t nameIndex = 0;
};
using SortData = PreviousEntry;
static const std::string PrevFileName("last.gue");

struct TableEntry final
{
    std::int32_t score = 0;
    std::int32_t handicap = 0;
    std::int32_t name = -1;
    std::int32_t positionChange = 1; //index into animation 0 down, 1 NC, 2 up
    TableEntry(std::int32_t s, std::int32_t h, std::int32_t n, std::int32_t p)
        :score(s), handicap(h), name(n), positionChange(p) {}
};

struct LeagueRoundID final
{
    enum
    {
        Club,
        RoundOne,
        RoundTwo,
        RoundThree,
        RoundFour,
        RoundFive,
        RoundSix,

        Count
    };
};

class League final
{
public:
    static constexpr std::size_t PlayerCount = 15u;
    static constexpr std::int32_t MaxIterations = 24;

    explicit League(std::int32_t leagueID);
    //hmmm given that there should only be once instance of any table at a time
    //should we not at least make this move-only? Or even a dreaded singleton???
    void reset();
    void iterate(const std::array<std::int32_t, 18>&, const std::vector<std::uint8_t>& playerScores, std::size_t holeCount);

    std::int32_t getCurrentIteration() const { return m_currentIteration; }
    std::int32_t getCurrentSeason() const { return m_currentSeason; }
    std::int32_t getCurrentScore() const { return m_playerScore; }
    std::int32_t getCurrentPosition() const { return m_currentPosition + 1; /*convert from index to position*/ }
    std::int32_t getCurrentBest() const { return m_currentBest + 1; /*convert from index to position*/ }

    const std::array<LeaguePlayer, PlayerCount>& getTable() const { return m_players; }
    const std::vector<TableEntry>& getSortedTable() const { return m_sortedTable; } //used for display

    const cro::String& getPreviousResults(const cro::String& playerName) const;
    std::int32_t getPreviousPosition() const { return m_previousPosition; }

    std::int32_t getMaxIterations() const { return m_maxIterations; }
    std::int32_t reward(std::int32_t position) const;

private:
    const std::int32_t m_id;
    const std::int32_t m_maxIterations;
    
    std::array<LeaguePlayer, PlayerCount> m_players = {};
    std::int32_t m_playerScore;
    std::int32_t m_currentIteration;
    std::int32_t m_currentSeason;
    std::int32_t m_increaseCount;
    std::int32_t m_currentPosition;
    std::int32_t m_lastIterationPosition;

    std::int32_t m_currentBest;

    mutable cro::String m_previousResults;
    mutable std::int32_t m_previousPosition;

    void increaseDifficulty();
    void decreaseDifficulty();
    std::string getFilePath(const std::string& fileName) const;

    std::vector<TableEntry> m_sortedTable = {};
    void createSortedTable();

    void read();
    void write();
};