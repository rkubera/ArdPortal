// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdFSVolume.h"
#include "ArdFSStorageLimits.h"
#if defined(ESP8266)
#include <flash_hal.h>
#endif
#include <algorithm>
namespace {
constexpr uint32_t BankMagic=0x31445241, RecordMagic=0x31524341, Commit=0x51ac0ffe;
constexpr uint32_t Sector=4096, Header=32;
/**
 * @brief Round a journal extent up to a flash sector boundary.
 * @param n Number of bytes or elements to process.
 * @return The smallest whole-sector extent containing the requested byte count.
 */
uint32_t aligned(uint32_t n){return (n+Sector-1)/Sector*Sector;}
// Fields are explicitly little endian, independent of compiler struct packing.
/**
 * @brief Encode a 32-bit word into little-endian journal bytes.
 * @param p Destination for four bytes.
 * @param n Word to encode.
 * @return No value.
 */
void put(uint8_t* p,uint32_t n){for(int i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));}
/**
 * @brief Decode a little-endian 32-bit journal word.
 * @param p Pointer to four source bytes.
 * @return Decoded word.
 */
uint32_t get(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
}
/**
 * @brief Update the journal CRC-32 checksum over a byte range.
 * @param bytes Byte buffer to read or write.
 * @param length Number of bytes or elements to process.
 * @param c Current character or running CRC state, according to its type.
 * @return Updated CRC-32 accumulator; callers apply the final complement when committing a record.
 */
uint32_t ArdFSVolume::crc(const void* bytes,size_t length,uint32_t c){auto p=static_cast<const uint8_t*>(bytes);for(size_t i=0;i<length;++i){c^=p[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320U&(0U-(c&1)));}return c;}
/**
 * @brief Locate and validate the flash region used by the two-bank journal.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdFSVolume::configure(){if(_configured)return true;
#if defined(ESP8266)
 _address=FS_PHYS_ADDR;_size=FS_PHYS_SIZE;
 if(ESP.getFlashChipSize()>ESP.getFlashChipRealSize()||_address<ESP.getSketchSize()||_address>ESP.getFlashChipRealSize()||_size>ESP.getFlashChipRealSize()-_address)return false;
#elif defined(ESP32)
 _partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"ardfs");
 if(!_partition)_partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"littlefs");
 if(!_partition)_partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
 if (!_partition || _partition->encrypted) {
   return false;
 }
 _size = _partition->size;
#endif
 _bankSize=(_size/(2*Sector))*Sector;_configured=_bankSize>=8*Sector&&_address%Sector==0;return _configured;}
bool ArdFSVolume::readRaw(uint32_t address,void* bytes,size_t n){if(address>_size||n>_size-address)return false;
#if defined(ESP8266)
 // Read aligned words even when a document starts after an odd-length path.
 alignas(4) uint32_t words[64];auto* output=static_cast<uint8_t*>(bytes);
 while(n){uint32_t absolute=_address+address,skip=absolute&3U;size_t count=std::min<size_t>(n,sizeof(words)-skip);size_t rounded=(skip+count+3)&~size_t(3);
  if(!ESP.flashRead(absolute-skip,reinterpret_cast<uint8_t*>(words),rounded))return false;
  memcpy(output,reinterpret_cast<uint8_t*>(words)+skip,count);output+=count;address+=count;n-=count;yield();
 }return true;
#else
 auto* output=static_cast<uint8_t*>(bytes);
 while(n){size_t count=std::min<size_t>(n,256);if(esp_partition_read(_partition,address,output,count)!=ESP_OK)return false;address+=count;output+=count;n-=count;yield();}return true;
#endif
}
/**
 * @brief Write a bounded flash range in small transfers, yielding between chunks.
 * @param address Byte offset within the configured flash region.
 * @param bytes Byte buffer to read or write.
 * @param n Number of bytes or elements to process.
 * @return True if all requested bytes were written; false for invalid bounds or a flash error.
 */
bool ArdFSVolume::writeRaw(uint32_t address,const void* bytes,size_t n){if(address>_size||n>_size-address)return false;
 auto* input=static_cast<const uint8_t*>(bytes);
 while(n){size_t count=std::min<size_t>(n,256);
#if defined(ESP8266)
  if(!ESP.flashWrite(_address+address,input,count))return false;
#else
  if(esp_partition_write(_partition,address,input,count)!=ESP_OK)return false;
#endif
  address+=count;input+=count;n-=count;yield();
 }return true;
}
/**
 * @brief Erase complete flash sectors, yielding between sectors.
 * @param address Byte offset within the configured flash region.
 * @param n Number of bytes or elements to process.
 * @return True if the aligned sector range was erased; false for invalid alignment, bounds or a flash error.
 */
bool ArdFSVolume::eraseRaw(uint32_t address,size_t n){if(address%Sector||n%Sector||address>_size||n>_size-address)return false;
 for(size_t i=0;i<n;i+=Sector){
#if defined(ESP8266)
  if(!ESP.flashEraseSector((_address+address+i)/Sector))return false;
#else
  if(esp_partition_erase_range(_partition,address+i,Sector)!=ESP_OK)return false;
#endif
  yield();
 }return true;
}
/**
 * @brief Scan committed journal records and rebuild the live file index.
 * @param bank Flash offset of the selected journal bank.
 * @param entries Entry collection to read or update.
 * @param next Output or in/out cursor for the next record.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdFSVolume::scan(uint32_t bank,std::vector<Entry>& entries,uint32_t& next){next=Sector;entries.clear();
 while(next+Header<=_bankSize){uint8_t h[Header];if(!readRaw(bank+next,h,sizeof(h)))return false;
 if(get(h)==0xffffffff){return true;}
 uint32_t extent=get(h+4),name=get(h+8),length=get(h+12);
 if(get(h)!=RecordMagic||extent!=aligned(Header+name+length)||extent>_bankSize-next||!name||name>72||length>ArdFSMaxJournalBytes){next=_bankSize;return true;}
 if(get(h+24)==Commit){uint8_t bytes[256];uint32_t c=crc(h,16);String path;if(!path.reserve(name))return false;
 for(uint32_t offset=0;offset<name+length;offset+=sizeof(bytes)){size_t count=std::min<size_t>(size_t(name+length-offset),sizeof(bytes));if(!readRaw(bank+next+Header+offset,bytes,count))return false;c=crc(bytes,count,c);for(size_t j=0;j<count&&offset+j<name;++j)path+=char(bytes[j]);}
 if(~c==get(h+16)){auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry&e){return e.path==path;});Entry e{path,bank+next+Header+name,length};if(it==entries.end()){if(entries.size()>=32)return false;entries.push_back(e);}else *it=e;}}
 next+=extent;yield();
 }return true;}
/**
 * @brief Initialize the component and prepare its runtime state.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdFSVolume::begin(){if(_mounted)return true;
 if(!configure()){_error=-22;return false;}uint8_t h[2][Header];bool valid[2];
 for(int i=0;i<2;++i){if(!readRaw(i*_bankSize,h[i],Header)){_error=-5;return false;}valid[i]=get(h[i])==BankMagic&&get(h[i]+4)==1&&get(h[i]+12)==_bankSize&&get(h[i]+24)==Commit&&get(h[i]+16)==~crc(h[i],16);}
 int first=valid[1]&&(!valid[0]||int32_t(get(h[1]+8)-get(h[0]+8))>0)?1:0;
 for(int k=0;k<2;++k){int i=k?1-first:first;if(valid[i]&&scan(i*_bankSize,_entries,_next)){_active=i*_bankSize;_generation=get(h[i]+8);_mounted=true;_error=0;return true;}}
 _error=-84;return false;}
/**
 * @brief Format the storage volume, discarding its existing records.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdFSVolume::format(){
 if(!configure()){_error=-22;return false;}_mounted=false;if(!eraseRaw(0,_bankSize*2)){_error=-5;return false;}
 uint8_t h[Header];memset(h,255,sizeof(h));put(h,BankMagic);put(h+4,1);put(h+8,1);put(h+12,_bankSize);put(h+16,~crc(h,16));
 if(!writeRaw(0,h,Header)){_error=-5;return false;}uint8_t marker[4];put(marker,Commit);if(!writeRaw(24,marker,4)){_error=-5;return false;}return true;}
/**
 * @brief Check whether the mounted journal contains the requested path.
 * @param path Journal file path.
 * @return True if the path exists; false if missing or not mounted.
 */
bool ArdFSVolume::exists(const char* path){
 bool found=_mounted&&std::any_of(_entries.begin(),_entries.end(),[&](const Entry&e){return e.path==path;});_error=found?0:-2;return found;}
/**
 * @brief Append the supplied data or journal record to the current destination.
 * @param bank Flash offset of the selected journal bank.
 * @param next Output or in/out cursor for the next record.
 * @param path Journal file path.
 * @param data Data buffer or value used by the operation.
 * @param entries Entry collection to read or update.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdFSVolume::append(uint32_t bank,uint32_t& next,const String& path,const String& data,std::vector<Entry>& entries){uint32_t n=path.length(),extent=aligned(Header+n+data.length());if(extent>_bankSize-next)return false;
 uint8_t probe[256];bool erased=true;for(uint32_t i=0;i<extent&&erased;i+=sizeof(probe)){if(!readRaw(bank+next+i,probe,sizeof(probe)))return false;for(uint8_t b:probe)if(b!=0xff){erased=false;break;}}
 if(!erased&&!eraseRaw(bank+next,extent))return false;
 uint8_t h[Header];memset(h,255,Header);put(h,RecordMagic);put(h+4,extent);put(h+8,n);put(h+12,data.length());uint32_t c=crc(h,16);c=crc(path.c_str(),n,c);c=crc(data.c_str(),data.length(),c);put(h+16,~c);
 if(!writeRaw(bank+next,h,Header)||!writeRaw(bank+next+Header,path.c_str(),n)||(data.length()!=0&&!writeRaw(bank+next+Header+n,data.c_str(),data.length())))return false;
 // Read back header/body before the final commit marker.
 uint8_t vh[Header];if(!readRaw(bank+next,vh,Header)||memcmp(vh,h,Header))return false;uint32_t verified=crc(vh,16);for(size_t i=0;i<n+data.length();i+=sizeof(probe)){size_t count=std::min<size_t>(sizeof(probe),n+data.length()-i);if(!readRaw(bank+next+Header+i,probe,count))return false;verified=crc(probe,count,verified);}if(~verified!=get(h+16))return false;
 uint8_t marker[4];put(marker,Commit);if(!writeRaw(bank+next+24,marker,4))return false;
 auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry&e){return e.path==path;});Entry e{path,bank+next+Header+n,uint32_t(data.length())};if(it==entries.end())entries.push_back(e);else *it=e;next+=extent;return true;}
/**
 * @brief Copy live records to the alternate bank and atomically commit a new generation with the replacement file.
 * @param path Journal file path.
 * @param replacement New file content to commit.
 * @return True when the new bank is committed; false on allocation or flash failure.
 */
bool ArdFSVolume::compact(const String& path,const String& replacement){uint32_t required=aligned(Header+path.length()+replacement.length());for(const Entry&e:_entries)if(e.path!=path)required+=aligned(Header+e.path.length()+e.length);if(required>_bankSize-Sector)return false;
 uint32_t bank=_active?0:_bankSize,next=Sector;std::vector<Entry> copied;if(!eraseRaw(bank,Sector))return false;
 for(const Entry&e:_entries){if(e.path==path)continue;String data;if(!data.reserve(e.length))return false;uint8_t chunk[256];for(uint32_t offset=0;offset<e.length;offset+=sizeof(chunk)){size_t n=std::min<size_t>(size_t(e.length-offset),sizeof(chunk));if(!readRaw(e.address+offset,chunk,n))return false;for(size_t i=0;i<n;++i)data+=char(chunk[i]);}if(data.length()!=e.length||!append(bank,next,e.path,data,copied))return false;}
 if(!append(bank,next,path,replacement,copied))return false;
 if(next<_bankSize&&!eraseRaw(bank+next,Sector))return false;
 uint8_t h[Header];memset(h,255,Header);put(h,BankMagic);put(h+4,1);put(h+8,_generation+1);put(h+12,_bankSize);put(h+16,~crc(h,16));if(!writeRaw(bank,h,Header))return false;uint8_t marker[4];put(marker,Commit);if(!writeRaw(bank+24,marker,4))return false;
 _active=bank;_next=next;++_generation;_entries=std::move(copied);return true;}
/**
 * @brief Append new file contents to the journal, compacting the bank when necessary.
 * @param path Journal file path.
 * @param data Data buffer or value used by the operation.
 * @return True when the record is committed; false on space, allocation or flash failure.
 */
bool ArdFSVolume::commit(const String& path,const String& data){uint32_t need=aligned(Header+path.length()+data.length());bool known=exists(path.c_str());if(!known&&_entries.size()>=32){_error=-28;return false;}
 if(need>_bankSize-Sector){_error=-28;return false;}if(_next>_bankSize||need>_bankSize-_next){if(!compact(path,data)){_error=-5;return false;}return true;}
 if(need>_bankSize-_next){_error=-28;return false;}uint32_t prior=_next;if(!append(_active,_next,path,data,_entries)){_next=prior+need;_error=-5;return false;}_error=0;return true;}
/**
 * @brief Open a journal path for the requested access mode.
 * @param path Journal file path.
 * @param mode File access mode.
 * @return File handle; its boolean conversion is false when opening fails.
 */
ArdFSFile ArdFSVolume::open(const char* path,const char* mode){ArdFSFile f;if(!_mounted||!path||!mode||(*mode!='r'&&*mode!='w')){_error=-22;return f;}f._handle.reset(new(std::nothrow) ArdFSFile::Handle);if(!f._handle){_error=-12;return f;}auto& h=*f._handle;h.volume=this;h.path=path;h.writing=*mode=='w';
 if(!h.writing){auto it=std::find_if(_entries.begin(),_entries.end(),[&](const Entry&e){return e.path==path;});if(it==_entries.end()){f._handle.reset();_error=-2;return f;}if(!h.data.reserve(it->length)){f._handle.reset();_error=-12;return f;}uint8_t bytes[256];for(uint32_t offset=0;offset<it->length;offset+=sizeof(bytes)){size_t n=std::min<size_t>(size_t(it->length-offset),sizeof(bytes));if(!readRaw(it->address+offset,bytes,n)){f._handle.reset();_error=-5;return f;}for(size_t i=0;i<n;++i)h.data+=char(bytes[i]);}}_error=0;return f;}
/**
 * @brief Check whether the journal file handle is open.
 * @return True for an open handle; false otherwise.
 */
ArdFSFile::operator bool() const{return bool(_handle);}
/**
 * @brief Read the buffered file length.
 * @return Number of file bytes, or zero for a closed handle.
 */
size_t ArdFSFile::size() const{if(!_handle)return 0;
 return _handle->data.length();}
/**
 * @brief Read bytes from the file handle and advance its cursor.
 * @param p Source or destination byte pointer.
 * @param n Number of bytes or elements to process.
 * @return Number of bytes read; a negative value indicates failure.
 */
int ArdFSFile::read(uint8_t*p,size_t n){if(!_handle)return -1;
 auto&h=*_handle;n=std::min<size_t>(n,h.data.length()-h.offset);memcpy(p,h.data.c_str()+h.offset,n);h.offset+=n;return n;}
/**
 * @brief Append bytes to the writable file buffer.
 * @param p Source or destination byte pointer.
 * @param n Number of bytes or elements to process.
 * @return Number of bytes appended, or zero when the write cannot be accepted.
 */
size_t ArdFSFile::write(const uint8_t*p,size_t n){if(!_handle||!_handle->writing)return 0;
 if(_handle->data.length()+n>ArdFSMaxJournalBytes)return 0;
 if(!_handle->data.reserve(_handle->data.length()+n))return 0;
 size_t prior=_handle->data.length();for(size_t i=0;i<n;++i)_handle->data+=char(p[i]);if(_handle->data.length()!=prior+n)return 0;_handle->dirty=true;return n;}
/**
 * @brief Commit dirty buffered file contents to the journal.
 * @return True on a successful commit or when no commit is needed; false on failure.
 */
bool ArdFSFile::flush(){if(!_handle)return false;
 if(!_handle->dirty)return true;
 bool ok=_handle->volume->commit(_handle->path,_handle->data);if(ok)_handle->dirty=false;return ok;}
/**
 * @brief Flush the file if needed and release its handle.
 * @return Whether the flush completed successfully.
 */
bool ArdFSFile::close(){if(!_handle)return true;bool ok=true;
 _handle.reset();return ok;}
