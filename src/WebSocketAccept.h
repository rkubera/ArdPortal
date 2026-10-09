// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
// RFC 6455 handshake, bounded to the 24-byte base64 key plus the fixed GUID.
namespace ArdPortalWS {
/**
 * @brief Rotate a 32-bit word left for the SHA-1 calculation.
 * @param v Word to rotate.
 * @param n Rotation count, from 1 to 31.
 * @return Word with its bits rotated left by n positions.
 */
inline uint32_t rotate(uint32_t v, unsigned n) { return (v << n) | (v >> (32-n)); }
/**
 * @brief Compute the RFC 6455 Sec-WebSocket-Accept handshake value.
 * @param key 24-byte Sec-WebSocket-Key header value.
 * @return Base64-encoded SHA-1 result, or an empty string if the key length is invalid.
 */
inline String accept(const String& key) {
  if (key.length() != 24) return "";
  String input = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t blocks[64] = {}; memcpy(blocks, input.c_str(), input.length());
  blocks[input.length()] = 0x80;
  uint32_t h[5] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
  // 60 bytes requires two SHA-1 blocks; only the first contains input data.
  for (unsigned block=0; block<2; ++block) {
    if (block) { memset(blocks,0,64); blocks[62]=1; blocks[63]=0xe0; }
    uint32_t w[80];
    for(unsigned i=0;i<16;++i) w[i]=(uint32_t(blocks[i*4])<<24)|(uint32_t(blocks[i*4+1])<<16)|(uint32_t(blocks[i*4+2])<<8)|blocks[i*4+3];
    for(unsigned i=16;i<80;++i) w[i]=rotate(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
    for(unsigned i=0;i<80;++i) {
      uint32_t f,k;
      if(i<20){f=(b&c)|(~b&d);k=0x5a827999;}else if(i<40){f=b^c^d;k=0x6ed9eba1;}else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}else{f=b^c^d;k=0xca62c1d6;}
      uint32_t next=rotate(a,5)+f+e+k+w[i];e=d;d=c;c=rotate(b,30);b=a;a=next;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
  }
  uint8_t digest[20];for(unsigned i=0;i<20;++i)digest[i]=h[i/4]>>(24-8*(i%4));
  const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;for(unsigned i=0;i<20;i+=3){uint32_t v=uint32_t(digest[i])<<16;if(i+1<20)v|=uint32_t(digest[i+1])<<8;if(i+2<20)v|=digest[i+2];out+=alphabet[(v>>18)&63];out+=alphabet[(v>>12)&63];out+=i+1<20?alphabet[(v>>6)&63]:'=';out+=i+2<20?alphabet[v&63]:'=';}return out;
}
}
