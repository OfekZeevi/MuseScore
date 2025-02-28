/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef MU_LOGREMOVER_H
#define MU_LOGREMOVER_H

#include <QStringList>
#include <QDate>

#include <gtest/gtest_prod.h>

#include "io/path.h"

namespace mu {
class LogRemover
{
public:

    static void removeLogs(const io::path_t& logsDir, int olderThanDays, const QString& pattern);

private:

    FRIEND_TEST(LogRemoverTests, ParseDate);

    static void scanDir(const io::path_t& logsDir, QStringList& files);
    static QDate parseDate(const QString& fileName);
    static void removeFiles(const QStringList& files);
};
}

#endif // MU_LOGREMOVER_H
