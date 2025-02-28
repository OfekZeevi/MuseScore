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
#ifndef MU_GLOBAL_ZIPWRITER_H
#define MU_GLOBAL_ZIPWRITER_H

#include <QIODevice>

#include "io/iodevice.h"

namespace mu {
class ZipWriter
{
public:
    enum Status {
        NoError,
        FileWriteError,
        FileOpenError,
        FilePermissionsError,
        FileError
    };

    explicit ZipWriter(QIODevice* device);
    explicit ZipWriter(io::IODevice* device);
    ~ZipWriter();

    void close();
    Status status() const;

    void addFile(const QString& fileName, const QByteArray& data);

private:

    void flush();

    struct Impl;
    Impl* m_impl = nullptr;
    io::IODevice* m_device = nullptr;
};
}

#endif // MU_GLOBAL_ZIPWRITER_H
