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

#ifndef MU_ENGRAVING_READCONTEXT_H
#define MU_ENGRAVING_READCONTEXT_H

#include <map>

#include "libmscore/mscore.h"
#include "libmscore/location.h"
#include "libmscore/connector.h"
#include "libmscore/interval.h"
#include "compat/dummyelement.h"
#include "linksindexer.h"

namespace Ms {
class Score;
class TimeSigMap;
class Staff;
class Spanner;
class LinkedObjects;
class Measure;
class Beam;
class Tuplet;

struct SpannerValues {
    int spannerId;
    Fraction tick2;
    track_idx_t track2;
};

struct TextStyleMap {
    QString name;
    TextStyleType ss;
};
}

namespace mu::engraving {
class ReadContext
{
public:

    ReadContext(Ms::Score* score);
    ~ReadContext();

    void setScore(Ms::Score* score);
    Ms::Score* score() const;

    bool pasteMode() const { return _pasteMode; }
    void setPasteMode(bool v) { _pasteMode = v; }

    void setIgnoreVersionError(bool arg);
    bool ignoreVersionError() const;

    QString mscoreVersion() const;
    int mscVersion() const;

    int fileDivision() const;
    int fileDivision(int t) const;

    qreal spatium() const;

    mu::engraving::compat::DummyElement* dummy() const;

    Ms::TimeSigMap* sigmap();
    Ms::Staff* staff(int n);

    void appendStaff(Ms::Staff* staff);
    void addSpanner(Ms::Spanner* s);

    bool undoStackActive() const;

    bool isSameScore(const Ms::EngravingObject* obj) const;

    void initLinks(const ReadContext& ctx);
    void addLink(Ms::Staff* staff, Ms::LinkedObjects* link, const Ms::Location& location);
    Ms::LinkedObjects* getLink(bool isMasterScore, const Ms::Location& location, int localIndexDiff);
    std::map<int, std::vector<std::pair<Ms::LinkedObjects*, Ms::Location> > >& staffLinkedElements();

    bool hasAccidental = false; // used for userAccidental backward compatibility

    Fraction tick()  const { return _tick + _tickOffset; }
    Fraction rtick()  const;
    Fraction tickOffset() const { return _tickOffset; }
    void setTick(const Fraction& f);
    void incTick(const Fraction& f);
    void setTickOffset(const Fraction& val) { _tickOffset = val; }

    track_idx_t track() const { return _track + _trackOffset; }
    void setTrackOffset(int val) { _trackOffset = val; }
    int trackOffset() const { return _trackOffset; }
    void setTrack(track_idx_t val) { _track = val; }

    Ms::Location location(bool forceAbsFrac = false) const;
    void fillLocation(Ms::Location&, bool forceAbsFrac = false) const;
    void setLocation(const Ms::Location&);   // sets a new reading point, taking into account its type (absolute or relative).

    void setCurrentMeasure(Ms::Measure* m) { _curMeasure = m; }
    Ms::Measure* currentMeasure() const { return _curMeasure; }
    void setLastMeasure(Ms::Measure* m) { _lastMeasure = m; }
    Ms::Measure* lastMeasure() const { return _lastMeasure; }
    void setCurrentMeasureIndex(int idx) { _curMeasureIdx = idx; }
    int currentMeasureIndex() const { return _curMeasureIdx; }

    void addBeam(Ms::Beam* s);
    Ms::Beam* findBeam(int id) const { return mu::value(_beams, id, nullptr); }

    void addTuplet(Ms::Tuplet* s);
    Ms::Tuplet* findTuplet(int id) const { return mu::value(_tuplets, id, nullptr); }
    std::unordered_map<int, Ms::Tuplet*>& tuplets() { return _tuplets; }
    void checkTuplets();

    void removeSpanner(const Ms::Spanner*);
    void addSpanner(int id, Ms::Spanner*);
    Ms::Spanner* findSpanner(int id);

    int spannerId(const Ms::Spanner*);        // returns spanner id, allocates new one if none exists

    void addSpannerValues(const Ms::SpannerValues& sv) { _spannerValues.push_back(sv); }
    const Ms::SpannerValues* spannerValues(int id) const;

    void addConnectorInfoLater(std::unique_ptr<Ms::ConnectorInfoReader> c) { _pendingConnectors.push_back(std::move(c)); }   // add connector info to be checked after calling checkConnectors()
    void checkConnectors();
    void reconnectBrokenConnectors();

    Ms::Interval transpose() const { return _transpose; }
    void setTransposeChromatic(int8_t v) { _transpose.chromatic = v; }
    void setTransposeDiatonic(int8_t v) { _transpose.diatonic = v; }

    std::map<int, Ms::LinkedObjects*>& linkIds() { return _elinks; }
    TracksMap& tracks() { return _tracks; }

    TextStyleType addUserTextStyle(const QString& name);
    TextStyleType lookupUserTextStyle(const QString& name) const;
    void clearUserTextStyles() { userTextStyles.clear(); }

    std::list<std::pair<Ms::EngravingItem*, mu::PointF> >& fixOffsets() { return _fixOffsets; }

private:

    void addConnectorInfo(std::unique_ptr<Ms::ConnectorInfoReader>);
    void removeConnector(const Ms::ConnectorInfoReader*);   // Removes the whole ConnectorInfo chain from the connectors list.

    Ms::Score* m_score = nullptr;

    bool _pasteMode = false;  // modifies read behaviour on paste operation

    bool m_ignoreVersionError = false;

    std::map<int /*staffIndex*/, std::vector<std::pair<Ms::LinkedObjects*, Ms::Location> > > m_staffLinkedElements; // one list per staff
    Ms::LinksIndexer m_linksIndexer;

    Fraction _tick             { Fraction(0, 1) };
    Fraction _tickOffset       { Fraction(0, 1) };
    int _intTick = 0;

    track_idx_t _track = 0;
    int _trackOffset = 0;

    Ms::Measure* _curMeasure = nullptr;
    Ms::Measure* _lastMeasure = nullptr;
    int _curMeasureIdx = 0;

    std::unordered_map<int, Ms::Beam*> _beams;
    std::unordered_map<int, Ms::Tuplet*> _tuplets;

    std::list<Ms::SpannerValues> _spannerValues;
    std::list<std::pair<int, Ms::Spanner*> > _spanner;

    std::vector<std::unique_ptr<Ms::ConnectorInfoReader> > _connectors;
    std::vector<std::unique_ptr<Ms::ConnectorInfoReader> > _pendingConnectors;  // connectors that are pending to be updated and added to _connectors. That will happen when checkConnectors() is called.

    Ms::Interval _transpose;

    std::map<int, Ms::LinkedObjects*> _elinks;   // for reading old files (< 3.01)
    TracksMap _tracks;

    std::list<Ms::TextStyleMap> userTextStyles;

    std::list<std::pair<Ms::EngravingItem*, mu::PointF> > _fixOffsets;
};
}

#endif // MU_ENGRAVING_READCONTEXT_H
