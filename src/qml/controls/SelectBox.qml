import QtQuick
import QtQuick.Controls
import FolderSnap

ComboBox {
    id: control
    implicitHeight: 38
    implicitWidth: 180
    leftPadding: 12
    rightPadding: 32
    font.family: Theme.fontFamily
    font.pixelSize: 12
    Accessible.name: displayText
    contentItem: LabelText {
        text: control.displayText
        color: control.enabled ? Theme.text : Theme.muted
    }
    indicator: Glyph {
        x: control.width - 24
        y: 12
        name: "down"
        font.pixelSize: 11
    }
    background: Rectangle {
        radius: 8
        color: control.hovered ? Theme.surfaceRaised : "#192226"
        border.color: control.visualFocus ? Theme.accent : Theme.border
    }
    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: control.width
        implicitHeight: 36
        contentItem: LabelText {
            text: modelData
        }
        background: Rectangle {
            color: parent.highlighted || parent.hovered ? Theme.surfaceRaised : Theme.surface
        }
        highlighted: control.highlightedIndex === index
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        background: Panel {
            radius: 8
        }
        contentItem: ListView {
            implicitHeight: contentHeight
            clip: true
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
