import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
 id: root
 objectName: "keypadPanel"
 property var owner
 property var choices: [qsTr("Research"),qsTr("News"),qsTr("Theme"),qsTr("Notes")]
 property var descriptions: [qsTr("Open a guided research chat. Live web collection is not connected."),qsTr("Prepare a news briefing from sources you provide. Live news collection is not connected."),qsTr("Plan an appearance change in chat. This does not apply system settings."),qsTr("Organize pasted notes locally. No AI connection needed.")]
 property int jobIndex: -1
 parent: Overlay.overlay
 anchors.centerIn:parent
 width:Math.min(640,parent.width-32)
 height:Math.min(660,parent.height-32)
 modal:true;focus:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
 onOpened:keys.forceActiveFocus()
 function recording(){var r=backend.recorder;if(!r)return;var state=r.current.state;if(state==="recording"||state==="paused")r.togglePause();else r.start(qsTr("My workflow"))}
 function chooseJob(delta){var s=backend.tasks;if(!s||!s.jobs.length||s.busy)return;var current=-1;for(var i=0;i<s.jobs.length;i++)if(s.jobs[i].id===s.selected.id)current=i;jobIndex=current<0?(delta<0?s.jobs.length-1:0):(current+delta+s.jobs.length)%s.jobs.length;s.select(s.jobs[jobIndex].id)}
 function review(){backend.recorder.stop();root.close();owner.page="recordings"}
 function dispatch(key){
  var action="";for(var binding of backend.keypadControls)if(binding.key===key)action=binding.action
  if(action==="previous_quick")backend.quickIndex=backend.quickIndex-1
  else if(action==="next_quick")backend.quickIndex=backend.quickIndex+1
  else if(action==="open_quick"){root.close();backend.launchQuickTask()}
  else if(action==="previous_job")chooseJob(-1)
  else if(action==="next_job")chooseJob(1)
  else if(action==="activity"){root.close();owner.page="tasks";backend.tasks.refresh()}
  else if(action==="toggle_record")recording()
  else if(action==="finish_record")review()
  else if(action==="add_note"||action==="recordings"){root.close();owner.page="recordings";if(action==="add_note")owner.openRecordingNote()}
  else if(action==="new_task"){root.close();owner.page="tasks";owner.openTaskComposer()}
  else if(action==="toggle_job"){var s=backend.tasks;if(s&&!s.busy){var state=s.selected.status;if(state==="paused")s.control("resume");else if(state==="queued"||state==="working")s.control("pause")}}
  else if(action==="close")root.close()
 }
 contentItem:FocusScope {
  id:keys;focus:true
  Keys.onPressed:function(event){
   if(event.isAutoRepeat){event.accepted=true;return}
   var command=""
   if(event.key===Qt.Key_Escape)command="escape"
   else if(event.key===Qt.Key_Enter||event.key===Qt.Key_Return)command="enter"
   else if(event.modifiers&Qt.KeypadModifier){
    var aliases={};aliases[Qt.Key_Left]="4";aliases[Qt.Key_Clear]="5";aliases[Qt.Key_Up]="8";aliases[Qt.Key_Down]="2";aliases[Qt.Key_Home]="7";aliases[Qt.Key_PageUp]="9";aliases[Qt.Key_End]="1";aliases[Qt.Key_PageDown]="3";aliases[Qt.Key_Insert]="0";aliases[Qt.Key_Delete]="0";aliases[Qt.Key_Right]="6";
    if(aliases[event.key])command=aliases[event.key]
    else if(event.key>=Qt.Key_0&&event.key<=Qt.Key_9)command=String(event.key-Qt.Key_0)
    else if(event.key===Qt.Key_Plus)command="+"
    else if(event.key===Qt.Key_Minus)command="-"
    else if(event.key===Qt.Key_Asterisk)command="*"
   }
   if(command.length){root.dispatch(command);event.accepted=true}
  }
  ScrollView {id:keypadScroll;anchors.fill:parent;contentWidth:availableWidth
   ColumnLayout {width:keypadScroll.availableWidth;spacing:14
    Label {text:qsTr("Keyboard controls");font.pixelSize:24;Layout.fillWidth:true}
    Label {text:qsTr("Use the numpad while this panel is open. Your normal typing works again when you close it.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Label {text:root.choices[backend.quickIndex];font.pixelSize:28;color:theme.accent;Layout.fillWidth:true}
    Label {text:root.descriptions[backend.quickIndex];wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    RowLayout {
     Button {text:qsTr("4  Previous");onClicked:root.dispatch("4")}
     Button {text:qsTr("5  Next");onClicked:root.dispatch("5")}
     Button {text:qsTr("Enter  Open");onClicked:root.dispatch("enter")}
    }
    Label {text:qsTr("8 / 2  Select previous / next job\n6  Activity     +  New task     *  Pause / continue job\n7  Record / pause     9  Finish and review\n1  Add a note     3  Recordings\n0 / − / Escape  Close");wrapMode:Text.Wrap;Layout.fillWidth:true;lineHeight:1.5}
    Label {text:backend.tasks&&backend.tasks.selected.title?qsTr("Selected job: %1").arg(backend.tasks.selected.title):qsTr("No job selected");textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Label {text:backend.recorder.current.state==="recording"?qsTr("● Recording Hotbox actions"):backend.recorder.current.state==="paused"?qsTr("Recording paused"):qsTr("Recording is off");color:backend.recorder.current.state==="recording"?"#f1b593":"#aeb9c9";Layout.fillWidth:true}
    Label {text:qsTr("Browser actions require the optional companion. Passwords, cookie values and general keyboard typing are not recorded.");wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12;color:"#9198a6"}
    Button {text:qsTr("Close");onClicked:root.close()}
   }
  }
 }
}
