#!/usr/bin/env python3
"""Exercise bounded Matter URL decoding and matrix serialization with an SDK stub."""
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
header=r'''
#pragma once
#include <cstdint>
using esp_err_t=int;
using esp_qrcode_handle_t=const uint8_t*;
struct esp_qrcode_config_t {void(*display_func)(esp_qrcode_handle_t);int max_qrcode_version;int qrcode_ecc_level;};
constexpr int ESP_OK=0,ESP_QRCODE_ECC_MED=1;
int esp_qrcode_get_size(esp_qrcode_handle_t);
bool esp_qrcode_get_module(esp_qrcode_handle_t,int,int);
esp_err_t esp_qrcode_generate(esp_qrcode_config_t*,const char*);
'''
native=(root/'src/MatterNativeImpl.h').read_text()
a=native.index('inline String ArdMatter::manualPairingCode()')
b=native.index('inline bool ArdMatter::openCommissioningWindow',a)
getters=native[a:b]
fixture=r'''
#include <cassert>
#include <string>
#include "MatterQr.h"
std::string payload;bool failed=false;
int esp_qrcode_get_size(esp_qrcode_handle_t){return 21;}
bool esp_qrcode_get_module(esp_qrcode_handle_t,int x,int y){return (x+y)%2==0;}
esp_err_t esp_qrcode_generate(esp_qrcode_config_t* cfg,const char* text){payload=text;assert(cfg->max_qrcode_version==10);assert(cfg->qrcode_ecc_level==ESP_QRCODE_ECC_MED);if(failed)return -1;cfg->display_func(nullptr);return ESP_OK;}
struct ArdMatter { bool _running=false; String manualPairingCode() const; String onboardingUrl() const; };
struct MockMatter {String getManualPairingCode(){return "34970112332";}String getOnboardingQRCodeUrl(){return "https://example.test/?data=MT%3AX";}} Matter;
GETTERS
int main(){
 ArdMatter matter;assert(!matter.manualPairingCode().length());assert(!matter.onboardingUrl().length());
 matter._running=true;assert(matter.manualPairingCode()=="34970112332");assert(matter.onboardingUrl().length());
 String result=ArdMatterQr::generate("https://example.test/?data=MT%3ATEST%2BABC&ignored=1");
 assert(payload=="MT:TEST+ABC");
 const std::string encoded=result.c_str();
 const std::string prefix="{\"size\":21,\"bits\":\"";
 assert(encoded.substr(0,prefix.size())==prefix);assert(encoded.size()==prefix.size()+111+2);
 for(int i=0;i<110;i++)assert(encoded[prefix.size()+i]=='a');assert(encoded[prefix.size()+110]=='8');
 assert(ArdMatterQr::output==nullptr);
 assert(!ArdMatterQr::generate("https://example.test/?data=MT%3AX%").length());
 assert(!ArdMatterQr::generate("https://example.test/?data=HTTP%3AX").length());
 assert(!ArdMatterQr::generate("https://example.test/").length());
 failed=true;assert(!ArdMatterQr::generate("https://example.test/?data=MT%3AX").length());assert(ArdMatterQr::output==nullptr);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-qr-') as directory:
 p=Path(directory);(p/'qrcode.h').write_text(header);(p/'test.cpp').write_text(fixture.replace('GETTERS',getters))
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: Matter QR URL decoding, matrix bit order, padding and generation failure')
