import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
 id:root
 objectName:"activityRow"
 required property var row
 required property var feed
 property bool opened:!!feed.openRows[String(row.id)]
 property var cached:feed.detailCache[String(row.id)]||({})
 property var detail:cached.detail||({})
 property string body:cached.body||""
 spacing:6
 function toggle(){var opening=!opened;var next=Object.assign({},feed.openRows);next[String(row.id)]=!opened;feed.openRows=next;if(opening&&body.length===0)load(0)}
 function load(offset){var nextDetail=backend.activityDetail(row.id,offset);var next=Object.assign({},feed.detailCache);next[String(row.id)]={detail:nextDetail,body:offset>0?body+(nextDetail.content||""):nextDetail.content||nextDetail.error||""};feed.detailCache=next}
 Component.onCompleted:if(opened&&body.length===0)load(0)
 function kindLabel(){var kind=row.kind||"";if(kind.indexOf("context.")===0)return qsTr("Loaded context");if(kind.indexOf("tool.before")===0)return qsTr("Before tool");if(kind.indexOf("tool.")===0)return qsTr("Tool result");if(kind.indexOf("agent.")===0)return qsTr("Agent");if(kind.indexOf("task.")===0)return qsTr("Task");if(kind.indexOf("plan")>=0)return qsTr("Plan");if(kind.indexOf("bash")>=0||kind.indexOf("command")>=0)return qsTr("Bash / command");if(kind==="chat.user")return qsTr("Prompt");if(kind.indexOf("chat.")===0)return qsTr("Assistant");return qsTr("Session activity")}
 Button {
  Layout.fillWidth:true;flat:true;objectName:"activityDisclosure"
  contentItem:RowLayout {
   spacing:8
   Label {text:root.opened?"⌄":">";color:theme.accent;font.pixelSize:18}
   Label {text:root.kindLabel();color:theme.accent;font.bold:true}
   Label {text:(root.row.name||"")+" · "+(root.row.summary||"");textFormat:Text.PlainText;elide:Text.ElideRight;Layout.fillWidth:true}
   Label {text:root.row.origin&&root.row.origin.mark?root.row.origin.mark:"#"+String(root.row.id);textFormat:Text.PlainText;font.pixelSize:10;color:root.row.origin&&root.row.origin.color?root.row.origin.color:"#9198a6"}
  }
  onClicked:root.toggle()
 }
 ColumnLayout {
  visible:root.opened;Layout.fillWidth:true;Layout.leftMargin:22
  Label {text:(root.row.origin&&root.row.origin.route?root.row.origin.route:qsTr("Workspace activity"))+" · "+(root.row.created||"");textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
  Label {text:root.row.source||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6"}
  ScrollView {Layout.fillWidth:true;Layout.preferredHeight:Math.min(360,Math.max(60,payload.contentHeight+24))
   TextArea {id:payload;text:root.body;textFormat:TextEdit.PlainText;readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap}
  }
  Button {visible:root.detail.nextOffset!==undefined&&root.detail.nextOffset>=0;text:qsTr("↓ More detail");onClicked:root.load(root.detail.nextOffset)}
 }
 Connections {target:backend;function onChanged(){if(root.opened&&root.row.id<0)root.load(0)}}
}
