/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#ifdef USE_RSS

#include <string>
#include <vector>

class RSSFeed final
{
public:
    /*!
    \brief Contains the name and location (URL)
    of a news item parsed from an RSS feed.
    */
    struct Item final
    {
        std::string title;
        std::string url;
        std::string date;
        std::string description;
    };

    /*!
    \brief Attempts to fet items from the given feed
    \returns false if unsuccessful, else true if the
    items vector has been populated.
    Clears any existing items, which remain cleared should
    fetching a new feed fail.
    */
    bool fetch(const std::string& url);

    /*!
    \brief Returns a vector of items parsed from the RSS feed.
    This will be empty if fetch() returned false
    */
    const std::vector<Item>& getItems() const { return m_items; }

private:
    std::vector<Item> m_items;
    bool parseFeed(const std::string&);
};

#endif