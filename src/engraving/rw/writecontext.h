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

#ifndef MU_ENGRAVING_WRITECONTEXT_H
#define MU_ENGRAVING_WRITECONTEXT_H

#include <map>
#include "containers.h"
#include "linksindexer.h"
#include "libmscore/select.h"

namespace mu::engraving {
class WriteContext
{
public:
    int assignLocalIndex(const Ms::Location& mainElementLocation);
    void setLidLocalIndex(int lid, int localIndex);
    int lidLocalIndex(int lid) const;

    Fraction curTick() const { return _curTick; }
    void setCurTick(const Fraction& v) { _curTick   = v; }
    void incCurTick(const Fraction& v) { _curTick += v; }

    Fraction tickDiff() const { return _tickDiff; }
    void setTickDiff(const Fraction& v) { _tickDiff  = v; }

    track_idx_t curTrack() const { return _curTrack; }
    void setCurTrack(track_idx_t v) { _curTrack  = v; }
    int trackDiff() const { return _trackDiff; }
    void setTrackDiff(int v) { _trackDiff = v; }

    bool clipboardmode() const { return _clipboardmode; }
    bool excerptmode() const { return _excerptmode; }
    bool isMsczMode() const { return _msczMode; }
    bool writeTrack() const { return _writeTrack; }
    bool writePosition() const { return _writePosition; }

    void setClipboardmode(bool v) { _clipboardmode = v; }
    void setExcerptmode(bool v) { _excerptmode = v; }
    void setIsMsczMode(bool v) { _msczMode = v; }
    void setWriteTrack(bool v) { _writeTrack= v; }
    void setWritePosition(bool v) { _writePosition = v; }

    void setFilter(Ms::SelectionFilter f) { _filter = f; }
    bool canWrite(const Ms::EngravingItem*) const;
    bool canWriteVoice(track_idx_t track) const;

private:
    Ms::LinksIndexer m_linksIndexer;
    std::map<int, int> m_lidLocalIndices;

    Fraction _curTick    { 0, 1 };           // used to optimize output
    Fraction _tickDiff   { 0, 1 };
    track_idx_t _curTrack = mu::nidx;
    int _trackDiff       { 0 };             // saved track is curTrack-trackDiff

    bool _clipboardmode  { false };     // used to modify write() behaviour
    bool _excerptmode    { false };     // true when writing a part
    bool _msczMode       { true };      // false if writing into *.msc file
    bool _writeTrack     { false };
    bool _writePosition  { false };

    Ms::SelectionFilter _filter;
};
}

#endif // MU_ENGRAVING_WRITECONTEXT_H
