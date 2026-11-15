import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material

Window {
 id:overlay
 property var owner
 property real anchorX:width/2
 property real anchorY:height/2
 property int currentParent:0
 property var stack:[]
 property var contextItem:({})
 property var entries:[]
 property real spread:backend.expanded?1:0
 width:1000;height:760;visible:false;color:"transparent"
 flags:Qt.FramelessWindowHint|Qt.WindowStaysOnTopHint|Qt.Tool
 Material.theme:Material.Dark;Material.accent:theme.accent;Material.roundedScale:theme.radius
 function resetMenu(){currentParent=0;stack=[];contextItem={};rebuild()}
 function rebuild(){
  var items=[]
  if(currentParent===0)items=[{name:qsTr("New task"),glyph:"+",action:"task"},{name:qsTr("Activity"),glyph:"◷",action:"activity"},{name:qsTr("Folder"),glyph:"▱",action:"folder"},{name:qsTr("Tools"),glyph:"⚒",action:"tools"},{name:qsTr("Browser"),glyph:"↗",action:"browser"}]
  else if(currentParent===-1)items=[{name:qsTr("Terminal"),glyph:">_",action:"terminal"},{name:qsTr("Open folder"),glyph:"▱",action:"folder"},{name:qsTr("Keybindings"),glyph:"⌨",action:"keybindings"}]
  else items=[{name:qsTr("Open chat"),glyph:"◎",action:"chat",id:currentParent},{name:qsTr("Open folder"),glyph:"▱",action:"folder",id:currentParent}]
  var children=currentParent<0?[]:backend.children(currentParent)
  for(var i=0;i<children.length;i++)items.push({name:children[i].name,glyph:"▸",action:"category",id:children[i].id})
  items.push({name:qsTr("Add category"),glyph:"+",action:"add"})
  entries=items
 }
 function choose(item){
  if(item.action==="tools"||item.action==="category"){stack.push(currentParent);currentParent=item.action==="tools"?-1:item.id;rebuild();return}
  if(item.action==="add"){backend.release();owner.reveal();owner.editCategory(-1,Math.max(0,currentParent));return}
  backend.launch(item.action,item.id===undefined?backend.currentId:item.id)
 }
 Behavior on spread {NumberAnimation {duration:150;easing.type:Easing.OutCubic}}
 Timer {id:hideDelay;interval:170;onTriggered:if(!backend.expanded)overlay.hide()}
 Connections {
  target:backend
  function onExpandedChanged(){if(!backend.expanded){contextItem={};hideDelay.restart()}else hideDelay.stop()}
  function onChanged(){overlay.rebuild()}
 }
 MouseArea {anchors.fill:parent;onClicked:backend.release();acceptedButtons:Qt.LeftButton|Qt.RightButton}
 Rectangle {anchors.fill:parent;color:"#3907090e";opacity:overlay.spread}
 Item {
  id:menuRoot;anchors.fill:parent
  property real radiusX:Math.max(205,Math.ceil(overlay.entries.length/8)*155)
  property real radiusY:Math.max(166,Math.ceil(overlay.entries.length/10)*125)
  property real cx:Math.max(radiusX+84,Math.min(width-radiusX-84,overlay.anchorX))
  property real cy:Math.max(radiusY+58,Math.min(height-radiusY-58,overlay.anchorY))
  Repeater {
   model:overlay.entries
   delegate:Rectangle {
    required property var modelData
    required property int index
    property real angle:(index/overlay.entries.length)*Math.PI*2-Math.PI/2
    property real desiredX:menuRoot.cx+Math.cos(angle)*menuRoot.radiusX
    property real desiredY:menuRoot.cy+Math.sin(angle)*menuRoot.radiusY
    // Translate the ring inward at edges while retaining the pointer as the origin.
    property real shiftX:Math.max(0,82-(menuRoot.cx-menuRoot.radiusX))-Math.max(0,(menuRoot.cx+menuRoot.radiusX)-(menuRoot.width-82))
    property real shiftY:Math.max(0,52-(menuRoot.cy-menuRoot.radiusY))-Math.max(0,(menuRoot.cy+menuRoot.radiusY)-(menuRoot.height-52))
    x:overlay.anchorX+(desiredX+shiftX-overlay.anchorX)*overlay.spread-width/2
    y:overlay.anchorY+(desiredY+shiftY-overlay.anchorY)*overlay.spread-height/2
    width:148;height:80;radius:theme.radius;opacity:overlay.spread
    color:choiceMouse.containsMouse?"#303947":"#f0191e27";border.color:choiceMouse.containsMouse?theme.accent:"#4a5262";border.width:1
    scale:choiceMouse.containsMouse?1.05:1
    Behavior on scale {NumberAnimation {duration:100}}
    Column {anchors.centerIn:parent;spacing:5;width:parent.width-14
     Label {text:modelData.glyph;color:theme.accent;font.pixelSize:25;anchors.horizontalCenter:parent.horizontalCenter}
     Label {text:modelData.name;color:"#f1f3f7";font.pixelSize:12;width:parent.width;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideRight}
    }
    MouseArea {id:choiceMouse;anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.LeftButton|Qt.RightButton;onClicked:function(mouse){if(mouse.button===Qt.RightButton){overlay.contextItem=modelData;contextPanel.x=Math.max(8,Math.min(overlay.width-contextPanel.width-8,parent.x+parent.width/2));contextPanel.y=Math.max(8,Math.min(overlay.height-contextPanel.height-8,parent.y+20))}else overlay.choose(modelData)}}
   }
  }
  Rectangle {
   x:overlay.anchorX+(menuRoot.cx-overlay.anchorX)*overlay.spread-width/2;y:overlay.anchorY+(menuRoot.cy-overlay.anchorY)*overlay.spread-height/2;width:104;height:90;radius:theme.radius;opacity:overlay.spread;color:"#f011151c";border.width:2;border.color:theme.accent
   Column {anchors.centerIn:parent;spacing:5
    Label {text:overlay.currentParent===0?"◎":"‹";font.pixelSize:29;color:theme.accent;anchors.horizontalCenter:parent.horizontalCenter}
    Label {text:overlay.currentParent===0?qsTr("MASTER CHAT"):qsTr("BACK");font.pixelSize:9;font.letterSpacing:0.6;color:"#b8c0ce";anchors.horizontalCenter:parent.horizontalCenter}
   }
   MouseArea {anchors.fill:parent;onDoubleClicked:backend.openChat(1);onClicked:{if(overlay.currentParent!==0){overlay.currentParent=overlay.stack.length?overlay.stack.pop():0;overlay.rebuild()}}}
  }
  Rectangle {
   visible:overlay.currentParent===0;x:Math.max(8,Math.min(parent.width-width-8,menuRoot.cx-width/2));y:Math.max(8,Math.min(parent.height-height-8,menuRoot.cy+menuRoot.radiusY+58));width:270;height:30;radius:theme.radius;color:"#dd11151c";opacity:overlay.spread
   Label {anchors.centerIn:parent;text:qsTr("Ctrl + Super  ·  release to close");color:"#b8c0ce";font.pixelSize:11}
  }
 }
 Rectangle {
  id:contextPanel;visible:overlay.contextItem.name!==undefined&&backend.expanded;width:220;height:contextColumn.implicitHeight+16;color:"#252c37";border.color:theme.accent;radius:theme.radius;z:10
  Column {id:contextColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:8;spacing:3
   Label {text:overlay.contextItem.name||"";font.bold:true;padding:8}
   Repeater {
    model:overlay.contextItem.id!==undefined?[qsTr("Open chat"),qsTr("Open folder"),qsTr("Edit specs and memory"),qsTr("Add child"),qsTr("Archive")]:[qsTr("Open"),qsTr("Add category here")]
    delegate:Button {
     required property string modelData
     required property int index
     width:contextColumn.width;text:modelData;flat:true
     onClicked:{var item=overlay.contextItem;overlay.contextItem={};if(item.id!==undefined){if(index===0)backend.openChat(item.id);else if(index===1)backend.launch("folder",item.id);else if(index===2){backend.release();owner.reveal();owner.editCategory(item.id,0)}else if(index===3){backend.release();owner.reveal();owner.editCategory(-1,item.id)}else backend.archive(item.id)}else if(index===0)overlay.choose(item);else{backend.release();owner.reveal();owner.editCategory(-1,Math.max(0,overlay.currentParent))}}
    }
   }
  }
 }
}
