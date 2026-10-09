// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdFS.h"
#define ARDUI_DEFINE_STORAGE_MESSAGES
#include "Language.h"
#undef ARDUI_DEFINE_STORAGE_MESSAGES

namespace {
/**
 * @brief Build the JSON resource limits for a stored configuration document.
 * @return The JSON resource limits for a stored configuration document.
 */
ArdJSON::Limits documentLimits() {
  ArdJSON::Limits limits; limits.maxInputBytes = ArdFS::MaxBytes;
  limits.maxStringBytes = ArdFS::MaxBytes; limits.maxNodes = 4096;
  return limits;
}
/**
 * @brief Build the JSON resource limits for a journal envelope.
 * @return The JSON resource limits for a journal envelope.
 */
ArdJSON::Limits journalLimits() {
  ArdJSON::Limits limits; limits.maxInputBytes = ArdFSMaxJournalBytes; limits.maxOutputBytes = ArdFSMaxJournalBytes;
  limits.maxStringBytes = ArdFS::MaxBytes; limits.maxDepth = 1; limits.maxNodes = 5;
  return limits;
}
}
/**
 * @brief Initialize the component and prepare its runtime state.
 * @return No value.
 */
void ArdFS::begin() {
  if (_mountDone || _phase != Phase::Idle) return;
  _phase = Phase::Mount;
}
/**
 * @brief Prepare a new operation after checking that the component is idle.
 * @param path Journal file path.
 * @param callback Completion or event handler supplied to this operation.
 * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
 */
bool ArdFS::start(const String& path, Callback callback) {
  if (!_mountDone || !_mounted || busy() || !callback || path.length() < 2 ||
      path.length() > 64 || path[0] != '/' || path.indexOf("..") >= 0) return false;
  _path = path; _callback = callback; _slot = 0; _best = -1;
  _buffer = String(); _bestData = String(); _input = String();
  for (int i = 0; i < 2; ++i) { _valid[i] = _present[i] = _match[i] = false; _generation[i] = 0; }
  _phase = Phase::OpenRead; return true;
}
/**
 * @brief Request or perform a read through the component API.
 * @param path Journal file path.
 * @param callback Completion or event handler supplied to this operation.
 * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
 */
bool ArdFS::read(const String& path, Callback callback) {
  if (!start(path, callback)) return false;
  _writing = false; return true;
}
/**
 * @brief Request or perform a write through the component API.
 * @param path Journal file path.
 * @param json JSON document text to persist.
 * @param callback Completion or event handler supplied to this operation.
 * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
 */
bool ArdFS::write(const String& path, const String& json, Callback callback) {
  if (!json.length() || json.length() > MaxBytes || !start(path, callback)) return false;
  _writing = true; _input = json; return true;
}

bool ArdFS::writeJson(const String& path,size_t length,JsonSource source,Callback callback) {
 if(!length||length>MaxBytes||!source||!start(path,callback))return false;
 _writing=true;_source=std::move(source);_sourceLength=length;_input=String();return true;
}
void ArdFS::resetJournalOutput(){_journalStage=0;_journalHeaderAt=0;_sourceAt=0;_rawAt=0;_escapeAt=_escapeLength=0;_raw=String();}
String ArdFS::journalOutput(){
 String output;if(!output.reserve(256))return output;
 while(output.length()<256&&_journalStage<3){
  if(_journalStage==0){if(_journalHeaderAt<_journalHeader.length()){output+=_journalHeader[_journalHeaderAt++];continue;}_journalStage=1;}
  if(_journalStage==1){
   if(_escapeAt<_escapeLength){output+=_escape[_escapeAt++];continue;}
   if(_rawAt>=_raw.length()){
    if(_sourceAt>=_sourceLength){_journalStage=2;_rawAt=0;continue;}
    size_t n=_sourceLength-_sourceAt;if(n>128)n=128;_raw=_source(_sourceAt,n);_rawAt=0;
    if(_raw.length()!=n){_sourceFailed=true;return String();}
   }
   uint8_t c=_raw[_rawAt++];++_sourceAt;_escapeAt=0;
   if(c=='"'||c=='\\'){_escape[0]='\\';_escape[1]=c;_escapeLength=2;}
   else if(c<32){static const char hex[]="0123456789abcdef";_escape[0]='\\';_escape[1]='u';_escape[2]=_escape[3]='0';_escape[4]=hex[c>>4];_escape[5]=hex[c&15];_escapeLength=6;}
   else{_escape[0]=c;_escapeLength=1;}
  }
  if(_journalStage==2){output+=_rawAt++==0?'"':'}';if(_rawAt>=2)_journalStage=3;}
 }
 return output;
}
/**
 * @brief Queue formatting of the storage volume; existing records will be discarded.
 * @param callback Completion or event handler supplied to this operation.
 * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
 */
bool ArdFS::format(Callback callback) {
  if (!_mountDone || busy() || !callback) return false;
#if defined(ESP8266)
  if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize()) return false;
#endif
  _callback = callback; _formatRequested = true; _mountDone = false; _phase = Phase::End; return true;
}
/**
 * @brief Compute the checksum covering the journal generation and document bytes.
 * @param generation Journal generation number included in the checksum.
 * @param data Data buffer or value used by the operation.
 * @return The checksum covering the journal generation and document bytes.
 */
uint32_t ArdFS::checksum(uint32_t generation, const String& data) {
  ArdCooperativeBudget budget;uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < data.length() + 4; ++i) {
    if((i&255)==0)budget.checkpoint();
    uint8_t byte = i < 4 ? uint8_t(generation >> (i * 8)) : uint8_t(data[i - 4]);
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
  }
  return ~crc;
}
/**
 * @brief Complete the operation and report its stored result.
 * @param ok Whether the completed operation succeeded.
 * @param error Output error text; populated when the operation fails.
 * @param changed Whether persistent contents changed during this operation.
 * @return No value.
 */
void ArdFS::finish(bool ok, const String& error, bool changed) {
  Result result; result.ok = ok; result.error = error; result.changed = changed;
  result.found = _best >= 0;
  result.recovered = result.found && ((_present[0] && !_valid[0]) || (_present[1] && !_valid[1]));
  if (ok && !_writing && !_formatRequested) result.data = std::move(_bestData);
  _source=nullptr;_sourceCompare=String();_raw=String();_journalHeader=String();
  _error = error; _buffer = String(); _input = String(); _bestData = String(); _phase = Phase::Idle;
  _formatRequested = false;
  Callback callback = std::move(_callback); _callback = nullptr;
  if (callback) callback(result);
}
/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void ArdFS::loop() {
  using ArdJSON::JSON; using ArdJSON::JSONVar;
  uint8_t chunk[256];
  switch (_phase) {
    case Phase::Mount:
#if defined(ESP8266)
      if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize()) {
        _mountDone = true; finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_194)); return;
      }
      _mounted = _volume.begin();
#else
      _mounted = _volume.begin();
#endif
      if (_mounted) { _mountDone = true; _phase = Phase::Idle; }
      else _phase = Phase::End;
      return;
    case Phase::End: _volume.end(); _mounted = false; _phase = Phase::Format; return;
    case Phase::Format:
      if (!_volume.format()) { _mountDone = true; finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_195)); return; }
      _phase = Phase::Remount; return;
    case Phase::Remount:
#if defined(ESP32)
      _mounted = _volume.begin();
#else
      _mounted = _volume.begin();
#endif
      _mountDone = true;
      if (_formatRequested) finish(_mounted, _mounted ? "" : ArdUILanguage::storageText(ArdUILanguage::Key::s_196), _mounted);
      else { _phase = Phase::Idle; if (!_mounted) _error = ArdUILanguage::storageText(ArdUILanguage::Key::s_196); }
      return;
    case Phase::Idle: return;
    case Phase::OpenRead:
      if(_source){_journal=ArdJournalStream();_sourceLegacy=false;_sourceMatch=true;_sourceFailed=false;_sourceCompare=String();_sourceCompareAt=0;}
      _present[_slot] = _volume.exists(slotPath(_slot).c_str());
      if (_volume.lastError() && _volume.lastError() != -2) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_199)); return; }
      if (!_present[_slot]) { _phase = Phase::Validate; return; }
      _file = _volume.open(slotPath(_slot).c_str(), "r");
      if (!_file) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_199)); return; }
      _expected = _file.size(); _offset = 0;
      if (!_expected || _expected > ArdFSMaxJournalBytes) { _file.close(); _phase = Phase::Validate; return; }
      if ((!_source||_sourceLegacy) && !_buffer.reserve(_expected)) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_198)); return; }
      _phase = Phase::Read; return;
    case Phase::Read: {
      size_t count = _expected - _offset; if (count > sizeof(chunk)) count = sizeof(chunk);
      if (!count) { _file.close(); _phase = Phase::Validate; return; }
      if (_file.read(chunk, count) != int(count)) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_199)); return; }
      if(_source&&!_sourceLegacy){
        auto compare=[&](uint8_t b,size_t at){
          if(at>=MaxBytes)return false;
          if(!_sourceMatch)return true;
          if(at>=_sourceLength){_sourceMatch=false;return true;}
          if(!_sourceCompare.length()||at<_sourceCompareAt||at>=_sourceCompareAt+_sourceCompare.length()){
            _sourceCompareAt=at;size_t n=_sourceLength-at;if(n>256)n=256;_sourceCompare=_source(at,n);
            if(_sourceCompare.length()!=n){_sourceFailed=true;return false;}
          }
          if(b!=uint8_t(_sourceCompare[at-_sourceCompareAt]))_sourceMatch=false;
          return true;
        };
        for(size_t i=0;i<count;++i)_journal.input(chunk[i],compare);
        if(_sourceFailed){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
        if(_journal.failed&&!_journal.canonical){
          _sourceLegacy=true;if(!_buffer.reserve(_expected)){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
          for(size_t i=0;i<count;++i)_buffer+=char(chunk[i]);
        }
      }else for (size_t i = 0; i < count; ++i) _buffer += char(chunk[i]);
      _offset += count; return;
    }
    case Phase::Validate: {
      if(_source&&!_sourceLegacy){
        if(_journal.valid()){
          _valid[_slot]=true;_generation[_slot]=_journal.generation;
          _match[_slot]=_sourceMatch&&_journal.length==_sourceLength;
          if(_best<0||int32_t(_generation[_slot]-_generation[_best])>0)_best=_slot;
        }
        _sourceCompare=String();
      }
      if (_buffer.length()) {
        const JSONVar root = _json.parse(_buffer, nullptr, journalLimits());
        int64_t version = 0, generation = 0, crc = 0;
        if (root.isValid() && root.type() == JSONVar::Type::Object && root.length() == 4 &&
            root["journal"].toInteger(version) && version == 1 &&
            root["generation"].toInteger(generation) && generation >= 0 && uint64_t(generation) <= UINT32_MAX &&
            root["crc32"].toInteger(crc) && crc >= 0 && uint64_t(crc) <= UINT32_MAX &&
            root["data"].type() == JSONVar::Type::String) {
          String data = root["data"].asString();
          if (data.length() <= MaxBytes && checksum(uint32_t(generation), data) == uint32_t(crc)) {
            _valid[_slot] = true; _generation[_slot] = uint32_t(generation); _match[_slot] = data == _input;
            if (_best < 0 || int32_t(_generation[_slot] - _generation[_best]) > 0) {
              _best = _slot; if (!_writing) _bestData = data;
            }
          }
        }
        // Import the existing text JSON configuration once; never binary files.
        else if (_slot == 0 && _buffer.length() <= MaxBytes) {
          const JSONVar legacy = _json.parse(_buffer);
          if (legacy.isValid() && legacy.type() == JSONVar::Type::Object && !legacy.hasOwnProperty("journal")) {
            _valid[0] = true; _generation[0] = 0; _match[0] = _buffer == _input;
            if (_best < 0) { _best = 0; if (!_writing) _bestData = _buffer; }
          }
        }
      }
      _buffer = "";
      if (++_slot < 2) _phase = Phase::OpenRead;
      else if (_writing) _phase = Phase::Prepare;
      else if (_best >= 0 || (!_present[0] && !_present[1])) finish(true);
      else finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_200));
      return;
    }
    case Phase::Prepare: {
      if (_best >= 0 && _match[_best]) { ++_skipped; finish(true); return; }
      if(_source){
        _sourceGeneration=_best>=0?_generation[_best]+1:1;_sourceCrc=UINT32_MAX;
        for(uint8_t i=0;i<4;++i)_sourceCrc=ArdJournalStream::update(_sourceCrc,uint8_t(_sourceGeneration>>(8*i)));
        _sourceAt=0;_escapedLength=0;_phase=Phase::SourceCrc;return;
      }
      if (!_json.parse(_input, nullptr, documentLimits()).isValid()) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_200)); return; }
      uint32_t generation = _best >= 0 ? _generation[_best] + 1 : 1;
      JSONVar record = JSONVar::object(); record["journal"] = 1; record["generation"] = generation;
      record["crc32"] = checksum(generation, _input); record["data"] = _input;
      _input = String();
      _buffer = _json.stringify(record, false, nullptr, journalLimits());
      if (!_buffer.length()) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_198)); return; }
      _slot = _best == 0 ? 1 : 0; _offset = 0; _phase = Phase::OpenWrite; return;
    }
    case Phase::SourceCrc: {
      if(_sourceAt<_sourceLength){
        size_t n=_sourceLength-_sourceAt;if(n>256)n=256;String part=_source(_sourceAt,n);
        if(part.length()!=n){finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
        for(size_t i=0;i<n;++i){uint8_t c=part[i];_sourceCrc=ArdJournalStream::update(_sourceCrc,c);_escapedLength+=c<32?6:(c=='"'||c=='\\'?2:1);}
        _sourceAt+=n;return;
      }
      char header[128];int headerBytes=snprintf(header,sizeof(header),"{\"journal\":1,\"generation\":%lu,\"crc32\":%lu,\"data\":\"",(unsigned long)_sourceGeneration,(unsigned long)~_sourceCrc);
      _journalHeader=header;
      if(headerBytes<=0||size_t(headerBytes)>=sizeof(header)||_journalHeader.length()!=size_t(headerBytes)){finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
      _expected=_journalHeader.length()+_escapedLength+2;
      if(_expected>ArdFSMaxJournalBytes){finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
      _slot=_best==0?1:0;_offset=0;resetJournalOutput();_phase=Phase::OpenWrite;return;
    }
    case Phase::OpenWrite:
      _file = _volume.open(slotPath(_slot).c_str(), "w");
      if (!_file) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _phase = Phase::Write; return;
    case Phase::Write: {
      if(_source){
        String part=journalOutput();if(_sourceFailed){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
        if(!part.length()){if(_offset!=_expected){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}_phase=Phase::Flush;return;}
        if(_file.write(reinterpret_cast<const uint8_t*>(part.c_str()),part.length())!=part.length()){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_204));return;}
        _offset+=part.length();return;
      }
      size_t count = _buffer.length() - _offset; if (count > sizeof(chunk)) count = sizeof(chunk);
      if (!count) { _phase = Phase::Flush; return; }
      if (_file.write(reinterpret_cast<const uint8_t*>(_buffer.c_str()) + _offset, count) != count) {
        _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return;
      }
      _offset += count; return;
    }
    case Phase::Flush:
      if (!_file.flush()) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _phase = Phase::CloseWrite; return;
    case Phase::CloseWrite:
      if (!_file.close()) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _phase = Phase::OpenVerify; return;
    case Phase::OpenVerify:
      _file = _volume.open(slotPath(_slot).c_str(), "r");
      if (!_file || _file.size() != (_source?_expected:_buffer.length())) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _offset = 0;if(_source)resetJournalOutput(); _phase = Phase::Verify; return;
    case Phase::Verify: {
      if(_source){
        String part=journalOutput();if(_sourceFailed){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_198));return;}
        if(!part.length()){if(_offset!=_expected){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_204));return;}_phase=Phase::CloseVerify;return;}
        if(_file.read(chunk,part.length())!=int(part.length())||memcmp(chunk,part.c_str(),part.length())){_file.close();finish(false,ArdUILanguage::storageText(ArdUILanguage::Key::s_204));return;}
        _offset+=part.length();return;
      }
      size_t count = _buffer.length() - _offset; if (count > sizeof(chunk)) count = sizeof(chunk);
      if (!count) { _phase = Phase::CloseVerify; return; }
      if (_file.read(chunk, count) != int(count) || memcmp(chunk, _buffer.c_str() + _offset, count)) {
        _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return;
      }
      _offset += count; return;
    }
    case Phase::CloseVerify: _file.close(); ++_commits; finish(true, "", true); return;
  }
}
