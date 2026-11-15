import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ListView {
 id:root
 objectName:"chatFeed"
 property bool followTail:true
 property var rows:[]
 property var detailCache:({})
 property var openRows:({})
 property var openSessions:({})
 property int pageCount:1
 property int scope:backend.currentId
 property bool more:true
 clip:true;spacing:8
 function goLatest(){followTail=true;positionViewAtBeginning()}
 function refresh(){
  var oldY=contentY
  var result=[],before=0
  for(var i=0;i<pageCount;i++){var page=backend.activityPage(before);result=result.concat(page);if(page.length<40){more=false;break}more=true;before=page[page.length-1].id}
  rows=result
  var groups=[],lookup={}
  for(var row of rows){if(backend.currentId===1){groups.push({group:false,key:String(row.id),row:row});continue}var key=String(row.category)+":"+(/^[a-f0-9]{32}$/.test(row.source||"")?row.source:row.session);if(!lookup[key]){lookup[key]={group:true,key:key,name:row.name,session:/^[a-f0-9]{32}$/.test(row.source||"")?row.source:row.session,items:[]};groups.push(lookup[key])}lookup[key].items.push(row)}
  model=groups
  Qt.callLater(function(){if(root.followTail)root.positionViewAtBeginning();else root.contentY=Math.max(root.originY,Math.min(oldY,root.originY+root.contentHeight-root.height))})
 }
 onScopeChanged:{pageCount=1;detailCache={};openRows={};openSessions={};followTail=true;refresh()}
 onMovementEnded:followTail=atYBeginning
 Component.onCompleted:refresh()
 Connections {target:backend;function onActivityChanged(){root.refresh()}function onBusyChanged(){root.refresh()}function onChanged(){if(root.scope===backend.currentId)root.refresh()}}
 ScrollBar.vertical:ScrollBar {}
 header:Label {width:root.width;text:backend.currentId===1?qsTr("Global activity · newest first"):qsTr("Session segments · expand to inspect");padding:10;color:"#9198a6"}
 delegate:ColumnLayout {
  id:node
  required property var modelData
  width:root.width;spacing:5
  Button {objectName:"sessionDisclosure";visible:node.modelData.group;Layout.fillWidth:true;flat:true;contentItem:Label {textFormat:Text.PlainText;elide:Text.ElideRight;text:(root.openSessions[node.modelData.key]?"⌄  ":">  ")+(node.modelData.name||"")+" · "+qsTr("Session")+" "+String(node.modelData.session||"").slice(0,8)+" · "+(node.modelData.items||[]).length}
   onClicked:{var next=Object.assign({},root.openSessions);next[node.modelData.key]=!next[node.modelData.key];root.openSessions=next}
  }
  Repeater {model:node.modelData.group?(root.openSessions[node.modelData.key]?node.modelData.items:[]):[node.modelData.row]
   delegate:ActivityRow {required property var modelData;row:modelData;feed:root;Layout.fillWidth:true}
  }
 }
 footer:Button {text:qsTr("↓ More activity");visible:root.more;width:root.width;onClicked:{root.followTail=false;root.pageCount++;root.refresh()}}
}
