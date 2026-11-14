import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
 id: win
 width: 1120; height: 760; minimumWidth: 760; minimumHeight: 520
 visible: false; title: qsTr("Hotbox — %1").arg(backend.title); color: "#111318"
 Material.theme: Material.Dark; Material.accent: theme.accent; Material.roundedScale: theme.radius
 property bool allowClose: false
 property string page: "chat"
 property int editId: -1
 property int newParent: 0
 property bool helpVisible: false
 onClosing: function(close) { close.accepted = allowClose; if (!allowClose) hide() }
 Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !composer.activeFocus; onActivated: help.open() }
 Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; onActivated: {win.page="bindings";backend.refreshBindings();bindingSearch.forceActiveFocus()} }
 Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: win.editCategory(-1,0) }
 Shortcut { sequence: "Ctrl+,"; context: Qt.ApplicationShortcut; onActivated: settings.open() }
 Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: {backend.release();overlay.visible=false} }
 Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: {win.allowClose=true;Qt.quit()} }
 function editCategory(id,parent) {
  editId=id;newParent=parent
  var item=null
  for(var i=0;i<backend.categories.length;i++)if(backend.categories[i].id===id)item=backend.categories[i]
  categoryName.text=item?item.name:"";categoryFolder.text=item?item.folder:"";categoryBrief.text=item?item.brief:""
  editor.open()
 }
 function reveal() {show();raise();requestActivate()}
 Connections {
  target:backend
  function onShowChat(){win.page="chat";win.reveal();composer.forceActiveFocus()}
  function onShowBindings(){win.page="bindings";win.reveal();bindingSearch.forceActiveFocus()}
  function onCreateRequested(parent){win.reveal();win.editCategory(-1,parent)}
  function onChanged(){Qt.callLater(function(){feed.positionViewAtEnd()})}
 }
 Item {
  anchors.fill:parent
  LayoutMirroring.enabled:Qt.application.layoutDirection===Qt.RightToLeft;LayoutMirroring.childrenInherit:true
  RowLayout {
   anchors.fill:parent;spacing:0
   Rectangle {
    Layout.preferredWidth:234;Layout.fillHeight:true;color:"#0c0e12"
    ColumnLayout {
     anchors.fill:parent;anchors.margins:18;spacing:12
     RowLayout {
      Label {text:"hotbox";font.pixelSize:27;font.weight:Font.DemiBold;Layout.fillWidth:true}
      Label {text:"●";color:theme.accent;font.pixelSize:18}
     }
     Label {text:qsTr("YOUR WORKSPACES");color:"#858b99";font.pixelSize:10;font.letterSpacing:1.5}
     ListView {
      Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:5;model:backend.categories
      delegate:Rectangle {
       required property var modelData
       width:ListView.view.width;height:48;radius:theme.radius
       color:backend.currentId===modelData.id?"#252a33":(categoryMouse.containsMouse?"#191d24":"transparent")
       Rectangle {visible:backend.currentId===modelData.id;width:3;height:22;anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;color:theme.accent}
       Label {anchors.fill:parent;anchors.leftMargin:modelData.parent>0?25:13;verticalAlignment:Text.AlignVCenter;text:(modelData.id===1?"◎  ":"▸  ")+modelData.name;elide:Text.ElideRight;color:backend.currentId===modelData.id?"#ffffff":"#b4bac5"}
       MouseArea {id:categoryMouse;anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.LeftButton|Qt.RightButton;onClicked:function(mouse){if(mouse.button===Qt.RightButton){categoryContext.categoryId=modelData.id;categoryContext.popup()}else{backend.select(modelData.id);win.page="chat"}}}
      }
     }
     Button {text:qsTr("+  Add category");Layout.fillWidth:true;onClicked:win.editCategory(-1,0)}
     Rectangle {Layout.fillWidth:true;height:1;color:"#252a33"}
     Button {text:qsTr("⌨  Keybindings");Layout.fillWidth:true;flat:true;onClicked:{win.page="bindings";backend.refreshBindings();bindingSearch.forceActiveFocus()}}
     Button {text:qsTr("Model settings");Layout.fillWidth:true;flat:true;onClicked:settings.open()}
     Button {text:qsTr("Archived categories");Layout.fillWidth:true;flat:true;onClicked:archiveDialog.open()}
     Label {text:qsTr("Hold Tab + Q · 0.5 seconds");color:"#858b99";font.pixelSize:11;Layout.alignment:Qt.AlignHCenter}
     Button {text:qsTr("Preview hotbox");Layout.fillWidth:true;flat:true;onClicked:backend.preview()}
    }
   }
   ColumnLayout {
    Layout.fillWidth:true;Layout.fillHeight:true;spacing:0
    Rectangle {
     Layout.fillWidth:true;Layout.preferredHeight:88;color:"#15181e"
     RowLayout {
      anchors.fill:parent;anchors.margins:22
      ColumnLayout {Layout.fillWidth:true;spacing:5
       Label {text:win.page==="bindings"?qsTr("Omarchy keybindings"):backend.title;font.pixelSize:22;font.weight:Font.DemiBold}
       Label {text:win.page==="bindings"?qsTr("Search shortcuts from your current desktop configuration"):(backend.folder||qsTr("Persistent conversation · local memory"));color:"#9198a6";font.pixelSize:12;elide:Text.ElideMiddle;Layout.fillWidth:true}
      }
      Button {visible:win.page==="chat";text:qsTr("Project brief");onClicked:win.editCategory(backend.currentId,0)}
      Button {visible:win.page==="chat";text:qsTr("Folder");onClicked:backend.launch("folder")}
      Button {text:"?";onClicked:help.open()}
     }
    }
    ColumnLayout {
     visible:win.page==="chat";Layout.fillHeight:true;Layout.fillWidth:true;spacing:0
     Rectangle {
      visible:backend.brief.length>0;Layout.fillWidth:true;Layout.preferredHeight:Math.min(100,briefLabel.implicitHeight+28);color:"#1a2027"
      Label {id:briefLabel;anchors.fill:parent;anchors.margins:14;text:qsTr("PROJECT MEMORY  ·  %1").arg(backend.brief);wrapMode:Text.Wrap;color:"#aeb9c9";font.pixelSize:12;maximumLineCount:4;elide:Text.ElideRight}
     }
     Item {
      Layout.fillWidth:true;Layout.fillHeight:true
      Column {
       visible:backend.messages.length===0;anchors.centerIn:parent;width:Math.min(parent.width-60,520);spacing:16
       Label {text:"◎";font.pixelSize:50;color:theme.accent;anchors.horizontalCenter:parent.horizontalCenter}
       Label {text:qsTr("A place to pick up where you left off.");font.pixelSize:24;wrapMode:Text.Wrap;width:parent.width;horizontalAlignment:Text.AlignHCenter}
       Label {text:qsTr("Keep a conversation, project location, and living brief together. Add a category from the sidebar or the hotbox.");color:"#9198a6";font.pixelSize:14;wrapMode:Text.Wrap;width:parent.width;horizontalAlignment:Text.AlignHCenter}
      }
      ListView {
       id:feed;anchors.fill:parent;anchors.margins:22;clip:true;spacing:16;model:backend.messages
       ScrollBar.vertical:ScrollBar {}
       delegate:Item {
        required property var modelData
        width:feed.width;height:messageColumn.implicitHeight+30
        Rectangle {anchors.fill:parent;color:modelData.role==="user"?"#202631":"#171b22";radius:theme.radius}
        ColumnLayout {
         id:messageColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:15;spacing:9
         Label {text:modelData.role==="user"?qsTr("YOU"):qsTr("ASSISTANT");font.pixelSize:10;font.letterSpacing:1.4;color:modelData.role==="user"?"#a9b2c3":theme.accent}
         TextEdit {Layout.fillWidth:true;readOnly:true;selectByMouse:true;text:modelData.content||qsTr("Thinking…");textFormat:TextEdit.PlainText;wrapMode:TextEdit.Wrap;color:"#e4e7ed";font.pixelSize:15}
        }
       }
      }
     }
     Rectangle {
      Layout.fillWidth:true;Layout.preferredHeight:Math.min(190,Math.max(115,composer.contentHeight+65));Layout.margins:22;Layout.topMargin:0;color:"#20252e";border.color:composer.activeFocus?theme.accent:"#353c49";radius:theme.radius
      ColumnLayout {
       anchors.fill:parent;anchors.margins:12;spacing:5
       ScrollView {
        Layout.fillWidth:true;Layout.fillHeight:true
        TextArea {id:composer;placeholderText:qsTr("Message this workspace…");wrapMode:TextArea.Wrap;background:null;selectByMouse:true
         Keys.onReturnPressed:function(event){if(!(event.modifiers&Qt.ShiftModifier)){if(!backend.busy&&text.trim().length){backend.send(text);text=""}event.accepted=true}else event.accepted=false}
        }
       }
       RowLayout {Layout.fillWidth:true
        Label {text:qsTr("Enter to send · Shift+Enter for a new line");color:"#9198a6";font.pixelSize:10;Layout.fillWidth:true}
        Button {text:backend.busy?qsTr("Stop"):qsTr("Send ↑");enabled:backend.busy||composer.text.trim().length>0;onClicked:{if(backend.busy)backend.cancel();else{backend.send(composer.text);composer.text=""}}}
       }
      }
     }
    }
    ColumnLayout {
     visible:win.page==="bindings";Layout.fillWidth:true;Layout.fillHeight:true;Layout.margins:22;spacing:16
     RowLayout {Layout.fillWidth:true
      TextField {id:bindingSearch;Layout.fillWidth:true;placeholderText:qsTr("Search an action or key combination…")}
      Button {text:qsTr("Refresh");onClicked:backend.refreshBindings()}

     }
     Label {text:qsTr("%1 shortcuts loaded").arg(backend.bindings.length);color:"#9198a6"}
     ListView {
      Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:3
      model:backend.bindings.filter(function(line){var words=bindingSearch.text.toLowerCase().trim().split(/\s+/);return words.every(function(word){return line.toLowerCase().indexOf(word)>=0})})
      ScrollBar.vertical:ScrollBar {}
      delegate:Rectangle {
       required property string modelData
       required property int index
       width:ListView.view.width;height:shortcutLabel.implicitHeight+24;color:index%2?"#191e26":"#15191f";radius:theme.radius
       Label {id:shortcutLabel;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:12;anchors.verticalCenter:parent.verticalCenter;text:modelData;wrapMode:Text.Wrap;font.family:"monospace";font.pixelSize:13}
      }
     }
    }
    Rectangle {
     visible:backend.status.length>0;Layout.fillWidth:true;Layout.preferredHeight:statusLabel.implicitHeight+24;color:"#2c281c"
     Label {id:statusLabel;anchors.left:parent.left;anchors.right:closeStatus.left;anchors.verticalCenter:parent.verticalCenter;anchors.margins:12;text:backend.status;wrapMode:Text.Wrap;color:"#f1d99b"}
     ToolButton {id:closeStatus;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;text:"×";onClicked:backend.clearStatus()}
    }
   }
  }
 }
 Menu {
  id:categoryContext;property int categoryId:1
  MenuItem {text:qsTr("Open chat");onTriggered:backend.openChat(categoryContext.categoryId)}
  MenuItem {text:qsTr("Open folder");onTriggered:backend.launch("folder",categoryContext.categoryId)}
  MenuItem {text:qsTr("Edit specs and memory");onTriggered:win.editCategory(categoryContext.categoryId,0)}
  MenuItem {text:qsTr("Add child category");onTriggered:win.editCategory(-1,categoryContext.categoryId)}
  MenuItem {text:qsTr("Archive category");enabled:categoryContext.categoryId!==1;onTriggered:backend.archive(categoryContext.categoryId)}
 }
 Dialog {
  id:editor;anchors.centerIn:parent;width:Math.min(win.width-60,580);modal:true;title:win.editId<0?qsTr("New category"):qsTr("Workspace specs and memory");standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:{if(win.editId<0){var id=backend.addCategory(categoryName.text,categoryFolder.text,categoryBrief.text,win.newParent);if(id>0)backend.openChat(id)}else backend.updateCategory(win.editId,categoryName.text,categoryFolder.text,categoryBrief.text)}
  ColumnLayout {anchors.fill:parent;spacing:14
   TextField {id:categoryName;Layout.fillWidth:true;placeholderText:qsTr("Category name")}
   TextField {id:categoryFolder;Layout.fillWidth:true;placeholderText:qsTr("Project folder, e.g. ~/Work/weather")}
   Label {text:qsTr("Goal, specs, decisions, and saved memory");color:"#aeb9c9"}
   ScrollView {Layout.fillWidth:true;Layout.preferredHeight:220;TextArea {id:categoryBrief;wrapMode:TextArea.Wrap;placeholderText:qsTr("What are we building? What should this chat remember?");selectByMouse:true}}
   Label {text:qsTr("The folder is linked as context. Creating a category does not create or change its files.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6";font.pixelSize:11}
  }
 }
 Dialog {
  id:settings;anchors.centerIn:parent;width:Math.min(win.width-60,580);modal:true;title:qsTr("Model connection");standardButtons:Dialog.Save|Dialog.Cancel
  onOpened:{endpointField.text=backend.endpoint;modelField.text=backend.model;keyField.text=backend.keyVariable}
  onAccepted:backend.saveSettings(endpointField.text,modelField.text,keyField.text)
  ColumnLayout {anchors.fill:parent;spacing:14
   Label {text:qsTr("Connect an OpenAI-compatible chat endpoint. Messages and project context are sent when you press Send.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
   TextField {id:endpointField;Layout.fillWidth:true;placeholderText:qsTr("Chat completions endpoint URL")}
   TextField {id:modelField;Layout.fillWidth:true;placeholderText:qsTr("Model name")}
   TextField {id:keyField;Layout.fillWidth:true;placeholderText:qsTr("API key environment variable")}
   Label {text:qsTr("For a local Ollama server: http://127.0.0.1:11434/v1/chat/completions. Set the model to one you have installed. API keys are read from the named environment variable and are never saved in the database.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6";font.pixelSize:12}
  }
 }
 Dialog {id:archiveDialog;anchors.centerIn:parent;width:440;modal:true;title:qsTr("Archived categories");standardButtons:Dialog.Close
  ColumnLayout {anchors.fill:parent
   Repeater {model:backend.archivedCategories();delegate:Button {required property var modelData;text:qsTr("Restore %1").arg(modelData.name);Layout.fillWidth:true;onClicked:{backend.restore(modelData.id);archiveDialog.close()}}}
   Label {visible:backend.archivedCategories().length===0;text:qsTr("No archived categories.")}
  }
 }
 Dialog {id:help;anchors.centerIn:parent;width:520;modal:true;title:qsTr("Hotbox controls");standardButtons:Dialog.Close
  Label {width:parent.width;wrapMode:Text.Wrap;text:qsTr("Hold Tab, then Q for 0.5 seconds to spread the menu at your pointer. Release either key to retract. Click an item to choose; right-click for deeper options. Double-click the center for master chat.\n\nCtrl+K: search keybindings\nCtrl+N: add category\nCtrl+,: model settings\nCtrl+Q: quit Hotbox\n?: this help\n\nThe Preview hotbox button opens a click-controlled preview. Click the background to dismiss it.")}
 }
 Hotbox {id:overlay;objectName:"hotboxOverlay";owner:win}
}
