#!/usr/bin/env python3
"""Bind only the actual Matter IPv6 UDP port, under TCP/IP synchronization."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'lwip').mkdir()
    (path / 'esp_netif_net_stack.h').write_text(r'''
#pragma once
#include <cstdint>
struct netif { int id; };
struct esp_netif_t { netif* impl; };
extern esp_netif_t* thread;
inline void* esp_netif_get_netif_impl(esp_netif_t* n){return n->impl;}
''')
    (path / 'esp_openthread_netif_glue.h').write_text('inline esp_netif_t* esp_openthread_get_netif(){return thread;}\n')
    (path / 'lwip/tcpip.h').write_text(r'''
#pragma once
using err_t=int;
constexpr err_t ERR_OK=0;
extern bool locked, callbackFails;
inline err_t tcpip_callback_wait(void(*fn)(void*),void* p){
 if(callbackFails)return -1;
 locked=true;fn(p);locked=false;return ERR_OK;
}
''')
    (path / 'lwip/udp.h').write_text(r'''
#pragma once
#include <cassert>
struct ip_addr_t { bool v6; };
struct udp_pcb { udp_pcb* next; uint16_t local_port; ip_addr_t local_ip; netif* bound; };
extern udp_pcb* udp_pcbs;
#define IP_IS_V6(p) ((p)->v6)
inline void udp_bind_netif(udp_pcb* p,const netif* n){assert(locked);p->bound=const_cast<netif*>(n);}
''')
    (path / 'test.cpp').write_text(r'''
#include "MatterThreadTransport.h"
bool locked=false,callbackFails=false;
esp_netif_t* thread=nullptr;
udp_pcb* udp_pcbs=nullptr;
int main(){
 netif ot{1},wifi{2};esp_netif_t adapter{&ot};
 assert(!ArdMatterThreadTransport::bind(5540));thread=&adapter;
 assert(!ArdMatterThreadTransport::bind(0));
 assert(!ArdMatterThreadTransport::bind(5540));
 udp_pcb other{nullptr,123,{true},&wifi};
 udp_pcb ipv4{&other,5540,{false},&wifi};
 udp_pcb matter{&ipv4,5540,{true},nullptr};udp_pcbs=&matter;
 callbackFails=true;assert(!ArdMatterThreadTransport::bind(5540));assert(!matter.bound);
 callbackFails=false;assert(ArdMatterThreadTransport::bind(5540));assert(matter.bound==&ot);
 assert(ipv4.bound==&wifi && other.bound==&wifi);
 assert(ArdMatterThreadTransport::bind(5540));
 matter.local_port=6000;matter.bound=nullptr;
 assert(ArdMatterThreadTransport::bind(6000));assert(matter.bound==&ot);
 adapter.impl=nullptr;assert(!ArdMatterThreadTransport::bind(6000));
}
''')
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(path), '-I' + str(root / 'src'), str(path / 'test.cpp'),
                    '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: Matter Thread UDP binding, IPv4/other ports preserved, SDK errors handled')
