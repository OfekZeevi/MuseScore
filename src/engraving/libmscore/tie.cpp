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
#include "tie.h"

#include <cmath>

#include "draw/transform.h"
#include "draw/pen.h"
#include "draw/brush.h"
#include "rw/xml.h"

#include "chord.h"
#include "measure.h"
#include "mscoreview.h"
#include "score.h"
#include "system.h"
#include "undo.h"
#include "chord.h"
#include "hook.h"
#include "ledgerline.h"
#include "accidental.h"
#include "stem.h"

#include "log.h"

using namespace mu;

namespace Ms {
Note* Tie::editStartNote;
Note* Tie::editEndNote;

TieSegment::TieSegment(System* parent)
    : SlurTieSegment(ElementType::TIE_SEGMENT, parent)
{
    autoAdjustOffset = mu::PointF();
}

TieSegment::TieSegment(const TieSegment& s)
    : SlurTieSegment(s)
{
    autoAdjustOffset = mu::PointF();
}

//---------------------------------------------------------
//   draw
//---------------------------------------------------------

void TieSegment::draw(mu::draw::Painter* painter) const
{
    TRACE_OBJ_DRAW;
    using namespace mu::draw;
    // hide tie toward the second chord of a cross-measure value
    if (tie()->endNote() && tie()->endNote()->chord()->crossMeasure() == CrossMeasure::SECOND) {
        return;
    }

    Pen pen(curColor());
    qreal mag = staff() ? staff()->staffMag(tie()->tick()) : 1.0;

    //Replace generic Qt dash patterns with improved equivalents to show true dots (keep in sync with slur.cpp)
    std::vector<double> dotted     = { 0.01, 1.99 };   // tighter than Qt PenStyle::DotLine equivalent - would be { 0.01, 2.99 }
    std::vector<double> dashed     = { 3.00, 3.00 };   // Compensating for caps. Qt default PenStyle::DashLine is { 4.0, 2.0 }
    std::vector<double> wideDashed = { 5.00, 6.00 };

    switch (slurTie()->styleType()) {
    case SlurStyleType::Solid:
        painter->setBrush(Brush(pen.color()));
        pen.setCapStyle(PenCapStyle::RoundCap);
        pen.setJoinStyle(PenJoinStyle::RoundJoin);
        pen.setWidthF(score()->styleMM(Sid::SlurEndWidth) * mag);
        break;
    case SlurStyleType::Dotted:
        painter->setBrush(BrushStyle::NoBrush);
        pen.setCapStyle(PenCapStyle::RoundCap);           // True dots
        pen.setDashPattern(dotted);
        pen.setWidthF(score()->styleMM(Sid::SlurDottedWidth) * mag);
        break;
    case SlurStyleType::Dashed:
        painter->setBrush(BrushStyle::NoBrush);
        pen.setDashPattern(dashed);
        pen.setWidthF(score()->styleMM(Sid::SlurDottedWidth) * mag);
        break;
    case SlurStyleType::WideDashed:
        painter->setBrush(BrushStyle::NoBrush);
        pen.setDashPattern(wideDashed);
        pen.setWidthF(score()->styleMM(Sid::SlurDottedWidth) * mag);
        break;
    case SlurStyleType::Undefined:
        break;
    }
    painter->setPen(pen);
    painter->drawPath(path);
}

bool TieSegment::isEditAllowed(EditData& ed) const
{
    if (ed.key == Qt::Key_X && !ed.modifiers) {
        return true;
    }

    if (ed.key == Qt::Key_Home && !ed.modifiers) {
        return true;
    }

    return false;
}

//---------------------------------------------------------
//   edit
//    return true if event is accepted
//---------------------------------------------------------

bool TieSegment::edit(EditData& ed)
{
    if (!isEditAllowed(ed)) {
        return false;
    }

    SlurTie* sl = tie();

    if (ed.key == Qt::Key_X && !ed.modifiers) {
        sl->setSlurDirection(sl->up() ? DirectionV::DOWN : DirectionV::UP);
        sl->layout();
        return true;
    }
    if (ed.key == Qt::Key_Home && !ed.modifiers) {
        ups(ed.curGrip).off = PointF();
        sl->layout();
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   changeAnchor
//---------------------------------------------------------

void TieSegment::changeAnchor(EditData& ed, EngravingItem* element)
{
    if (ed.curGrip == Grip::START) {
        spanner()->setStartElement(element);
        Note* note = toNote(element);
        if (note->chord()->tick() <= tie()->endNote()->chord()->tick()) {
            tie()->startNote()->setTieFor(0);
            tie()->setStartNote(note);
            note->setTieFor(tie());
        }
    } else {
        spanner()->setEndElement(element);
        Note* note = toNote(element);
        // do not allow backward ties
        if (note->chord()->tick() >= tie()->startNote()->chord()->tick()) {
            tie()->endNote()->setTieBack(0);
            tie()->setEndNote(note);
            note->setTieBack(tie());
        }
    }

    const size_t segments  = spanner()->spannerSegments().size();
    ups(ed.curGrip).off = PointF();
    spanner()->layout();
    if (spanner()->spannerSegments().size() != segments) {
        const std::vector<SpannerSegment*>& ss = spanner()->spannerSegments();

        TieSegment* newSegment = toTieSegment(ed.curGrip == Grip::END ? ss.back() : ss.front());
        score()->endCmd();
        score()->startCmd();
        ed.view()->changeEditElement(newSegment);
        triggerLayoutAll();
    }
}

//---------------------------------------------------------
//   editDrag
//---------------------------------------------------------

void TieSegment::editDrag(EditData& ed)
{
    Grip g = ed.curGrip;
    ups(g).off += ed.delta;

    if (g == Grip::START || g == Grip::END) {
        computeBezier();
        //
        // move anchor for slurs/ties
        //
        if ((g == Grip::START && isSingleBeginType()) || (g == Grip::END && isSingleEndType())) {
            Spanner* spanner = tie();
            EngravingItem* e = ed.view()->elementNear(ed.pos);
            Note* note = (e && e->isNote()) ? toNote(e) : nullptr;
            if (note && ((g == Grip::END && note->tick() > tie()->tick()) || (g == Grip::START && note->tick() < tie()->tick2()))) {
                if (g == Grip::END) {
                    Tie* tie = toTie(spanner);
                    if (tie->startNote()->pitch() == note->pitch()
                        && tie->startNote()->chord()->tick() < note->chord()->tick()) {
                        ed.view()->setDropTarget(note);
                        if (note != tie->endNote()) {
                            changeAnchor(ed, note);
                            return;
                        }
                    }
                }
            } else {
                ed.view()->setDropTarget(0);
            }
        }
    } else if (g == Grip::BEZIER1 || g == Grip::BEZIER2) {
        computeBezier();
    } else if (g == Grip::SHOULDER) {
        ups(g).off = PointF();
        computeBezier(ed.delta);
    } else if (g == Grip::DRAG) {
        ups(Grip::DRAG).off = PointF();
        roffset() += ed.delta;
    }

    // if this SlurSegment was automatically adjusted to avoid collision
    // lock this edit by resetting SlurSegment to default position
    // and incorporating previous adjustment into user offset
    PointF offset = getAutoAdjust();
    if (!offset.isNull()) {
        setAutoAdjust(0.0, 0.0);
        roffset() += offset;
    }
}

//---------------------------------------------------------
//   computeBezier
//    compute help points of slur bezier segment
//---------------------------------------------------------

void TieSegment::computeBezier(PointF shoulderOffset)
{
    qreal _spatium = spatium();
    qreal shoulderW; // height as fraction of slur-length
    qreal shoulderH;

    PointF tieStart = ups(Grip::START).p + ups(Grip::START).off;
    PointF tieEnd = ups(Grip::END).p + ups(Grip::END).off;

    PointF tieEndNormalized = tieEnd - tieStart;  // normalize to zero
    if (tieEndNormalized.x() == 0.0) {
        LOGD("zero tie");
        return;
    }

    qreal tieAngle = atan(tieEndNormalized.y() / tieEndNormalized.x()); // angle required from tie start to tie end--zero if horizontal
    Transform t;
    t.rotateRadians(-tieAngle);  // rotate so that we are working with horizontal ties regardless of endpoint height difference
    tieEndNormalized = t.map(tieEndNormalized);  // apply that rotation
    shoulderOffset = t.map(shoulderOffset);  // also apply to shoulderOffset

    double smallH = 0.38; // I don't know what this means currently
    qreal tieWidthInSp = tieEndNormalized.x() / _spatium;
    shoulderH = tieWidthInSp * 0.4 * smallH;  // magic math?
    shoulderH = qBound(shoulderHeightMin, shoulderH, shoulderHeightMax);
    shoulderH *= _spatium;  // shoulderH is now canvas units
    shoulderW = .6;

    shoulderH -= shoulderOffset.y();

    if (!tie()->up()) {
        shoulderH = -shoulderH;
    }

    qreal tieWidth = tieEndNormalized.x();
    qreal bezier1X = (tieWidth - tieWidth * shoulderW) * .5 + shoulderOffset.x();
    qreal bezier2X = bezier1X + tieWidth * shoulderW + shoulderOffset.x();

    PointF tieDrag = PointF(tieWidth * .5, 0.0);

    PointF bezier1(bezier1X, -shoulderH);
    PointF bezier2(bezier2X, -shoulderH);

    qreal w = score()->styleMM(Sid::SlurMidWidth) - score()->styleMM(Sid::SlurEndWidth);
    if (staff()) {
        w *= staff()->staffMag(tie()->tick());
    }
    PointF tieThickness(0.0, w);

    PointF bezier1Offset = shoulderOffset + t.map(ups(Grip::BEZIER1).off);
    PointF bezier2Offset = shoulderOffset + t.map(ups(Grip::BEZIER2).off);

    if (!shoulderOffset.isNull()) {
        PointF invertedShoulder = t.inverted().map(shoulderOffset);
        ups(Grip::BEZIER1).off += invertedShoulder;
        ups(Grip::BEZIER2).off += invertedShoulder;
    }

    //-----------------------------------calculate p6
    PointF bezier1Final = bezier1 + bezier1Offset;
    PointF bezier2Final = bezier2 + bezier2Offset;
    PointF bezierNormalized = bezier2Final - bezier1Final;

    qreal bezierAngle = atan(bezierNormalized.y() / bezierNormalized.x());  // in case bezier1 and bezier2 are not horizontal
    t.reset();
    t.rotateRadians(-bezierAngle);
    PointF tieShoulder = PointF(t.map(bezierNormalized).x() * .5, 0.0);

    t.rotateRadians(2 * bezierAngle);
    tieShoulder = t.map(tieShoulder) + bezier1Final - shoulderOffset;
    //-----------------------------------

    path = PainterPath();
    path.moveTo(PointF());
    path.cubicTo(bezier1 + bezier1Offset - tieThickness, bezier2 + bezier2Offset - tieThickness, tieEndNormalized);
    if (tie()->styleType() == SlurStyleType::Solid) {
        path.cubicTo(bezier2 + bezier2Offset + tieThickness, bezier1 + bezier1Offset + tieThickness, PointF());
    }

    tieThickness = PointF(0.0, 3.0 * w);
    shapePath = PainterPath();
    shapePath.moveTo(PointF());
    shapePath.cubicTo(bezier1 + bezier1Offset - tieThickness, bezier2 + bezier2Offset - tieThickness, tieEndNormalized);
    shapePath.cubicTo(bezier2 + bezier2Offset + tieThickness, bezier1 + bezier1Offset + tieThickness, PointF());

    // translate back
    t.reset();
    t.translate(tieStart.x(), tieStart.y());
    t.rotateRadians(tieAngle);
    path = t.map(path);
    shapePath = t.map(shapePath);
    ups(Grip::BEZIER1).p = t.map(bezier1);
    ups(Grip::BEZIER2).p = t.map(bezier2);
    ups(Grip::END).p = t.map(tieEndNormalized) - ups(Grip::END).off;
    ups(Grip::DRAG).p = t.map(tieDrag);
    ups(Grip::SHOULDER).p = t.map(tieShoulder);

    _shape.clear();
    PointF start;
    start = t.map(start);

    qreal minH = qAbs(3.0 * w);
    int nbShapes = 15;
    const CubicBezier b(tieStart, ups(Grip::BEZIER1).pos(), ups(Grip::BEZIER2).pos(), ups(Grip::END).pos());
    for (int i = 1; i <= nbShapes; i++) {
        const PointF point = b.pointAtPercent(i / float(nbShapes));
        RectF re = RectF(start, point).normalized();
        if (re.height() < minH) {
            tieWidthInSp = (minH - re.height()) * .5;
            re.adjust(0.0, -tieWidthInSp, 0.0, tieWidthInSp);
        }
        _shape.add(re);
        start = point;
    }
}

//---------------------------------------------------------
//   adjustY
//    adjust the y-position of the tie. this is called before adjustX()
//    p1, p2  are in System coordinates
//---------------------------------------------------------

void TieSegment::adjustY(const PointF& p1, const PointF& p2)
{
    autoAdjustOffset = PointF();
    const StaffType* staffType = this->staffType();
    bool useTablature = staffType->isTabStaff();
    Tie* t = toTie(slurTie());
    Chord* sc = t->startNote() ? t->startNote()->chord() : 0;

    if (!sc) {
        return; // don't adjust these ties vertically
    }
    qreal sp = spatium();
    const qreal ld = staff()->lineDistance(sc->tick()) * sp;
    const qreal lines = staff()->lines(sc->tick());
    const int line = t->startNote()->line();
    shoulderHeightMin = 0.4;
    shoulderHeightMax = 1.3;
    qreal tieAdjustSp = 0;

    const qreal staffLineOffset = 0.125 + (styleP(Sid::staffLineWidth) / 2 / ld); // sp
    const qreal noteHeadOffset = 0.185; // sp
    bool isUp = t->up();

    setPos(PointF());
    ups(Grip::START).p = p1;
    ups(Grip::END).p = p2;

    //Adjust Y pos to staff type offset before other calculations
    if (staffType) {
        rypos() += staffType->yoffset().val() * spatium();
    }

    if (isNudged() || isEdited()) {
        return;
    }
    if (!t->isInside()) {
        setAutoAdjust(PointF(0, noteHeadOffset * spatium() * (slurTie()->up() ? -1 : 1)));
    }
    RectF bbox;
    if (p1.y() == p2.y()) {
#if 0
        // for horizontal ties we can estimate the bbox using simple math instead of having to call
        // computeBezier() which uses a whole lot of trigonometry to draw the entire tie
        bbox.setX(p1.x());
        bbox.setWidth(p2.x() - p1.x());

        // The following is ripped from computeBezier()
        // TODO: refactor this into its own method
        qreal shoulderHeight = bbox.width() * 0.4 * 0.38;
        shoulderHeight = qBound(shoulderHeightMin * spatium(), shoulderHeight, shoulderHeightMax * spatium());
        //////////
        qreal actualHeight = 3 * (shoulderHeight + styleP(Sid::SlurMidWidth)) / 4;

        bbox.setY(p1.y() - (slurTie()->up() ? actualHeight : 0));
        bbox.setHeight(actualHeight);
#else
        // more correct, less efficient
        computeBezier();
        bbox = path.boundingRect();
#endif
    } else {
        // don't adjust ties that aren't horizontal, just add offset
        return;
    }

    auto spansBarline = [staffLineOffset, lines](qreal a, qreal b) {
        if (b < a) {
            qSwap(a, b);
        }
        if (b < -staffLineOffset || a > (lines - 1) + staffLineOffset) {
            return false;
        }
        if (a < -staffLineOffset && b > staffLineOffset) {
            // a and b straddle line zero
            return true;
        }
        if (floor(a - staffLineOffset) != floor(b + staffLineOffset)) {
            return true;
        }
        return false;
    };

    qreal endpointYsp = (bbox.y() + (isUp ? bbox.height() : 0)) / ld;
    qreal tieHeightSp = bbox.height() / ld;
    qreal tieThicknessSp = (styleP(Sid::SlurMidWidth) + ((styleP(Sid::SlurMidWidth) - styleP(Sid::SlurEndWidth)) / 2)) / ld;
    qreal tieMidOutsideSp = endpointYsp + (isUp ? -tieHeightSp : tieHeightSp);
    qreal tieMidInsideSp = tieMidOutsideSp + (isUp ? (tieThicknessSp) : -(tieThicknessSp));
    if (useTablature && t->isInside()) {
        const qreal tieEndpointOffsetSp = 0.2;
        Note* sn = tie()->startNote();
        int string = sn->string();
        shoulderHeightMax = 4 / 3; // at max ties will be 1sp tall
        qreal newAnchor = (qreal)string;
        newAnchor += tieEndpointOffsetSp * (isUp ? -1 : 1);
        setAutoAdjust(PointF(0, (newAnchor - endpointYsp) * ld));
    } else if (!t->isInside()) {
        // OUTSIDE TIES

        qreal endpointYLineDist = endpointYsp - floor(endpointYsp);

        // ENDPOINTS ////////////////////////////////
        // If the endpoints are less than staffLineOffset from a line, they need to be adjusted
        // in the direction of the tie.
        qreal newAnchor = endpointYsp;
        bool farAdjust = false;
        if ((isUp && endpointYsp > -staffLineOffset) || (!isUp && endpointYsp < (lines - 1) + staffLineOffset)) {
            if (isUp) {
                if (endpointYLineDist < staffLineOffset) {
                    newAnchor = floor(endpointYsp) - staffLineOffset;
                    farAdjust = true;
                } else if (endpointYLineDist > (1 - staffLineOffset)) {
                    newAnchor = ceil(endpointYsp) - staffLineOffset;
                }
            } else { // down
                if (endpointYLineDist < staffLineOffset) {
                    newAnchor = floor(endpointYsp) + staffLineOffset;
                } else if (endpointYLineDist > (1 - staffLineOffset)) {
                    newAnchor = ceil(endpointYsp) + staffLineOffset;
                    farAdjust = true;
                }
            }
            tieAdjustSp += newAnchor - endpointYsp;
            tieMidOutsideSp += tieAdjustSp;
            tieMidInsideSp += tieAdjustSp;

            // TIE APOGEE ///////////////////////////////
            // If the middle of the tie conflicts with a staff line, the tie must be adjusted to resolve
            // that collision.
            if (farAdjust) {
                // we've already adjusted the tie pretty far from the notehead, so let's just
                // constrain the tie height to fit within a single space
                if (endpointYsp + tieAdjustSp > 0 && endpointYsp + tieAdjustSp < lines - 1) {
                    shoulderHeightMax = 4 * (1 - ((staffLineOffset * 2) + (tieThicknessSp / 2))) / 3;
                }
            } else {
                if (spansBarline(tieMidOutsideSp, tieMidInsideSp)) {
                    newAnchor = tieMidInsideSp;
                    if (isUp) {
                        newAnchor = floor(tieMidInsideSp + staffLineOffset) - staffLineOffset;
                    } else { // down
                        newAnchor = ceil(tieMidInsideSp - staffLineOffset) + staffLineOffset;
                    }
                    tieAdjustSp += newAnchor - tieMidInsideSp;
                    // we've adjusted the midpoint, but maybe the endpoint is too close to a barline now
                    qreal newEndpoint = endpointYsp + tieAdjustSp;
                    newAnchor = newEndpoint;
                    if (isUp && newEndpoint - floor(newEndpoint + staffLineOffset) < staffLineOffset) {
                        // clamp endpoint and adjust tie height
                        newAnchor = floor(newEndpoint + staffLineOffset) + staffLineOffset;
                        shoulderHeightMin = 4 * (staffLineOffset * 2 + (tieThicknessSp / 2)) / 3;
                        shoulderHeightMax = shoulderHeightMin;
                    } else if (!isUp && ceil(newEndpoint - staffLineOffset) - newEndpoint < staffLineOffset) {
                        // clamp endpoint and adjust tie height
                        newAnchor = ceil(newEndpoint - staffLineOffset) - staffLineOffset;
                        shoulderHeightMin = 4 * (staffLineOffset * 2 + (tieThicknessSp / 2)) / 3;
                        shoulderHeightMax = shoulderHeightMin;
                    }
                    tieAdjustSp += newAnchor - newEndpoint;
                }
            }
        }
        setAutoAdjust(PointF(0, (tieAdjustSp * ld) - (p1.y() - (endpointYsp * ld))));
    } else {
        // INSIDE TIES (non-tab)
        bool collideAbove = false;
        bool collideBelow = false;
        Note* sn = tie()->startNote();
        Chord* sc = sn->chord();

        // figure out if there are situations where a tie collides with a tie above or below it
        // in a chord
        for (Note* note : sc->notes()) {
            if (note == sn || !note->tieFor()) {
                continue;
            }
            if (note->line() == sn->line() - 1 && t->up() == note->tieFor()->up()) {
                collideAbove = true;
            }
            if (note->line() == sn->line() + 1 && t->up() == note->tieFor()->up()) {
                collideBelow = true;
            }
        }
        shoulderHeightMax = 4 / 3; // at max ties will be 1sp tall
        // are the endpoints within the staff?
        if (line > 0 && line < (lines - 1) * 2) {
            // ENDPOINTS ////////////////////////////////
            // Each line position in the staff has a set endpoint Y location
            qreal newAnchor;
            if (isUp) {
                newAnchor = floor(line / 2) + ((line & 1) ? staffLineOffset : -staffLineOffset);
            } else {
                newAnchor = floor((line + 1) / 2) + ((line & 1) ? -staffLineOffset : staffLineOffset);
            }

            // TIE APOGEE ///////////////////////////////
            // Constrain tie height to avoid staff line collisions
            if (line & 1) {
                // tie endpoint is right below the line, so let's adjust the height so that the top clears the line
                shoulderHeightMin = 4 * ((staffLineOffset * 2) + (tieThicknessSp / 2)) / 3;
            } else {
                // avoid collisions with the next line up by constraining maximum
                shoulderHeightMax = 4 * (1 - ((staffLineOffset * 2) + tieThicknessSp / 2)) / 3;
            }
            if ((isUp && collideBelow) || (!isUp && collideAbove)) {
                shoulderHeightMin = 4 * ((staffLineOffset * 2) + (tieThicknessSp / 2)) / 3;
            }
            if ((isUp && collideAbove && newAnchor > staffLineOffset)
                || (!isUp && collideBelow && newAnchor < (lines - 1))) {
                shoulderHeightMax = 4 * (1 - (staffLineOffset * 2) - (tieThicknessSp / 2)) / 3;
            }

            setAutoAdjust(PointF(0, (newAnchor - endpointYsp) * ld));
        }
    }
}

//---------------------------------------------------------
//   finalizeSegment
//    compute the bezier and adjust the bbox for the curve
//---------------------------------------------------------

void TieSegment::finalizeSegment()
{
    computeBezier();
    setbbox(path.boundingRect());
}

//---------------------------------------------------------
//   adjustX
//    adjust the tie endpoints to avoid staff lines. call adjustY() first!
//---------------------------------------------------------

void TieSegment::adjustX()
{
    qreal offsetMargin = spatium() * 0.25;
    qreal collisionYMargin = spatium() * 0.25;
    Note* sn = tie()->startNote();
    Note* en = tie()->endNote();
    Chord* sc = sn ? sn->chord() : nullptr;
    Chord* ec = en ? en->chord() : nullptr;

    qreal xo = 0;

    if (isNudged() || isEdited()) {
        return;
    }

    // ADJUST LEFT GRIP -----------
    if (sc && (spannerSegmentType() == SpannerSegmentType::SINGLE || spannerSegmentType() == SpannerSegmentType::BEGIN)) {
        // grips are in system coordinates, normalize to note position
        PointF p1 = ups(Grip::START).p + PointF(system()->pos().x() - sn->canvasX() + sn->headWidth(), 0);
        xo = 0;
        if (tie()->isInside()) {  // only adjust for inside-style ties
            // for cross-voice collisions, we need a list of all chords at this tick
            std::vector<Chord*> chords;
            track_idx_t strack = sc->staffIdx() * VOICES;
            track_idx_t etrack = sc->staffIdx() * VOICES + VOICES;
            chords.push_back(sc);
            for (track_idx_t track = strack; track < etrack; ++track) {
                if (Chord* ch = sc->measure()->findChord(sc->tick(), track)) {
                    const std::vector<Chord*>& graceNotes = ch->graceNotes();
                    if (ch != sc && std::find(graceNotes.begin(), graceNotes.end(), sc) == graceNotes.end()) {
                        chords.push_back(ch);
                    }
                }
            }

            for (Chord* chord : chords) {
                qreal chordOffset = chord->x() - sc->x() - sn->x() - sn->width(); // sn for right-offset notes, width() to normalize to zero
                // adjust for hooks
                if (chord->hook() && chord->hook()->visible()) {
                    qreal hookHeight = chord->hook()->bbox().height();
                    // turn the hook upside down for downstems
                    qreal hookY = chord->hook()->pos().y() - (chord->up() ? 0 : hookHeight);
                    if (p1.y() > hookY - collisionYMargin && p1.y() < hookY + hookHeight + collisionYMargin) {
                        xo = qMax(xo, chord->hook()->x() + chord->hook()->width() + chordOffset);
                    }
                }

                // adjust for stems
                if (chord->stem() && chord->stem()->visible()) {
                    qreal stemLen = chord->stem()->bbox().height();
                    qreal stemY = chord->stem()->pos().y() - (chord->up() ? stemLen : 0);
                    if (p1.y() > stemY - collisionYMargin && p1.y() < stemY + stemLen + collisionYMargin) {
                        xo = qMax(xo, chord->stem()->x() + chord->stem()->width() + chordOffset);
                    }
                }

                // adjust for ledger lines
                for (LedgerLine* currLedger = chord->ledgerLines(); currLedger; currLedger = currLedger->next()) {
                    // search through ledger lines and see if any are within .5sp of tie start
                    if (qAbs(p1.y() - currLedger->y()) < spatium() * 0.5) {
                        xo = qMax(xo, (currLedger->x() + currLedger->len() + chordOffset));
                        break;
                    }
                }

                for (auto note : chord->notes()) {
                    // adjust for dots
                    if (note->dots().size() > 0) {
                        qreal dotY = note->pos().y() + note->dots().back()->y();
                        if (qAbs(p1.y() - dotY) < spatium() * 0.5) {
                            xo = qMax(xo, note->x() + note->dots().back()->x() + note->dots().back()->width() + chordOffset);
                        }
                    }

                    // adjust for note collisions
                    if (note == sn) {
                        continue;
                    }
                    qreal noteTop = note->y() + note->bbox().top();
                    qreal noteHeight = note->height();
                    if (p1.y() > noteTop - collisionYMargin && p1.y() < noteTop + noteHeight + collisionYMargin) {
                        xo = qMax(xo, note->x() + note->width() + chordOffset);
                    }
                }
            }
            xo += offsetMargin;
        } else { // tie is outside
            if ((slurTie()->up() && sc->up()) || (!slurTie()->up() && !sc->up())) {
                // outside ties may still require adjustment for hooks
                if (sc->hook() && sc->hook()->visible()) {
                    qreal hookHeight = sc->hook()->bbox().height();
                    // turn the hook upside down for downstems
                    qreal hookY = sc->hook()->pos().y() - (sc->up() ? 0 : hookHeight);
                    if (p1.y() > hookY - collisionYMargin && p1.y() < hookY + hookHeight + collisionYMargin) {
                        qreal tieAttach = sn->outsideTieAttachX(slurTie()->up());
                        qreal hookOffsetX = sc->hook()->width() - (slurTie()->up() ? 0 : tieAttach);
                        xo = hookOffsetX + offsetMargin;
                    }
                } else if (sc->stem()) {
                    xo = offsetMargin;
                }
            } else if (sn->tieBack()) {
                xo += spatium() / 6; // 1/3 spatium in either direction, so .33/2
            } else {
                xo += spatium() / 8; // tiny offset to the right
            }
        }
        xo *= sc->mag();
        ups(Grip::START).p += PointF(xo, 0);
    }

    // ADJUST RIGHT GRIP ----------
    if (ec && (spannerSegmentType() == SpannerSegmentType::SINGLE || spannerSegmentType() == SpannerSegmentType::END)) {
        // grips are in system coordinates, normalize to note position
        PointF p2 = ups(Grip::END).p + PointF(system()->pos().x() - en->canvasX(), 0);
        xo = 0;
        if (tie()->isInside()) {
            // for inter-voice collisions, we need a list of all notes from all voices
            std::vector<Chord*> chords;
            track_idx_t strack = ec->staffIdx() * VOICES;
            track_idx_t etrack = ec->staffIdx() * VOICES + VOICES;
            for (track_idx_t track = strack; track < etrack; ++track) {
                if (Chord* ch = ec->measure()->findChord(ec->tick(), track)) {
                    chords.push_back(ch);
                }
            }

            for (Chord* chord : chords) {
                qreal chordOffset = (ec->x() + en->x()) - chord->x(); // en->x() for right-offset notes
                for (LedgerLine* currLedger = chord->ledgerLines(); currLedger; currLedger = currLedger->next()) {
                    // search through ledger lines and see if any are within .5sp of tie end
                    if (qAbs(p2.y() - currLedger->y()) < spatium() * 0.5) {
                        xo = qMin(xo, currLedger->x() - chordOffset);
                    }
                }

                if (chord->stem() && chord->stem()->visible()) {
                    // adjust for stems
                    qreal stemLen = chord->stem()->bbox().height();
                    qreal stemY = chord->stem()->pos().y() - (chord->up() ? stemLen : 0);
                    if (p2.y() > stemY - offsetMargin && p2.y() < stemY + stemLen + offsetMargin) {
                        xo = qMin(xo, chord->stem()->x() - chordOffset);
                    }
                }

                for (Note* note : chord->notes()) {
                    // adjust for accidentals
                    Accidental* acc = note->accidental();
                    if (acc && acc->visible()) {
                        qreal accTop = (note->y() + acc->y()) + acc->bbox().top();
                        qreal accHeight = acc->height();
                        if (p2.y() >= accTop && p2.y() <= accTop + accHeight) {
                            xo = qMin(xo, note->x() + acc->x() - chordOffset);
                        }
                    }

                    if (note == en) {
                        continue;
                    }
                    // adjust for shifted notes (such as intervals of unison or second)
                    qreal noteTop = note->y() + note->bbox().top();
                    qreal noteHeight = note->headHeight();
                    if (p2.y() >= noteTop - collisionYMargin && p2.y() <= noteTop + noteHeight + collisionYMargin) {
                        xo = qMin(xo, note->x() - chordOffset);
                    }
                }
            }
            xo -= offsetMargin;
        } else {
            // tie is outside
            if (!tie()->up() && !ec->up() && ec->stem() && ec->stem()->visible()) {
                xo -= offsetMargin;
            } else if (en && en->tieFor()) {
                xo -= spatium() / 6;
            } else {
                xo -= spatium() / 8;
            }
        }
        xo *= ec->mag();
        ups(Grip::END).p += PointF(xo, 0);
    }
}

//---------------------------------------------------------
//   setAutoAdjust
//---------------------------------------------------------

void TieSegment::setAutoAdjust(const PointF& offset)
{
    PointF diff = offset - autoAdjustOffset;
    if (!diff.isNull()) {
        path.translate(diff);
        shapePath.translate(diff);
        _shape.translate(diff);
        for (int i = 0; i < int(Grip::GRIPS); ++i) {
            _ups[i].p += diff;
        }
        autoAdjustOffset = offset;
    }
}

//---------------------------------------------------------
//   isEdited
//---------------------------------------------------------

bool TieSegment::isEdited() const
{
    for (int i = 0; i < int(Grip::GRIPS); ++i) {
        if (!_ups[i].off.isNull()) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   slurPos
//    Calculate position of start- and endpoint of slur
//    relative to System() position.
//---------------------------------------------------------

void Tie::slurPos(SlurPos* sp)
{
    const StaffType* staffType = this->staffType();
    bool useTablature = staffType->isTabStaff();
    qreal _spatium = spatium();
    qreal hw = startNote()->tabHeadWidth(staffType) * mag(); // if staffType == 0, defaults to headWidth()
    /* Inside-style and Outside-style ties
     Outside ties connect above the notehead, in the middle. Ideally, we'd use opticalcenter for this, but
     that Smufl anchor is not available for noteheads yet. For this reason, we rely on Note::outsideTieAttachX()
     which makes its best guess as to where ties should connect.

     As for y connection point, inside-style ties are decided by the space or line the note occupies in TieSegment::adjustY()
     so we don't need to worry about that. Outside-style ties will be 0.125 spatium from the top or bottom of the notehead.
     We can parameterize that later, but this describes only a minimum distance from the notehead, it can be changed, again,
     in TieSegment::adjustY().
    */

    Chord* sc = startNote()->chord();
    Chord* ec = endNote() ? endNote()->chord() : nullptr;
    sp->system1 = sc->measure()->system();
    if (!sp->system1) {
        Measure* m = sc->measure();
        LOGD("No system: measure is %d has %d count %d", m->isMMRest(), m->hasMMRest(), m->mmRestCount());
    }

    qreal x1, y1;
    qreal x2, y2;

    // determine attachment points
    // similar code is used in Chord::layoutPitched()
    // to allocate extra space to enforce minTieLength
    // so keep these in sync
    if (sc->notes().size() > 1 || (ec && ec->notes().size() > 1)) {
        _isInside = true;
    } else {
        _isInside = false;
    }
    sp->p1    = sc->pos() + sc->segment()->pos() + sc->measure()->pos();

    //------p1
    y1 = startNote()->pos().y();
    y2 = endNote() ? endNote()->pos().y() : y1;

    // force tie to be horizontal except for cross-staff or if there is a difference of line (tpc, clef)
    int line1 = useTablature ? startNote()->string() : startNote()->line();
    int line2 = line1;
    if (endNote()) {
        line2 = useTablature ? endNote()->string() : endNote()->line();
    }
    bool isHorizontal = ec ? line1 == line2 && sc->vStaffIdx() == ec->vStaffIdx() : true;
    y1 += startNote()->bbox().y();
    if (endNote()) {
        y2 += endNote()->bbox().y();
    }
    if (!up()) {
        y1 += startNote()->bbox().height();
        if (endNote()) {
            y2 += endNote()->bbox().height();
        }
    }
    if (!endNote()) {
        y2 = y1;
    }

    // ensure that horizontal ties remain horizontal
    if (isHorizontal) {
        y1 = _up ? qMin(y1, y2) : qMax(y1, y2);
        y2 = _up ? qMin(y1, y2) : qMax(y1, y2);
    }

    if (_isInside) {
        x1 = startNote()->pos().x() + hw;  // the offset for these will be decided in TieSegment::adjustX()
    } else {
        if (sc->stem() && sc->stem()->visible() && sc->up() && _up) {
            // usually, outside ties start in the middle of the notehead, but
            // for up-ties on up-stems, we'll start at the end of the notehead
            // to avoid the stem
            x1 = startNote()->pos().x() + hw;
        } else {
            x1 = startNote()->outsideTieAttachX(_up);
        }
    }

    sp->p1 += PointF(x1, y1);

    //------p2
    if (!ec) {
        sp->p2 = sp->p1 + PointF(_spatium * 3, 0.0);
        sp->system2 = sp->system1;
        return;
    }
    sp->p2 = ec->pos() + ec->segment()->pos() + ec->measure()->pos();
    sp->system2 = ec->measure()->system();

    if (isInside()) {
        x2 = endNote()->x();
    } else {
        if (ec->stem() && ec->stem()->visible() && !ec->up() && !_up) {
            // as before, xo should account for stems that could get in the way
            x2 = endNote()->x();
        } else {
            x2 = endNote()->outsideTieAttachX(_up);
        }
    }
    sp->p2 += PointF(x2, y2);

    // adjust for cross-staff
    if (sc->vStaffIdx() != vStaffIdx() && sp->system1) {
        qreal diff = sp->system1->staff(sc->vStaffIdx())->y() - sp->system1->staff(vStaffIdx())->y();
        sp->p1.ry() += diff;
    }
    if (ec->vStaffIdx() != vStaffIdx() && sp->system2) {
        qreal diff = sp->system2->staff(ec->vStaffIdx())->y() - sp->system2->staff(vStaffIdx())->y();
        sp->p2.ry() += diff;
    }
}

//---------------------------------------------------------
//   Tie
//---------------------------------------------------------

Tie::Tie(EngravingItem* parent)
    : SlurTie(ElementType::TIE, parent)
{
    setAnchor(Anchor::NOTE);
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void Tie::write(XmlWriter& xml) const
{
    xml.startObject(this);
    SlurTie::writeProperties(xml);
    xml.endObject();
}

//---------------------------------------------------------
//   calculateDirection
//---------------------------------------------------------

static int compareNotesPos(const Note* n1, const Note* n2)
{
    if (n1->line() != n2->line()) {
        return n2->line() - n1->line();
    } else if (n1->string() != n2->string()) {
        return n2->string() - n1->string();
    } else {
        return n1->pitch() - n2->pitch();
    }
}

void Tie::calculateDirection()
{
    Chord* c1   = startNote()->chord();
    Chord* c2   = endNote()->chord();
    Measure* m1 = c1->measure();
    Measure* m2 = c2->measure();

    if (_slurDirection == DirectionV::AUTO) {
        std::vector<Note*> notes = c1->notes();
        size_t n = notes.size();
        // if there are multiple voices, the tie direction goes on stem side
        if (m1->hasVoices(c1->staffIdx(), c1->tick(), c1->actualTicks())) {
            _up = c1->up();
        } else if (m2->hasVoices(c2->staffIdx(), c2->tick(), c2->actualTicks())) {
            _up = c2->up();
        } else if (n == 1) {
            //
            // single note
            //
            if (c1->up() != c2->up()) {
                // if stem direction is mixed, always up
                _up = true;
            } else {
                _up = !c1->up();
            }
        } else {
            //
            // chords
            //
            // first, find pivot point in chord (below which all ties curve down and above which all ties curve up)
            Note* pivotPoint = nullptr;
            bool multiplePivots = false;
            for (size_t i = 0; i < n - 1; ++i) {
                if (!notes[i]->tieFor()) {
                    continue; // don't include notes that don't have ties
                }
                for (size_t j = i + 1; j < n; ++j) {
                    if (!notes[j]->tieFor()) {
                        continue;
                    }
                    int noteDiff = compareNotesPos(notes[i], notes[j]);
                    if (!multiplePivots && qAbs(noteDiff) <= 1) {
                        // TODO: Fix unison ties somehow--if noteDiff == 0 then we need to determine which of the unison is 'lower'
                        if (pivotPoint) {
                            multiplePivots = true;
                            pivotPoint = nullptr;
                        } else {
                            pivotPoint = noteDiff < 0 ? notes[i] : notes[j];
                        }
                    }
                }
            }
            if (!pivotPoint) {
                // if the pivot point was not found (either there are no unisons/seconds or there are more than one),
                // determine if this note is in the lower or upper half of this chord
                int notesAbove = 0, tiesAbove = 0;
                int notesBelow = 0, tiesBelow = 0;
                int unisonNotes = 0, unisonTies = 0;
                for (size_t i = 0; i < n; ++i) {
                    if (notes[i] == startNote()) {
                        // skip counting if this note is the current note or if this note doesn't have a tie
                        continue;
                    }
                    int noteDiff = compareNotesPos(startNote(), notes[i]);
                    if (noteDiff == 0) {  // unison
                        unisonNotes++;
                        if (notes[i]->tieFor()) {
                            unisonTies++;
                        }
                    }
                    if (noteDiff < 0) { // the note is above startNote
                        notesAbove++;
                        if (notes[i]->tieFor()) {
                            tiesAbove++;
                        }
                    }
                    if (noteDiff > 0) { // the note is below startNote
                        notesBelow++;
                        if (notes[i]->tieFor()) {
                            tiesBelow++;
                        }
                    }
                }

                if (tiesAbove == 0 && tiesBelow == 0 && unisonTies == 0) {
                    // this is the only tie in the chord.
                    if (notesAbove == notesBelow) {
                        _up = !c1->up();
                    } else {
                        _up = (notesAbove < notesBelow);
                    }
                } else if (tiesAbove == tiesBelow) {
                    // this note is dead center, so its tie should go counter to the stem direction
                    _up = !c1->up();
                } else {
                    _up = (tiesAbove < tiesBelow);
                }
            } else if (pivotPoint == startNote()) {
                // the current note is the lower of the only second or unison in the chord; tie goes down.
                _up = false;
            } else {
                // if lower than the pivot, tie goes down, otherwise up
                int noteDiff = compareNotesPos(startNote(), pivotPoint);
                _up = (noteDiff >= 0);
            }
        }
    } else {
        _up = _slurDirection == DirectionV::UP ? true : false;
    }
}

//---------------------------------------------------------
//   layoutFor
//    layout the first SpannerSegment of a slur
//---------------------------------------------------------

TieSegment* Tie::layoutFor(System* system)
{
    // do not layout ties in tablature if not showing back-tied fret marks
    StaffType* st = staff()->staffType(startNote() ? startNote()->tick() : Fraction(0, 1));
    if (st && st->isTabStaff() && !st->showBackTied()) {
        if (!segmentsEmpty()) {
            eraseSpannerSegments();
        }
        return nullptr;
    }
    //
    //    show short bow
    //
    if (startNote() == 0 || endNote() == 0) {
        if (startNote() == 0) {
            LOGD("no start note");
            return 0;
        }
        Chord* c1 = startNote()->chord();
        setTick(c1->tick());
        if (_slurDirection == DirectionV::AUTO) {
            if (c1->measure()->hasVoices(c1->staffIdx(), c1->tick(), c1->actualTicks())) {
                // in polyphonic passage, ties go on the stem side
                _up = c1->up();
            } else {
                _up = !c1->up();
            }
        } else {
            _up = _slurDirection == DirectionV::UP ? true : false;
        }
        fixupSegments(1);
        TieSegment* segment = segmentAt(0);
        segment->setSpannerSegmentType(SpannerSegmentType::SINGLE);
        segment->setSystem(startNote()->chord()->segment()->measure()->system());
        SlurPos sPos;
        slurPos(&sPos);
        segment->adjustY(sPos.p1, sPos.p2);
        segment->finalizeSegment();
        return segment;
    }
    calculateDirection();

    SlurPos sPos;
    slurPos(&sPos);  // get unadjusted x values and determine inside or outside

    setPos(0, 0);

    int n;
    if (sPos.system1 != sPos.system2) {
        n = 2;
        sPos.p2 = PointF(system->lastNoteRestSegmentX(true), sPos.p1.y());
    } else {
        n = 1;
    }

    fixupSegments(n);
    TieSegment* segment = segmentAt(0);
    segment->setSystem(system);   // Needed to populate System.spannerSegments
    Chord* c1 = startNote()->chord();
    setTick(c1->tick());
    segment->adjustY(sPos.p1, sPos.p2); // adjust vertically
    segment->setSpannerSegmentType(sPos.system1 != sPos.system2 ? SpannerSegmentType::BEGIN : SpannerSegmentType::SINGLE);
    segment->adjustX(); // adjust horizontally for inside-style ties
    segment->finalizeSegment(); // compute bezier and set bbox
    return segment;
}

//---------------------------------------------------------
//   layoutBack
//    layout the second SpannerSegment of a split slur
//---------------------------------------------------------

TieSegment* Tie::layoutBack(System* system)
{
    // do not layout ties in tablature if not showing back-tied fret marks
    StaffType* st = staff()->staffType(startNote() ? startNote()->tick() : Fraction(0, 1));
    if (st->isTabStaff() && !st->showBackTied()) {
        if (!segmentsEmpty()) {
            eraseSpannerSegments();
        }
        return nullptr;
    }

    SlurPos sPos;
    slurPos(&sPos);

    fixupSegments(2);
    TieSegment* segment = segmentAt(1);
    segment->setSystem(system);

    qreal x = system->firstNoteRestSegmentX(true);

    segment->adjustY(PointF(x, sPos.p2.y()), sPos.p2);
    segment->setSpannerSegmentType(SpannerSegmentType::END);
    segment->adjustX();
    segment->finalizeSegment();
    return segment;
}

//---------------------------------------------------------
//   setStartNote
//---------------------------------------------------------

void Tie::setStartNote(Note* note)
{
    setStartElement(note);
    setParent(note);
}

//---------------------------------------------------------
//   startNote
//---------------------------------------------------------

Note* Tie::startNote() const
{
    Q_ASSERT(!startElement() || startElement()->type() == ElementType::NOTE);
    return toNote(startElement());
}

//---------------------------------------------------------
//   endNote
//---------------------------------------------------------

Note* Tie::endNote() const
{
    return toNote(endElement());
}

bool Tie::isConnectingEqualArticulations() const
{
    if (!startNote() || !endNote()) {
        return false;
    }

    const Chord* firstChord = startNote()->chord();
    const Chord* lastChord = endNote()->chord();

    if (!firstChord || !lastChord) {
        return false;
    }

    return firstChord->containsEqualArticulations(lastChord)
           && firstChord->containsEqualTremolo(lastChord);
}
}
