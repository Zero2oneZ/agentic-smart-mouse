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
  // Build task threads from the scoped ledger, without changing stored history.
  var groups=[],lookup={},active={}
  for(var row of rows.slice().reverse()){
   var category=String(row.category), kind=row.kind||"", key
   if(kind==="chat.user"){
    key="post:"+row.id
    active[category]=key
   }else if(kind.indexOf("chat.")===0){
    key=active[category]||("earlier:"+category)
   }else{
    key=category+":"+(row.source||row.session||row.id)
   }
   if(!lookup[key]){
    lookup[key]={group:true,key:key,name:row.name,session:row.session,items:[],title:row.summary||qsTr("Earlier conversation"),post:kind.indexOf("chat.")===0||kind==="agent.prompt"}
    groups.push(lookup[key])
   }
   lookup[key].items.push(row)
  }
  groups.reverse()
  model=groups
  Qt.callLater(function(){if(root.followTail)root.positionViewAtBeginning();else root.contentY=Math.max(root.originY,Math.min(oldY,root.originY+root.contentHeight-root.height))})
 }
 onScopeChanged:{pageCount=1;detailCache={};openRows={};openSessions={};followTail=true;refresh()}
 onMovementEnded:followTail=atYBeginning
 Component.onCompleted:refresh()
 Connections {target:backend;function onActivityChanged(){root.refresh()}function onBusyChanged(){root.refresh()}function onChanged(){if(root.scope===backend.currentId)root.refresh()}}
 ScrollBar.vertical:ScrollBar {}
 header:Label {width:root.width;text:root.rows.length?qsTr("Task posts · newest first"):qsTr("Post a task to start this project’s conversation.");padding:10;wrapMode:Text.Wrap;color:"#9198a6"}
 delegate:Rectangle {
  id:node
  required property var modelData
  width:root.width
  height:thread.implicitHeight+24;color:"#1a2029";radius:10;border.color:"#303a48"
  ColumnLayout {
   id:thread;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:6
  Label {Layout.fillWidth:true;text:node.modelData.title;textFormat:Text.PlainText;font.pixelSize:17;font.bold:true;wrapMode:Text.Wrap;maximumLineCount:3;elide:Text.ElideRight}
  Label {text:(node.modelData.name||qsTr("Project"))+" · "+(node.modelData.post?qsTr("Task discussion"):qsTr("Activity segment"));textFormat:Text.PlainText;color:backend.feedIdentity.color;font.pixelSize:11}
  Button {objectName:"sessionDisclosure";visible:node.modelData.group&&!node.modelData.post;Layout.fillWidth:true;flat:true;contentItem:Label {textFormat:Text.PlainText;elide:Text.ElideRight;text:(root.openSessions[node.modelData.key]?"⌄  ":">  ")+(node.modelData.name||"")+" · "+qsTr("Session")+" "+String(node.modelData.session||"").slice(0,8)+" · "+(node.modelData.items||[]).length}
   onClicked:{var next=Object.assign({},root.openSessions);next[node.modelData.key]=!next[node.modelData.key];root.openSessions=next}
  }
  Repeater {model:node.modelData.post?node.modelData.items:(root.openSessions[node.modelData.key]?node.modelData.items:[])
   delegate:ActivityRow {required property var modelData;row:modelData;feed:root;Layout.fillWidth:true}
  }
 }
 }
 footer:Button {text:qsTr("↓ More activity");visible:root.more;width:root.width;onClicked:{root.followTail=false;root.pageCount++;root.refresh()}}
}
