pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons as Commons
import "TaskModel.js" as TaskModel

Item {
 id:root
 property var shell:null
 property var manifest:null
 property var jobs:[]
 property string menuJob:""
 onJobsChanged:if(!jobs.some(function(job){return job.id===root.menuJob}))menuJob=""
 property double now:Date.now()
 readonly property string home:Quickshell.env("HOME")
 Commons.Translations {id:strings;directory:Qt.resolvedUrl("i18n")}
 FileView {
  id:statusFile
  path:(Quickshell.env("XDG_DATA_HOME")||root.home+"/.local/share")+"/hotbox/agent-runs/status.json"
  watchChanges:true;printErrors:false
  onLoaded:root.jobs=TaskModel.parse(text())
  onFileChanged:reload()
  onLoadFailed:root.jobs=[]
 }
 Timer {interval:2000;running:true;repeat:true;onTriggered:{root.now=Date.now();statusFile.reload()}}
 function launch(id,view){var args=TaskModel.command(root.home,id,view);if(args)Quickshell.execDetached(args);root.menuJob=""}
 function elapsed(created){var seconds=Math.max(0,Math.floor((root.now-Date.parse(created))/1000));return isFinite(seconds)?Commons.I18n.formatNumber(Math.floor(seconds/60))+"m":""}
 Variants {
  model:Quickshell.screens
  delegate:PanelWindow {
   id:strip
   required property var modelData
   screen:modelData
   visible:root.jobs.length>0
   anchors {top:true;left:true;right:true}
   implicitHeight:root.menuJob.length?94:38
   exclusiveZone:38
   color:Commons.Color.background
   WlrLayershell.namespace:"hotbox-tasks"
   WlrLayershell.layer:WlrLayer.Top
   WlrLayershell.keyboardFocus:WlrKeyboardFocus.None
   ColumnLayout {
    anchors.fill:parent;anchors.margins:4;spacing:4
    LayoutMirroring.enabled:Commons.I18n.rightToLeft;LayoutMirroring.childrenInherit:true
    RowLayout {
     Layout.fillWidth:true;Layout.preferredHeight:30
     Text {text:strings.t("tasks");textFormat:Text.PlainText;color:Commons.Color.foreground;font.pixelSize:12;Layout.leftMargin:8}
     Flickable {
      Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:badges.width;contentHeight:height
      Row {
       id:badges;height:parent.height;spacing:6
       Repeater {
        model:root.jobs
        delegate:Rectangle {
         id:badge
         required property var modelData
         width:Math.min(270,label.implicitWidth+24);height:28;radius:Commons.Style.cornerRadius
         property color identity:["#9ac8fa","#a6d49c","#d7b1f1","#f0c18f","#8fd1cd"][parseInt(modelData.id.slice(0,4),16)%5]
         color:Qt.rgba(identity.r,identity.g,identity.b,0.14);border.color:identity
         SequentialAnimation on opacity {running:badge.modelData.state==="running";loops:Animation.Infinite;NumberAnimation {to:0.6;duration:900}NumberAnimation {to:1;duration:900}}
         Text {
          id:label;anchors.centerIn:parent;width:Math.min(246,implicitWidth);elide:Text.ElideRight;textFormat:Text.PlainText;color:Commons.Color.foreground;font.pixelSize:12
          text:(badge.modelData.state==="running"?"● ":badge.modelData.state==="finished"?"✓ ":"! ")+badge.modelData.title+" · "+badge.modelData.id.slice(0,4)+" · "+strings.t(badge.modelData.state==="running"?"working":badge.modelData.state)+ (badge.modelData.state==="running"?" "+root.elapsed(badge.modelData.created):"")
         }
         MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton
          onClicked:function(mouse){root.menuJob=root.menuJob===badge.modelData.id?"":badge.modelData.id}
          onDoubleClicked:root.launch(badge.modelData.id,"chat")
         }
        }
       }
      }
     }
    }
    RowLayout {
     visible:root.menuJob.length>0;Layout.fillWidth:true;Layout.preferredHeight:44
     Repeater {
      model:[{key:"chat",view:"chat"},{key:"terminal",view:"terminal"},{key:"prompt",view:"prompt"},{key:"close",view:""}]
      delegate:Rectangle {
       required property var modelData
       Layout.preferredWidth:150;Layout.fillHeight:true;color:Qt.rgba(1,1,1,0.08);radius:Commons.Style.cornerRadius
       Text {anchors.centerIn:parent;text:strings.t(parent.modelData.key);textFormat:Text.PlainText;color:Commons.Color.foreground;font.pixelSize:12}
       MouseArea {anchors.fill:parent;onClicked:if(parent.modelData.view.length)root.launch(root.menuJob,parent.modelData.view);else root.menuJob=""}
      }
     }
     Item {Layout.fillWidth:true}
    }
   }
  }
 }
}
