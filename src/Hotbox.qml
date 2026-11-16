import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Window {
 id:overlay
 objectName:"hotboxOverlay"
 property int groupIndex:0
 property var groups:["recent","favorites","pinned"]
 property var groupNames:[qsTr("Recent"),qsTr("Favorites"),qsTr("Pinned")]
 property var positions:[7,8,9,3,2,1]
 property int selectedIndex:-1
 property int pageIndex:0
 property bool centerSelected:false
 property string entryMode:""
 property var agentTarget:({})
 property string previousAgent:""
 property bool recording:backend.recorder.current.state==="recording"||backend.recorder.current.state==="paused"
 Shortcut {sequence:"Escape";context:Qt.WindowShortcut;enabled:overlay.visible&&(overlay.entryMode==="deck"||overlay.entryMode==="browser"||overlay.contextItem.name!==undefined);onActivated:{overlay.entryMode="";overlay.contextItem={};keyboard.forceActiveFocus()}}
 property var owner
 property real anchorX:width/2
 property real anchorY:height/2
 property int currentParent:0
 property var toolRoute:({kind:"none"})
 property var stack:[]
 property var contextItem:({})
 property var entries:[]
 property real spread:backend.expanded?1:0
 width:1000;height:760;visible:false;color:"transparent"
 flags:Qt.FramelessWindowHint|Qt.WindowStaysOnTopHint|Qt.Tool
 Material.theme:Material.Dark;Material.accent:theme.accent;Material.roundedScale:theme.radius
 function resetMenu(){currentParent=0;toolRoute={kind:"none"};stack=[];contextItem={};selectedIndex=-1;pageIndex=0;centerSelected=false;entryMode="";rebuild();keyboard.forceActiveFocus()}
 function rebuild(){
  var items=[]
  if(currentParent===0)items=[{name:qsTr("New task"),glyph:"+",action:"task"},{name:qsTr("Activity"),glyph:"◷",action:"activity"},{name:qsTr("Folder"),glyph:"▱",action:"folder"},{name:qsTr("Tools"),glyph:"⚒",action:"tools"},{name:qsTr("Browser"),glyph:"↗",action:"browser"}]
  else if(currentParent===-1){
   if(toolRoute.kind==="tool_category"){
    for(var tool of backend.registeredTools)if(tool.menu_category===toolRoute.category)items.push(tool)
    items.push({name:qsTr("+ New tool"),glyph:"+",action:"tool_create",menu_category:toolRoute.category})
   }else{
    var toolGroups=Object.create(null)
    for(var tool of backend.registeredTools){var group=tool.menu_category||qsTr("Pending");if(!toolGroups[group]){toolGroups[group]=true;items.push({name:group,glyph:"▸",action:"tool_group",menu_category:group})}}
    items.push({name:qsTr("+ New tool"),glyph:"+",action:"tool_create"},{name:qsTr("Manage tools"),glyph:"⚒",action:"tool_manage"},{name:qsTr("Terminal"),glyph:">_",action:"terminal"},{name:qsTr("Open folder"),glyph:"▱",action:"folder"},{name:qsTr("Keybindings"),glyph:"⌨",action:"keybindings"})
   }
  }
  else items=[{name:qsTr("Open chat"),glyph:"◎",action:"chat",id:currentParent},{name:qsTr("Open folder"),glyph:"▱",action:"folder",id:currentParent}]
  var children=currentParent<0?[]:backend.children(currentParent)
  for(var i=0;i<children.length;i++)items.push({name:children[i].name,glyph:"▸",action:"category",id:children[i].id})
  if(currentParent!==-1)items.push({name:qsTr("Add category"),glyph:"+",action:"add"})
  if(currentParent===0){
   var saved=backend.wheelItems(groups[groupIndex])
   if(groupIndex===0){for(var item of items){var found=false;for(var prior of saved)if(prior.action===item.action&&(prior.id||backend.currentId)===(item.id||backend.currentId))found=true;if(!found)saved.push(item)}}
   items=saved
  }
  entries=items;pageIndex=Math.min(pageIndex,Math.max(0,Math.ceil(items.length/6)-1));selectedIndex=-1;centerSelected=false
 }
 function identityFor(item,action,remember){
  var path=[groupNames[groupIndex]];var ids=stack.slice();if(currentParent!==0)ids.push(currentParent)
  for(var id of ids){if(id===-1)path.push(qsTr("Tools"));else for(var category of backend.categories)if(category.id===id)path.push(category.name)}
  if(item.name)path.push(item.name)
  path.push(action)
  return backend.identifyFeed(item.id||backend.currentId,path,groups[groupIndex],action,remember)
 }
 function refreshTools(){
  var selected=selectedIndex>=0?entries[selectedIndex]:null;var centered=centerSelected
  rebuild()
  if(selected)for(var i=0;i<entries.length;i++){var item=entries[i];if(item.action===selected.action&&item.tool_id===selected.tool_id&&item.id===selected.id&&item.menu_category===selected.menu_category){selectedIndex=i;pageIndex=Math.floor(i/6);break}}
  centerSelected=centered
 }
 function choose(item){
  if(item.action==="tool_group"){backend.latchMenu();toolRoute={kind:"tool_category",category:item.menu_category};pageIndex=0;rebuild();return}
  if(item.action==="tool_create"||item.action==="tool_manage"){backend.closeMenu();owner.openTools(item.menu_category||"",item.action==="tool_create");return}
  if(item.action==="registered_tool"){backend.latchMenu();backend.touchToolWheel(item.tool_id);contextItem=item;contextPanel.x=Math.max(8,Math.min(width-contextPanel.width-8,menuRoot.cx+65));contextPanel.y=Math.max(8,Math.min(height-contextPanel.height-8,menuRoot.cy-60));return}
  if(item.action==="tools"||item.action==="category"){backend.latchMenu();backend.touchWheel(item.action,item.id||backend.currentId);pageIndex=0;stack.push(currentParent);currentParent=item.action==="tools"?-1:item.id;if(item.action==="tools")toolRoute={kind:"tools"};rebuild();if(item.action==="tools")backend.refreshToolRegistry();return}
  if(item.action==="add"){backend.closeMenu();owner.reveal();owner.editCategory(-1,Math.max(0,currentParent));return}
  if(item.action==="chat")identityFor(item,qsTr("Chat"),true)
  backend.launch(item.action,item.id===undefined?backend.currentId:item.id)
 }
 Behavior on spread {NumberAnimation {duration:150;easing.type:Easing.OutCubic}}
 Timer {id:hideDelay;interval:170;onTriggered:if(!backend.expanded)overlay.hide()}
 Connections {
  target:backend
  function onExpandedChanged(){if(!backend.expanded){contextItem={};entryMode="";hideDelay.restart()}else hideDelay.stop()}
  function onChanged(){overlay.rebuild()}
  function onToolsChanged(){overlay.refreshTools()}
 }
 MouseArea {anchors.fill:parent;onClicked:backend.closeMenu();acceptedButtons:Qt.LeftButton|Qt.RightButton}
 Rectangle {anchors.fill:parent;color:"#3907090e";opacity:overlay.spread}
 Item {
  id:menuRoot;anchors.fill:parent
  property real pulse:1
  scale:pulse
  SequentialAnimation {id:wheelAnimation;NumberAnimation {target:menuRoot;property:"pulse";to:0.94;duration:60}NumberAnimation {target:menuRoot;property:"pulse";to:1;duration:170;easing.type:Easing.OutBack}}
  property real radiusX:Math.min(230,Math.max(160,(width-190)/2))
  property real radiusY:Math.min(185,Math.max(130,(height-260)/2))
  property real cx:Math.max(radiusX+84,Math.min(width-radiusX-84,overlay.anchorX))
  property real cy:Math.max(radiusY+100,Math.min(height-radiusY-100,overlay.anchorY))
  Repeater {
   model:Math.min(6,Math.max(0,overlay.entries.length-overlay.pageIndex*6))
   delegate:Rectangle {
    required property int index
    property int entryIndex:overlay.pageIndex*6+index
    property var modelData:overlay.entries[entryIndex]||({})
    property bool selected:overlay.selectedIndex===entryIndex
    property real angle:[-135,-90,-45,45,90,135][index]*Math.PI/180
    property real desiredX:menuRoot.cx+Math.cos(angle)*menuRoot.radiusX
    property real desiredY:menuRoot.cy+Math.sin(angle)*menuRoot.radiusY
    // Translate the ring inward at edges while retaining the pointer as the origin.
    property real shiftX:Math.max(0,82-(menuRoot.cx-menuRoot.radiusX))-Math.max(0,(menuRoot.cx+menuRoot.radiusX)-(menuRoot.width-82))
    property real shiftY:Math.max(0,52-(menuRoot.cy-menuRoot.radiusY))-Math.max(0,(menuRoot.cy+menuRoot.radiusY)-(menuRoot.height-52))
    x:overlay.anchorX+(desiredX+shiftX-overlay.anchorX)*overlay.spread-width/2
    y:overlay.anchorY+(desiredY+shiftY-overlay.anchorY)*overlay.spread-height/2
    width:148;height:80;radius:theme.radius;opacity:overlay.spread
    color:selected||choiceMouse.containsMouse?"#303947":"#f0191e27";border.color:selected||choiceMouse.containsMouse?theme.accent:"#4a5262";border.width:1
    scale:selected||choiceMouse.containsMouse?1.05:1
    Behavior on scale {NumberAnimation {duration:100}}
    Label {anchors.right:parent.right;anchors.top:parent.top;anchors.margins:5;text:String(overlay.positions[index]);color:theme.accent;font.pixelSize:11}
    Column {anchors.centerIn:parent;spacing:5;width:parent.width-14
     Label {text:modelData.glyph;color:theme.accent;font.pixelSize:25;anchors.horizontalCenter:parent.horizontalCenter}
     Label {text:modelData.name;color:"#f1f3f7";font.pixelSize:12;width:parent.width;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideRight}
    }
    MouseArea {id:choiceMouse;anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.LeftButton|Qt.RightButton;onClicked:function(mouse){if(mouse.button===Qt.RightButton){backend.latchMenu();overlay.contextItem=modelData;contextPanel.x=Math.max(8,Math.min(overlay.width-contextPanel.width-8,parent.x+parent.width/2));contextPanel.y=Math.max(8,Math.min(overlay.height-contextPanel.height-8,parent.y+20))}else overlay.choose(modelData)}}
   }
  }
  Rectangle {
   x:overlay.anchorX+(menuRoot.cx-overlay.anchorX)*overlay.spread-width/2;y:overlay.anchorY+(menuRoot.cy-overlay.anchorY)*overlay.spread-height/2;width:104;height:90;radius:theme.radius;opacity:overlay.spread;color:overlay.centerSelected?"#344150":"#f011151c";border.width:2;border.color:theme.accent
   Column {anchors.centerIn:parent;spacing:5
    Label {text:overlay.currentParent===0?"◎":"‹";font.pixelSize:29;color:theme.accent;anchors.horizontalCenter:parent.horizontalCenter}
    Label {text:overlay.currentParent===0?qsTr("5  MASTER CHAT"):qsTr("5  BACK");font.pixelSize:9;font.letterSpacing:0.6;color:"#b8c0ce";anchors.horizontalCenter:parent.horizontalCenter}
   }
   MouseArea {anchors.fill:parent;onDoubleClicked:{overlay.identityFor({name:qsTr("Master chat"),id:1},qsTr("Chat"),true);backend.openChat(1)}
    onClicked:overlay.goBack()}
  }
  Rectangle {
   visible:overlay.currentParent===0;x:Math.max(8,Math.min(parent.width-width-8,menuRoot.cx-width/2));y:Math.max(8,Math.min(parent.height-height-8,menuRoot.cy+menuRoot.radiusY+58));width:330;height:30;radius:theme.radius;color:"#dd11151c";opacity:overlay.spread
   MouseArea {anchors.fill:parent;onClicked:overlay.advancePage()}
   Label {anchors.centerIn:parent;text:qsTr("0 Run · . Options · More (%1/%2)").arg(overlay.pageIndex+1).arg(Math.max(1,Math.ceil(overlay.entries.length/6)));color:"#b8c0ce";font.pixelSize:11}
  }
 }
 Label {
  x:menuRoot.cx-width/2;y:menuRoot.cy+menuRoot.radiusY+120;width:450
  text:backend.recorder.problem||backend.status;textFormat:Text.PlainText;wrapMode:Text.Wrap
  visible:overlay.entryMode.length===0&&text.length>0;opacity:overlay.spread;color:"#f1b593";font.pixelSize:11;horizontalAlignment:Text.AlignHCenter
 }
 function mark(group){
  var item=contextItem;var id=item.id===undefined?backend.currentId:item.id;var enabled=true
  if(item.action==="registered_tool"){for(var saved of backend.wheelItems(group))if(saved.tool_id===item.tool_id)enabled=false;backend.markToolWheel(item.tool_id,group,enabled);contextItem={};return}
  for(var saved of backend.wheelItems(group))if(saved.action===item.action&&saved.id===id)enabled=false
  backend.markWheel(item.action,id,group,enabled);contextItem={}
 }
 function goBack(){if(currentParent===-1&&toolRoute.kind==="tool_category"){toolRoute={kind:"tools"}}else if(currentParent!==0){currentParent=stack.length?stack.pop():0;toolRoute={kind:"none"}}pageIndex=0;rebuild()}
 function changeGroup(delta){backend.latchMenu();groupIndex=(groupIndex+delta+3)%3;currentParent=0;toolRoute={kind:"none"};stack=[];pageIndex=0;contextItem={};rebuild();wheelAnimation.restart()}
 function openEntry(mode){backend.latchMenu();entryMode=mode;note.text="";note.forceActiveFocus()}
 function openAgentPrompt(){
  var job=backend.agents.selected;agentTarget=job.target||{};previousAgent=job.id||"";agentProvider.currentIndex=agentTarget.modelRole==="builder"?2:agentTarget.modelRole==="vision"?3:job.provider==="claude"?1:0;openEntry("agent")
 }
 function selectedTarget(){
  if(centerSelected){var name=qsTr("Master chat");for(var category of backend.categories)if(category.id===currentParent)name=category.name;return {name:name,action:"chat",id:currentParent>0?currentParent:1}}
  return selectedIndex>=0&&selectedIndex<entries.length?entries[selectedIndex]:null
 }
 function targetOptions(){
  var item=selectedTarget();if(!item)return
  contextItem=item;contextPanel.x=Math.max(8,Math.min(width-contextPanel.width-8,menuRoot.cx+65));contextPanel.y=Math.max(8,Math.min(height-contextPanel.height-8,menuRoot.cy-60));Qt.callLater(function(){favoriteOption.forceActiveFocus()})
 }
 function advancePage(){pageIndex=(pageIndex+1)%Math.max(1,Math.ceil(entries.length/6));selectedIndex=-1;centerSelected=false;wheelAnimation.restart()}
 function confirmEntry(){
  if(!note.text.trim().length)return
  if(entryMode==="agent"){
   if(!previousAgent.length){delete agentTarget.modelRole;if(agentProvider.currentIndex>=2)agentTarget.modelRole=agentProvider.currentIndex===2?"builder":"vision"}
   if(backend.agents.start(note.text,agentTarget,agentProvider.currentIndex===0?"codex":"claude",previousAgent).length)backend.closeMenu()
   return
  }
  if(entryMode==="note"){if(!backend.recorder.addNote(note.text))return;entryMode="";keyboard.forceActiveFocus()}
  else if(backend.prepareDeck(note.text,false))showDeck()
 }
 function showDeck(){entryMode="deck";Qt.callLater(function(){deckTerminal.forceActiveFocus()})}
 function finishRecording(){backend.latchMenu();backend.recorder.stop();if(backend.prepareDeck("",true))showDeck()}
 function dispatch(key){
  if(entryMode.length)return
  if(key==="escape"){if(contextItem.name!==undefined){contextItem={};keyboard.forceActiveFocus()}else backend.closeMenu();return}
  backend.latchMenu()
  if(key==="4"||key==="6"){changeGroup(key==="4"?-1:1);return}
  if(key==="page"){advancePage();return}
  if(key==="."){targetOptions();return}
  if(key==="0"){var item=selectedTarget();if(item&&item.action==="registered_tool"){choose(item);return}if(item){agentTarget=Object.assign({},item,{id:item.id||backend.currentId,identity:identityFor(item,qsTr("Headless agent"),false)});previousAgent="";openEntry("agent")}return}
  if(key==="/"){if(recording)backend.recorder.togglePause();else backend.recorder.start(qsTr("Mouse workflow"));return}
  if(key==="*"){entryMode="browser";return}
  if(key==="-"){finishRecording();return}
  if(key==="+"){if(backend.recorder.current.state==="paused")backend.recorder.togglePause();openEntry(recording?"note":"prompt");return}
  if(key==="enter"){if(recording)finishRecording();else openEntry("prompt");return}
  if(key==="5"){if(centerSelected){if(currentParent===0){identityFor({name:qsTr("Master chat"),id:1},qsTr("Chat"),true);backend.openChat(1)}else goBack()}else {centerSelected=true;selectedIndex=-1}return}
  var position=positions.indexOf(Number(key));var index=pageIndex*6+position
  if(position<0||index>=entries.length)return
  if(selectedIndex===index)choose(entries[index]);else {selectedIndex=index;centerSelected=false}
 }
 FocusScope {
  id:keyboard;anchors.fill:parent;focus:true
  Keys.onPressed:function(event){
   if(overlay.entryMode.length)return
   var key=""
   if(event.key===Qt.Key_Escape)key="escape"
   else if(event.key===Qt.Key_Enter||event.key===Qt.Key_Return)key="enter"
   else if((event.key===Qt.Key_PageDown||event.key===Qt.Key_PageUp)&&!(event.modifiers&Qt.KeypadModifier))key="page"
   else if(event.modifiers&Qt.KeypadModifier){
    var aliases={};aliases[Qt.Key_Left]="4";aliases[Qt.Key_Right]="6";aliases[Qt.Key_Clear]="5";aliases[Qt.Key_Up]="8";aliases[Qt.Key_Down]="2";aliases[Qt.Key_Home]="7";aliases[Qt.Key_PageUp]="9";aliases[Qt.Key_End]="1";aliases[Qt.Key_PageDown]="3";aliases[Qt.Key_Insert]="0";aliases[Qt.Key_Delete]="."
    if(aliases[event.key])key=aliases[event.key]
    else if(event.key>=Qt.Key_0&&event.key<=Qt.Key_9)key=String(event.key-Qt.Key_0)
    else if(event.key===Qt.Key_Period||event.key===Qt.Key_Comma)key="."
    else if(event.key===Qt.Key_Plus)key="+"
    else if(event.key===Qt.Key_Minus)key="-"
    else if(event.key===Qt.Key_Asterisk)key="*"
    else if(event.key===Qt.Key_Slash)key="/"
   }
   if(key.length){event.accepted=true;if(!event.isAutoRepeat)overlay.dispatch(key)}
  }
 }
 Column {
  x:menuRoot.cx-width/2;y:menuRoot.cy-menuRoot.radiusY-135;width:450;spacing:6;opacity:overlay.spread
  Row {anchors.horizontalCenter:parent.horizontalCenter;spacing:16
   Button {text:"4  ‹";onClicked:overlay.changeGroup(-1)}
   Label {text:overlay.groupNames[overlay.groupIndex];font.pixelSize:24;color:theme.accent;anchors.verticalCenter:parent.verticalCenter}
   Button {text:"›  6";onClicked:overlay.changeGroup(1)}
  }
  Label {text:qsTr("Press a direction twice to open · 4 / 6 change spread");font.pixelSize:12;color:"#d0d6e0";anchors.horizontalCenter:parent.horizontalCenter}
  Label {visible:overlay.entries.length===0;text:qsTr("Right-click a target in Recent to save it here");font.pixelSize:12;color:"#aeb9c9";anchors.horizontalCenter:parent.horizontalCenter}
 }
 Label {
  x:menuRoot.cx-width/2;y:menuRoot.cy+menuRoot.radiusY+98;opacity:overlay.spread
  text:backend.recorder.current.state==="recording"?qsTr("● Recording Hotbox · + Note · Enter Stop"):backend.recorder.current.state==="paused"?qsTr("Paused · / Continue · Enter Stop"):qsTr("/ Record · * Browser capture · + Prompt · − Starter")
  color:overlay.recording?"#f1b593":"#d0d6e0";font.pixelSize:12
 }
 Rectangle {
  anchors.fill:parent;color:"#99090c12";visible:overlay.entryMode.length>0;z:20
  MouseArea {anchors.fill:parent;onClicked:{}}
  Rectangle {
   anchors.centerIn:parent;width:Math.min(540,parent.width-32);height:entryContent.implicitHeight+36;color:"#191e27";border.color:theme.accent;radius:theme.radius
   ColumnLayout {
    id:entryContent;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:18;spacing:12
    Label {text:overlay.entryMode==="agent"?qsTr("Run for %1").arg(overlay.agentTarget.name||""):overlay.entryMode==="note"?qsTr("Explain this step"):overlay.entryMode==="deck"?qsTr("On deck"):overlay.entryMode==="browser"?qsTr("Record browser steps"):qsTr("What should we do?");font.pixelSize:20;Layout.fillWidth:true}
    Label {visible:overlay.entryMode==="note";text:{var steps=backend.recorder.current.steps||[];return steps.length?qsTr("After: %1").arg(steps[steps.length-1].label):qsTr("Before the first event")}
     textFormat:Text.PlainText;color:"#aeb9c9";wrapMode:Text.Wrap;Layout.fillWidth:true}
    ComboBox {id:agentProvider;objectName:"headlessProvider";visible:overlay.entryMode==="agent";enabled:!overlay.previousAgent.length;model:["Codex","Claude","Builder · Ollama cloud","Vision · Ollama cloud"];Layout.fillWidth:true}
    ScrollView {visible:overlay.entryMode==="note"||overlay.entryMode==="prompt"||overlay.entryMode==="agent";Layout.fillWidth:true;Layout.preferredHeight:110
     TextArea {id:note;objectName:"wheelNote";wrapMode:TextEdit.Wrap;placeholderText:overlay.entryMode==="note"?qsTr("Why this step? What happens next?"):qsTr("Describe a task for Codex or Claude…");selectByMouse:true
      Keys.onPressed:function(event){if(event.key===Qt.Key_Enter||event.key===Qt.Key_Return){event.accepted=true;if(!event.isAutoRepeat)overlay.confirmEntry()}else if(event.key===Qt.Key_Escape){event.accepted=true;overlay.entryMode="";keyboard.forceActiveFocus()}}
     }
    }
    Label {visible:overlay.entryMode==="deck";text:backend.deck.path||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Label {visible:overlay.entryMode==="deck";text:qsTr("README and prompt are ready. Choose one run; the terminal keeps the result and exit status.");wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {visible:overlay.entryMode==="browser";text:qsTr("Start capture from the Hotbox browser companion on the selected tab, then stop and export. Import its URL and click events here. This key does not start a global desktop recorder.");wrapMode:Text.Wrap;Layout.fillWidth:true}
    Button {visible:overlay.entryMode==="browser";text:qsTr("Import browser recording");enabled:!overlay.recording;onClicked:browserImport.open()}
    Label {visible:overlay.entryMode==="browser"&&overlay.recording;text:qsTr("Finish the current Hotbox recording before importing.");wrapMode:Text.Wrap;Layout.fillWidth:true}
    RowLayout {visible:overlay.entryMode==="deck";Layout.fillWidth:true
     Button {id:deckTerminal;text:qsTr("Terminal");onClicked:if(backend.openDeck())backend.closeMenu()}
     Button {text:qsTr("Codex once");onClicked:if(backend.openDeck("codex"))backend.closeMenu()}
     Button {text:qsTr("Claude once");onClicked:if(backend.openDeck("claude"))backend.closeMenu()}
    }
    Label {text:overlay.entryMode==="agent"?backend.agents.problem:backend.recorder.problem||backend.status;visible:text.length>0;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#f1b593"}
    RowLayout {
     Button {visible:overlay.entryMode==="note"||overlay.entryMode==="prompt"||overlay.entryMode==="agent";text:qsTr("Enter  Confirm");onClicked:overlay.confirmEntry()}
     Button {text:qsTr("Back to wheel");onClicked:{overlay.entryMode="";keyboard.forceActiveFocus()}}
    }
   }
  }
 }
 FileDialog {id:browserImport;title:qsTr("Choose a browser recording");nameFilters:[qsTr("Browser recordings (*.json)")];onAccepted:if(backend.recorder.importBrowser(selectedFile)){if(backend.prepareDeck("",true))overlay.showDeck()}}
 Rectangle {
  id:contextPanel;visible:overlay.contextItem.name!==undefined&&backend.expanded;width:220;height:contextColumn.implicitHeight+16;color:"#252c37";border.color:theme.accent;radius:theme.radius;z:10
  Column {id:contextColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:8;spacing:3
   Label {text:overlay.contextItem.name||"";font.bold:true;padding:8}
   Label {visible:overlay.contextItem.action==="registered_tool";text:qsTr("Draft tool · This draft has no runnable release yet.");width:contextColumn.width;wrapMode:Text.Wrap;color:"#aeb9c9";font.pixelSize:11;padding:8}
   Button {id:favoriteOption;text:qsTr("Toggle favorite");width:contextColumn.width;visible:overlay.contextItem.action!=="add";onClicked:overlay.mark("favorites")}
   Button {text:qsTr("Toggle pin");width:contextColumn.width;visible:overlay.contextItem.action!=="add";onClicked:overlay.mark("pinned")}
   Repeater {
    model:overlay.contextItem.action==="registered_tool"?[qsTr("Edit"),qsTr("Name and category"),qsTr("Run unavailable")]:overlay.contextItem.id!==undefined?[qsTr("Open chat"),qsTr("Open folder"),qsTr("Edit specs and memory"),qsTr("Add child"),qsTr("Archive")]:[qsTr("Open"),qsTr("Add category here")]
    delegate:Button {
     required property string modelData
     required property int index
     width:contextColumn.width;text:modelData;flat:true
     enabled:overlay.contextItem.action!=="registered_tool"||(index<2&&overlay.contextItem.verified)
     onClicked:{var item=overlay.contextItem;overlay.contextItem={};if(item.action==="registered_tool"){backend.closeMenu();if(index===0)backend.openToolContext(item.tool_id);else if(index===1)owner.editTool(item);return}if(item.id!==undefined){if(index===0){overlay.identityFor(item,qsTr("Chat"),true);backend.openChat(item.id)}else if(index===1)backend.launch("folder",item.id);else if(index===2){backend.closeMenu();owner.reveal();owner.editCategory(item.id,0)}else if(index===3){backend.closeMenu();owner.reveal();owner.editCategory(-1,item.id)}else backend.archive(item.id)}else if(index===0)overlay.choose(item);else{backend.closeMenu();owner.reveal();owner.editCategory(-1,Math.max(0,overlay.currentParent))}}
    }
   }
  }
 }
}
