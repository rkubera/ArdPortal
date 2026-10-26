#!/usr/bin/env python3
"""Test the actual Matter command service gate during persistence debounce."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/MatterPortalImpl.h').read_text()
a=s.rindex('#if ARDPORTAL_MATTER_COMMAND_SUPPORT', 0, s.index('  // Same-source commands can merge'))
b=s.index('  if (int32_t(now - _matter._reportAt)',a)
fixture=r'''
#include <cassert>
#include "MatterFields.h"
#define ARDPORTAL_ENABLE_MATTER_CONSOLE 0
struct Portal {
 enum class ChangeSource { Matter, Portal };
 bool _saveQueued=false,_saveRetryPending=false,_pendingReady=false,_savePending=false,_httpWaitingStorage=false;
 ChangeSource _saveSource=ChangeSource::Matter;
 struct Deferred {unsigned size=0;unsigned length(){return size;}} _deferredApp;
 struct Storage {bool active=false;bool busy(){return active;}} _storage;
 bool trial=false; bool mqttTrialActive(){return trial;}
 struct Matter {
  unsigned queued=2, dirtied=0;
  bool pop(ArdMatterFields::Command& cmd){cmd.field=0;if(!queued)return false;--queued;return true;}
  void dirty(uint16_t){++dirtied;}
 } _matter;
 unsigned accepted=0;
 bool acceptMatterCommand(const ArdMatterFields::Command&){++accepted;_savePending=true;return true;}
 void service();
};
void Portal::service(){
'''
main=r'''
}
int main(){
 Portal p;
 p.service();assert(p.accepted==1 && p._savePending);
 p.service();assert(p.accepted==2 && p._matter.dirtied==2);
 // Every conflicting tree owner/storage guard still blocks processing.
 for(unsigned guard=0;guard<9;++guard){
  Portal q;
  switch(guard){
   case 0:q._saveQueued=true;break;
   case 1:q._saveRetryPending=true;break;
   case 2:q._pendingReady=true;break;
   case 3:q._deferredApp.size=1;break;
   case 4:q._savePending=true;q._saveSource=Portal::ChangeSource::Portal;break;
   case 5:q._httpWaitingStorage=true;break;
   case 6:q._storage.active=true;break;
   case 7:q.trial=true;break;
   case 8:q._matter.queued=0;break;
  }
  q.service();assert(q.accepted==0 && q._matter.dirtied==0);
 }
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-service-') as directory:
    path=Path(directory)
    (path/'test.cpp').write_text(fixture+s[a:b]+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: actual Matter service merges same-source debounce and preserves storage guards')
