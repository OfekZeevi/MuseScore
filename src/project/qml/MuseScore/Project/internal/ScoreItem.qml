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
import QtQuick 2.15
import QtGraphicalEffects 1.0

import MuseScore.Ui 1.0
import MuseScore.UiComponents 1.0
import MuseScore.Project 1.0

FocusScope {
    id: root

    property string name: ""
    property string suffix: ""
    property alias timeSinceModified: timeSinceModified.text
    property var thumbnail: null
    property bool isAdd: false

    property alias navigation: navCtrl

    signal clicked()

    NavigationControl {
        id: navCtrl
        name: root.name
        enabled: root.enabled && root.visible

        accessible.role: MUAccessible.Button
        accessible.name: root.name

        onActiveChanged: function(active) {
            if (active) {
                root.forceActiveFocus()
            }
        }

        onTriggered: root.clicked()
    }

    Column {
        anchors.fill: parent

        spacing: 16

        Item {
            id: scoreRect

            height: 224
            width: 172

            opacity: 0.9

            property int borderWidth: 0
            readonly property int radius: 3

            Loader {
                id: loader

                anchors.fill: parent

                sourceComponent: {
                    if (root.isAdd) {
                        return addComp
                    }

                    if (root.thumbnail) {
                        return scoreThumbnailComp
                    }

                    return genericThumbnailComp
                }

                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: scoreRect.width
                        height: scoreRect.height
                        radius: scoreRect.radius
                    }
                }
            }

            Rectangle {
                anchors.fill: parent

                color: "transparent"
                radius: parent.radius

                NavigationFocusBorder {
                    navigationCtrl: navCtrl

                    //! NOTE: the contrast between the navigation focus border and the thumbnail
                    //! is very low, especially in dark mode. Add a small padding between the
                    //! focus border and the thumbnail, to make the border easily visible.
                    anchors.margins: -border.width - 2
                }

                border.color: ui.theme.strokeColor
                border.width: parent.borderWidth
            }

            states: [
                State {
                    name: "NORMAL"
                    when: !mouseArea.containsMouse && !mouseArea.pressed

                    PropertyChanges {
                        target: scoreRect
                        borderWidth: ui.theme.borderWidth
                    }
                },

                State {
                    name: "HOVERED"
                    when: mouseArea.containsMouse && !mouseArea.pressed

                    PropertyChanges {
                        target: scoreRect
                        opacity: 1
                        borderWidth: 1
                    }
                },

                State {
                    name: "PRESSED"
                    when: mouseArea.pressed

                    PropertyChanges {
                        target: scoreRect
                        opacity: 0.5
                    }
                }
            ]

            RectangularGlow {
                anchors.fill: scoreRect
                z: -1

                glowRadius: 20
                color: "#08000000"
                cornerRadius: scoreRect.radius + glowRadius
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 4

            StyledTextLabel {
                anchors.horizontalCenter: parent.horizontalCenter

                text: root.name

                wrapMode: Text.WrapAnywhere
                maximumLineCount: 1
                width: parent.width

                font: ui.theme.largeBodyFont
            }

            StyledTextLabel {
                id: timeSinceModified

                anchors.horizontalCenter: parent.horizontalCenter

                font.capitalization: Font.AllUppercase

                visible: !root.isAdd
            }
        }
    }

    Component {
        id: addComp

        Rectangle {
            anchors.fill: parent
            color: "white"

            StyledIconLabel {
                anchors.centerIn: parent

                iconCode: IconCode.PLUS

                font.pixelSize: 50
                color: "black"
            }
        }
    }

    Component {
        id: scoreThumbnailComp

        ScoreThumbnail {
            anchors.fill: parent
            thumbnail: root.thumbnail
        }
    }

    Component {
        id: genericThumbnailComp

        Rectangle {
            anchors.fill: parent
            color: "white"

            Image {
                anchors.centerIn: parent

                width: 80
                height: 110

                source: {
                    switch (root.suffix) {
                    case "mid":
                    case "midi":
                    case "kar":
                        return "qrc:/resources/Placeholder_MIDI.png"
                    case "mxl":
                    case "musicxml":
                    case "xml":
                        return "qrc:/resources/Placeholder_MXML.png"
                    default:
                        return "qrc:/resources/Placeholder_Other.png"
                    }
                }

                fillMode: Image.PreserveAspectFit
            }
        }
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            root.clicked()
        }
    }
}
