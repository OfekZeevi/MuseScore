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

#include "connector.h"
#include "rw/xml.h"
#include "rw/writecontext.h"
#include "engravingitem.h"
#include "score.h"
#include "engravingobject.h"
#include "factory.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace Ms {
//---------------------------------------------------------
//   ConnectorInfo
//---------------------------------------------------------

ConnectorInfo::ConnectorInfo(const EngravingItem* current, int track, Fraction frac)
    : _current(current), _score(current->score()), _currentLoc(Location::absolute())
{
    if (!current) {
        ASSERT_X(QString::asprintf("ConnectorInfo::ConnectorInfo(): invalid argument: %p", current));
    }
    // It is not always possible to determine the track number correctly from
    // the current element (for example, in case of a Segment).
    // If the caller does not know the track number and passes -1
    // it may be corrected later.
    if (track >= 0) {
        _currentLoc.setTrack(track);
    }
    if (frac >= Fraction(0, 1)) {
        _currentLoc.setFrac(frac);
    }
}

//---------------------------------------------------------
//   ConnectorInfo
//---------------------------------------------------------

ConnectorInfo::ConnectorInfo(const Score* score, const Location& currentLocation)
    : _score(score), _currentLoc(currentLocation)
{}

//---------------------------------------------------------
//   ConnectorInfo::updateLocation
//---------------------------------------------------------

void ConnectorInfo::updateLocation(const EngravingItem* e, Location& l, bool clipboardmode)
{
    l.fillForElement(e, clipboardmode);
}

//---------------------------------------------------------
//   ConnectorInfo::updateCurrentInfo
//---------------------------------------------------------

void ConnectorInfo::updateCurrentInfo(bool clipboardmode)
{
    if (!currentUpdated() && _current) {
        updateLocation(_current, _currentLoc, clipboardmode);
    }
    setCurrentUpdated(true);
}

//---------------------------------------------------------
//   ConnectorInfo::connect
//---------------------------------------------------------

bool ConnectorInfo::connect(ConnectorInfo* other)
{
    if (!other || (this == other)) {
        return false;
    }
    if (_type != other->_type || _score != other->_score) {
        return false;
    }
    if (hasPrevious() && _prev == nullptr
        && other->hasNext() && other->_next == nullptr
        ) {
        if ((_prevLoc == other->_currentLoc)
            && (_currentLoc == other->_nextLoc)
            ) {
            _prev = other;
            other->_next = this;
            return true;
        }
    }
    if (hasNext() && _next == nullptr
        && other->hasPrevious() && other->_prev == nullptr
        ) {
        if ((_nextLoc == other->_currentLoc)
            && (_currentLoc == other->_prevLoc)
            ) {
            _next = other;
            other->_prev = this;
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   ConnectorInfo::forceConnect
//---------------------------------------------------------

void ConnectorInfo::forceConnect(ConnectorInfo* other)
{
    if (!other || (this == other)) {
        return;
    }
    _next = other;
    other->_prev = this;
}

//---------------------------------------------------------
//   distance
//---------------------------------------------------------

static int distance(const Location& l1, const Location& l2)
{
    constexpr int commonDenominator = 1000;
    Fraction dfrac = (l2.frac() - l1.frac()).absValue();
    int dpos = dfrac.numerator() * commonDenominator / dfrac.denominator();
    dpos += 10000 * qAbs(l2.measure() - l1.measure());
    return 1000 * dpos + 100 * qAbs(l2.track() - l1.track()) + 10 * qAbs(l2.note() - l1.note()) + qAbs(l2.graceIndex() - l1.graceIndex());
}

//---------------------------------------------------------
//   ConnectorInfo::orderedConnectionDistance
//---------------------------------------------------------

int ConnectorInfo::orderedConnectionDistance(const ConnectorInfo& c1, const ConnectorInfo& c2)
{
    Location c1Next = c1._nextLoc;
    c1Next.toRelative(c1._currentLoc);
    Location c2Prev = c2._currentLoc;   // inversed order to get equal signs
    c2Prev.toRelative(c2._prevLoc);
    if (c1Next == c2Prev) {
        return distance(c1._nextLoc, c2._currentLoc);
    }
    return INT_MAX;
}

//---------------------------------------------------------
//   ConnectorInfo::connectionDistance
//    Returns a "distance" representing a likelihood of
//    that the checked connectors should be connected.
//    Returns 0 if can be readily connected via connect(),
//    < 0 if other is likely to be the first,
//    INT_MAX if cannot be connected
//---------------------------------------------------------

int ConnectorInfo::connectionDistance(const ConnectorInfo& other) const
{
    if (_type != other._type || _score != other._score) {
        return INT_MAX;
    }
    int distThisOther = INT_MAX;
    int distOtherThis = INT_MAX;
    if (hasNext() && _next == nullptr
        && other.hasPrevious() && other._prev == nullptr) {
        distThisOther = orderedConnectionDistance(*this, other);
    }
    if (hasPrevious() && _prev == nullptr
        && other.hasNext() && other._next == nullptr) {
        distOtherThis = orderedConnectionDistance(other, *this);
    }
    if (distOtherThis < distThisOther) {
        return -distOtherThis;
    }
    return distThisOther;
}

//---------------------------------------------------------
//   ConnectorInfo::findFirst
//---------------------------------------------------------

ConnectorInfo* ConnectorInfo::findFirst()
{
    ConnectorInfo* i = this;
    while (i->_prev) {
        i = i->_prev;
        if (i == this) {
            LOGW("ConnectorInfo::findFirst: circular connector %p", this);
            return nullptr;
        }
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::findFirst
//---------------------------------------------------------

const ConnectorInfo* ConnectorInfo::findFirst() const
{
    return const_cast<ConnectorInfo*>(this)->findFirst();
}

//---------------------------------------------------------
//   ConnectorInfo::findLast
//---------------------------------------------------------

ConnectorInfo* ConnectorInfo::findLast()
{
    ConnectorInfo* i = this;
    while (i->_next) {
        i = i->_next;
        if (i == this) {
            LOGW("ConnectorInfo::findLast: circular connector %p", this);
            return nullptr;
        }
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::findLast
//---------------------------------------------------------

const ConnectorInfo* ConnectorInfo::findLast() const
{
    return const_cast<ConnectorInfo*>(this)->findLast();
}

//---------------------------------------------------------
//   ConnectorInfo::finished
//---------------------------------------------------------

bool ConnectorInfo::finished() const
{
    return finishedLeft() && finishedRight();
}

//---------------------------------------------------------
//   ConnectorInfo::finishedLeft
//---------------------------------------------------------

bool ConnectorInfo::finishedLeft() const
{
    const ConnectorInfo* i = findFirst();
    return i && !i->hasPrevious();
}

//---------------------------------------------------------
//   ConnectorInfo::finishedRight
//---------------------------------------------------------

bool ConnectorInfo::finishedRight() const
{
    const ConnectorInfo* i = findLast();
    return i && !i->hasNext();
}

//---------------------------------------------------------
//   ConnectorInfo::start
//---------------------------------------------------------

ConnectorInfo* ConnectorInfo::start()
{
    ConnectorInfo* i = findFirst();
    if (i && i->hasPrevious()) {
        return nullptr;
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::end
//---------------------------------------------------------

ConnectorInfo* ConnectorInfo::end()
{
    ConnectorInfo* i = findLast();
    if (i && i->hasNext()) {
        return nullptr;
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfoReader
//---------------------------------------------------------

ConnectorInfoReader::ConnectorInfoReader(XmlReader& e, EngravingItem* current, int track)
    : ConnectorInfo(current, track), _reader(&e), _connector(nullptr), _connectorReceiver(current)
{}

//---------------------------------------------------------
//   readPositionInfo
//---------------------------------------------------------

static Location readPositionInfo(const XmlReader& e, int track)
{
    Location info = e.context()->location();
    info.setTrack(track);
    return info;
}

//---------------------------------------------------------
//   ConnectorInfoReader
//---------------------------------------------------------

ConnectorInfoReader::ConnectorInfoReader(XmlReader& e, Score* current, int track)
    : ConnectorInfo(current, readPositionInfo(e, track)), _reader(&e), _connector(nullptr), _connectorReceiver(current)
{
    setCurrentUpdated(true);
}

//---------------------------------------------------------
//   ConnectorInfoWriter
//---------------------------------------------------------

ConnectorInfoWriter::ConnectorInfoWriter(XmlWriter& xml, const EngravingItem* current, const EngravingItem* connector, int track,
                                         Fraction frac)
    : ConnectorInfo(current, track, frac), _xml(&xml), _connector(connector)
{
    if (!connector) {
        ASSERT_X(QString::asprintf("ConnectorInfoWriter::ConnectorInfoWriter(): invalid arguments: %p, %p", connector, current));
        return;
    }
    _type = connector->type();
    updateCurrentInfo(xml.context()->clipboardmode());
}

//---------------------------------------------------------
//   ConnectorInfoWriter::write
//---------------------------------------------------------

void ConnectorInfoWriter::write()
{
    XmlWriter& xml = *_xml;
    if (!xml.context()->canWrite(_connector)) {
        return;
    }
    xml.startObject(QString("%1 type=\"%2\"").arg(tagName(), _connector->typeName()));
    if (isStart()) {
        _connector->write(xml);
    }
    if (hasPrevious()) {
        xml.startObject("prev");
        _prevLoc.toRelative(_currentLoc);
        _prevLoc.write(xml);
        xml.endObject();
    }
    if (hasNext()) {
        xml.startObject("next");
        _nextLoc.toRelative(_currentLoc);
        _nextLoc.write(xml);
        xml.endObject();
    }
    xml.endObject();
}

//---------------------------------------------------------
//   ConnectorInfoReader::read
//---------------------------------------------------------

bool ConnectorInfoReader::read()
{
    XmlReader& e = *_reader;
    const QString name(e.attribute("type"));
    _type = Factory::name2type(&name);

    e.context()->fillLocation(_currentLoc);

    while (e.readNextStartElement()) {
        const QStringRef& tag(e.name());

        if (tag == "prev") {
            readEndpointLocation(_prevLoc);
        } else if (tag == "next") {
            readEndpointLocation(_nextLoc);
        } else {
            if (tag == name) {
                _connector = Factory::createItemByName(tag, _connectorReceiver->score()->dummy());
            } else {
                LOGW("ConnectorInfoReader::read: element tag (%s) does not match connector type (%s). Is the file corrupted?",
                     tag.toLatin1().constData(), name.toLatin1().constData());
            }

            if (!_connector) {
                e.unknown();
                return false;
            }
            _connector->setTrack(_currentLoc.track());
            _connector->read(e);
        }
    }
    return true;
}

//---------------------------------------------------------
//   ConnectorInfoReader::readEndpointLocation
//---------------------------------------------------------

void ConnectorInfoReader::readEndpointLocation(Location& l)
{
    XmlReader& e = *_reader;
    while (e.readNextStartElement()) {
        const QStringRef& tag(e.name());

        if (tag == "location") {
            l = Location::relative();
            l.read(e);
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   ConnectorInfoReader::update
//---------------------------------------------------------

void ConnectorInfoReader::update()
{
    if (!currentUpdated()) {
        updateCurrentInfo(_reader->context()->pasteMode());
    }
    if (hasPrevious()) {
        _prevLoc.toAbsolute(_currentLoc);
    }
    if (hasNext()) {
        _nextLoc.toAbsolute(_currentLoc);
    }
}

//---------------------------------------------------------
//   ConnectorInfoReader::addToScore
//---------------------------------------------------------

void ConnectorInfoReader::addToScore(bool pasteMode)
{
    ConnectorInfoReader* r = this;
    while (r->prev()) {
        r = r->prev();
    }
    while (r) {
        r->_connectorReceiver->readAddConnector(r, pasteMode);
        r = r->next();
    }
}

//---------------------------------------------------------
//   ConnectorInfoReader::readConnector
//---------------------------------------------------------

void ConnectorInfoReader::readConnector(std::unique_ptr<ConnectorInfoReader> info, XmlReader& e)
{
    if (!info->read()) {
        e.skipCurrentElement();
        return;
    }
    e.context()->addConnectorInfoLater(std::move(info));
}

//---------------------------------------------------------
//   ConnectorInfoReader::connector
//---------------------------------------------------------

EngravingItem* ConnectorInfoReader::connector()
{
    // connector should be contained in the first node normally.
    ConnectorInfo* i = findFirst();
    if (i) {
        return static_cast<ConnectorInfoReader*>(i)->_connector;
    }
    return nullptr;
}

//---------------------------------------------------------
//   ConnectorInfoReader::connector
//---------------------------------------------------------

const EngravingItem* ConnectorInfoReader::connector() const
{
    return const_cast<ConnectorInfoReader*>(this)->connector();
}

//---------------------------------------------------------
//   ConnectorInfoReader::releaseConnector
//---------------------------------------------------------

EngravingItem* ConnectorInfoReader::releaseConnector()
{
    ConnectorInfoReader* i = static_cast<ConnectorInfoReader*>(findFirst());
    if (!i) {
        // circular connector?
        ConnectorInfoReader* ii = this;
        EngravingItem* c = nullptr;
        while (ii->prev()) {
            if (ii->_connector) {
                c = ii->_connector;
                ii->_connector = nullptr;
            }
            ii = ii->prev();
            if (ii == this) {
                break;
            }
        }
        return c;
    }
    EngravingItem* c = i->_connector;
    i->_connector = nullptr;
    return c;
}
}
