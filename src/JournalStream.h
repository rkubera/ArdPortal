// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// Bounded scanner for canonical ArdFS envelopes; legacy envelopes use the normal reader.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
struct ArdJournalStream {
 char header[128]={};uint8_t headerSize=0,state=0,hexLeft=0,utfLeft=0;
 uint16_t unicode=0,high=0;uint32_t utfCode=0,utfMin=0,generation=0,expectedCrc=0,crc=UINT32_MAX;size_t length=0;
 bool canonical=false,failed=false;
 static uint32_t update(uint32_t crc,uint8_t b){crc^=b;for(uint8_t i=0;i<8;++i)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1)));return crc;}
 static bool literal(const char*& at,const char* text){size_t n=strlen(text);if(strncmp(at,text,n))return false;at+=n;return true;}
 static bool number(const char*& at,uint32_t& value){
  if(*at<'0'||*at>'9')return false;bool zero=*at=='0';value=0;
  do{uint8_t digit=*at-'0';if(value>(UINT32_MAX-digit)/10)return false;value=value*10+digit;++at;if(zero&&*at>='0'&&*at<='9')return false;}while(*at>='0'&&*at<='9');return true;
 }
 bool valid()const{return canonical&&!failed&&state==4&&!utfLeft&&!high&&~crc==expectedCrc;}
 template<class Emit> bool output(uint8_t b,Emit& emit){
  if(utfLeft){if((b&0xc0)!=0x80)return false;utfCode=(utfCode<<6)|(b&63);if(!--utfLeft&&(utfCode<utfMin||utfCode>0x10ffff||(utfCode>=0xd800&&utfCode<=0xdfff)))return false;}
  else if(b>=0x80){if(b>=0xc2&&b<=0xdf){utfLeft=1;utfCode=b&31;utfMin=0x80;}else if(b>=0xe0&&b<=0xef){utfLeft=2;utfCode=b&15;utfMin=0x800;}else if(b>=0xf0&&b<=0xf4){utfLeft=3;utfCode=b&7;utfMin=0x10000;}else return false;}
  crc=update(crc,b);return emit(b,length++);
 }
 template<class Emit> bool codepoint(uint32_t cp,Emit& emit){
  if(cp<0x80)return output(cp,emit);
  if(cp<0x800)return output(0xc0|(cp>>6),emit)&&output(0x80|(cp&63),emit);
  if(cp<0x10000)return output(0xe0|(cp>>12),emit)&&output(0x80|((cp>>6)&63),emit)&&output(0x80|(cp&63),emit);
  return output(0xf0|(cp>>18),emit)&&output(0x80|((cp>>12)&63),emit)&&output(0x80|((cp>>6)&63),emit)&&output(0x80|(cp&63),emit);
 }
 template<class Emit> void input(uint8_t b,Emit& emit){
  if(failed)return;
  if(state==0){
   if(headerSize>=sizeof(header)-1){failed=true;return;}header[headerSize++]=char(b);header[headerSize]=0;
   static const char signature[]="{\"journal\":1,";
   if(headerSize<=sizeof(signature)-1&&memcmp(header,signature,headerSize)){failed=true;return;}
   if(headerSize<9||memcmp(header+headerSize-9,",\"data\":\"",9))return;
   uint32_t gen=0,checksum=0;const char* at=header;
   if(!literal(at,"{\"journal\":1,\"generation\":")||!number(at,gen)||!literal(at,",\"crc32\":")||!number(at,checksum)||!literal(at,",\"data\":\"")||*at){failed=true;return;}
   generation=gen;expectedCrc=checksum;canonical=true;state=1;
   for(uint8_t i=0;i<4;++i)crc=update(crc,uint8_t(generation>>(8*i)));return;
  }
  if(state==3){if(b=='}')state=4;else if(b!=32&&b!=9&&b!=10&&b!=13)failed=true;return;}
  if(state==4){if(b!=32&&b!=9&&b!=10&&b!=13)failed=true;return;}
  if(hexLeft){int digit=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:b>='A'&&b<='F'?b-'A'+10:-1;
   if(digit<0){failed=true;return;}unicode=(unicode<<4)|digit;
   if(!--hexLeft){state=1;if(high){if(unicode<0xdc00||unicode>0xdfff){failed=true;return;}uint32_t cp=0x10000+((uint32_t(high)-0xd800)<<10)+unicode-0xdc00;high=0;if(!codepoint(cp,emit))failed=true;}
    else if(unicode>=0xd800&&unicode<=0xdbff)high=unicode;
    else if((unicode>=0xdc00&&unicode<=0xdfff)||!codepoint(unicode,emit))failed=true;}
   return;
  }
  if(state==2){state=1;if(high&&b!='u'){failed=true;return;}if(b=='u'){unicode=0;hexLeft=4;return;}
   uint8_t decoded=b=='b'?8:b=='f'?12:b=='n'?10:b=='r'?13:b=='t'?9:b;
   if(!(b=='b'||b=='f'||b=='n'||b=='r'||b=='t'||b=='"'||b=='\\'||b=='/')||!output(decoded,emit))failed=true;return;
  }
  if(high&&b!='\\'){failed=true;return;}
  if(b=='"'){if(high||utfLeft)failed=true;else state=3;return;}
  if(b=='\\'){state=2;return;}
  if(b<32||!output(b,emit))failed=true;
 }
};
