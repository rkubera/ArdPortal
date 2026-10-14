// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
// Parse a single standard Upgrade Image subelement, including split headers.
class ArdZigbeeOtaImage {
  uint8_t _header[6]={},_headerBytes=0;
  uint32_t _expected=0,_received=0;
  bool _failed=false;
public:
  /** @brief Reset parser for a new download. @return No value. */
  void reset(){_headerBytes=0;_expected=_received=0;_failed=false;}
  /** @brief Decode a fragment and pass only firmware bytes to the writer. @param data Wire bytes. @param bytes Fragment length. @param limit OTA slot size. @param write Returns false on flash failure. @return True if accepted. */
  template<class Writer> bool feed(const uint8_t* data,size_t bytes,uint32_t limit,Writer write){
    if(_failed||(!data&&bytes))return false;
    while(_headerBytes<6&&bytes){_header[_headerBytes++]=*data++;--bytes;}
    if(_headerBytes==6&&!_expected){
      _expected=uint32_t(_header[2])|(uint32_t(_header[3])<<8)|(uint32_t(_header[4])<<16)|(uint32_t(_header[5])<<24);
      if(_header[0]||_header[1]||!_expected||_expected>limit){_failed=true;return false;}
    }
    if(bytes){if(bytes>_expected-_received||!write(data,bytes)){_failed=true;return false;}_received+=bytes;}
    return true;
  }
  /** @brief Check that exactly one complete image was received. @return True for a complete nonfailed download. */
  bool complete()const{return !_failed&&_headerBytes==6&&_expected&&_received==_expected;}
};
