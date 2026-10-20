# ArdPortal — rozmiary według flag kompilacji

Pomiar: 2026-10-20, ArdPortal 0.8.0, Arduino CLI 1.5.1, optymalizacja `-Os`.

Zakres: **55 publicznych flag, 113 profili po optymalizacji**, w tym każdy z 30 typów kontrolek oddzielnie.

ESP8266: `esp8266:esp8266:generic`, core 3.1.2. ESP32: klasyczny `esp32:esp32:esp32`, core 3.3.12. Zigbee dodatkowo na ESP32-C6, core 3.3.12.

## Jak czytać wyniki

- Flash: rozmiar gotowego pliku aplikacji `.bin`. Na ESP32 nie obejmuje oddzielnego bootloadera ani tablicy partycji; na ESP8266 format core zawiera także eboot. Nie obejmuje obszaru ArdFS ani drugiego slotu OTA. Jednostka główna to KiB (1024 B); nawias zawiera dokładny przyrost w bajtach.
- Część kompilacji ESP32 używa `--application-only`: pomija tylko pakowanie bootloadera i obrazu merged. Kompilacja, linkowanie oraz generowanie aplikacji `.bin` pozostają bez zmian.
- RAM: statyczne `.data`/`.bss` oraz obszary zaliczane do RAM przez raport danego core, w bajtach. Na ESP8266 raport obejmuje również stałe w RAM. Nie jest to maksymalne zużycie sterty ani stosu.
- ESP8266 ma również osobny budżet IRAM na kod. Jego zużycie (z rezerwą 32768 B cache) jest zachowane w CSV/JSON jako `iram_used`; główne tabele RAM dotyczą pamięci danych.
- Każdy profil zmienia wskazaną flagę z 0 na 1 względem podanej bazy, z wyjątkiem limitów liczbowych, profilu pełnego i Routera. Przyrostów nie należy sumować: kod i zasoby są współdzielone.
- Wszystkie flagi opcjonalne są jawnie wyłączone w `minimum`. Skrypt zachowuje pełną listę ustawień każdego profilu w JSON. Kontrolki mierzone pojedynczo z WWW, MQTT bez TLS i HA; pozostałe typy są wyłączone.
- Szkic wywołuje `begin()`/`loop()`, a z dynamicznymi stronami rejestruje pustą stronę. Eksportery Zigbee są jawnie wywoływane. Wyniki mierzą dostępny kod, bez danych czujników i bez uruchamiania na sprzęcie.
- Pomiary „przed” dotyczą stanu po wcześniejszych poprawkach odcięcia Wi-Fi i RAM, a „po” dodatkowo optymalizacji TX MQTT i wyłączonej diagnostyki.

## Moduły — wyniki po optymalizacji

| Flaga / profil | Baza | ESP8266 flash KiB (Δ B) | ESP8266 RAM B (Δ) | ESP32 flash KiB (Δ B) | ESP32 RAM B (Δ) |
|---|---|---:|---:|---:|---:|
| Minimum (wszystkie moduły opcjonalne wyłączone) | — | 302.2 | 30436 | 305.1 | 23604 |
| ARDPORTAL_ENABLE_WIFI | minimum | 309.3 (+7296 B) | 30812 (+376) | 920.4 (+630048 B) | 48796 (+25192) |
| ARDPORTAL_ENABLE_WWW | wifi | 345.9 (+37472 B) | 31700 (+888) | 969.6 (+50400 B) | 49068 (+272) |
| ARDPORTAL_ENABLE_OTA | www | 350.8 (+5008 B) | 31800 (+100) | 976.7 (+7296 B) | 49412 (+344) |
| ARDPORTAL_ENABLE_MQTT | www | 351.6 (+5840 B) | 34360 (+2660) | 977.8 (+8352 B) | 51540 (+2472) |
| ARDPORTAL_ENABLE_MQTT_TLS | mqtt | 456.2 (+107136 B) | 34740 (+380) | 1065.7 (+90000 B) | 52052 (+512) |
| ARDPORTAL_ENABLE_CONSOLE | mqtt | 358.5 (+7024 B) | 36388 (+2028) | 987.8 (+10320 B) | 53372 (+1832) |
| ARDPORTAL_ENABLE_CONSOLE_MESSAGES | console | 361.3 (+2848 B) | 37004 (+616) | 992.4 (+4624 B) | 53764 (+392) |
| ARDPORTAL_MQTT_DEBUG_SERIAL | mqtt | 354.9 (+3392 B) | 34632 (+272) | 979.9 (+2208 B) | 51540 (+0) |
| ARDPORTAL_ENABLE_DYNAMIC_PAGES | mqtt | 394.5 (+43888 B) | 36716 (+2356) | 1037.7 (+61392 B) | 52668 (+1128) |
| ARDPORTAL_ENABLE_DEPENDENCIES | dynamic | 401.7 (+7424 B) | 36940 (+224) | 1048.0 (+10544 B) | 52804 (+136) |
| ARDPORTAL_ENABLE_HA | dynamic | 398.2 (+3776 B) | 37112 (+396) | 1044.5 (+6912 B) | 52684 (+16) |
| ARDPORTAL_ENABLE_DEPENDENCIES przy HA=1 | ha | 408.5 (+10624 B) | 38408 (+1296) | 1058.1 (+13936 B) | 53876 (+1192) |
| ARDPORTAL_ENABLE_TEXT_TYPES | control_text | 402.3 (+1136 B) | 38132 (+348) | 1058.1 (+1296 B) | 52684 (+0) |
| ARDPORTAL_ENABLE_CONTROLS | ha | 435.6 (+38352 B) | 39384 (+2272) | 1102.9 (+59792 B) | 52964 (+280) |
| Domyślne moduły IP i wszystkie kontrolki (Messages=0) | controls | 561.7 (+129120 B) | 43084 (+3700) | 1219.8 (+119760 B) | 56580 (+3616) |
| Wszystkie moduły IP i kontrolki + Messages | controls | 566.0 (+133488 B) | 43940 (+4556) | 1226.6 (+126688 B) | 56972 (+4008) |

## Największe koszty i miejsca oszczędności

| Platforma | Flaga / profil | Baza | Przyrost flash B | Przyrost RAM B |
|---|---|---|---:|---:|
| esp8266 | ARDPORTAL_ENABLE_MQTT_TLS | mqtt | +107136 | +380 |
| esp8266 | ARDPORTAL_ENABLE_DYNAMIC_PAGES | mqtt | +43888 | +2356 |
| esp8266 | ARDPORTAL_ENABLE_WWW | wifi | +37472 | +888 |
| esp8266 | ARDPORTAL_ENABLE_DEPENDENCIES przy HA=1 | ha | +10624 | +1296 |
| esp8266 | ARDPORTAL_ENABLE_DEPENDENCIES | dynamic | +7424 | +224 |
| esp32 | ARDPORTAL_ENABLE_WIFI | minimum | +630048 | +25192 |
| esp32 | ARDPORTAL_ENABLE_MQTT_TLS | mqtt | +90000 | +512 |
| esp32 | ARDPORTAL_ENABLE_DYNAMIC_PAGES | mqtt | +61392 | +1128 |
| esp32 | ARDPORTAL_ENABLE_WWW | wifi | +50400 | +272 |
| esp32 | ARDPORTAL_ENABLE_DEPENDENCIES przy HA=1 | ha | +13936 | +1192 |

- Wi-Fi na ESP32 dołącza sterownik i stos IP dostawcy. Wariant bez sieci odcina tę zależność; zachowanie działającego Wi-Fi nadal wymaga tego kodu.
- TLS ma duży koszt flash, a pamięć sesji jest dodatkowo alokowana na stercie. Wyłączaj tę funkcję tylko w profilu świadomie korzystającym z połączeń bez TLS; optymalizacja nie zmienia walidacji certyfikatów.
- MQTT był głównym kandydatem do redukcji statycznego RAM: stały bufor TX zastąpiono pamięcią zależną od oczekującego pakietu. To poprawia stan bezczynny, a nie gwarantuje niższego szczytu podczas dużej transmisji.
- Konsola ma osobne historie i bufory WebSocket. Zależności z HA wymagają dodatkowych masek widoczności; dlatego pokazano także profil `ha_dependencies`.
- Dalsze większe zmiany wymagają osobnych pomiarów sterty przy konkretnych formularzach, TLS i zapisach ArdFS. Limity pól, pojemność pakietu RX i mechanizmy odzyskiwania danych nie zostały zmniejszone.

## Poszczególne kontrolki

Nazwy oznaczają flagi `ARDPORTAL_ENABLE_CONTROL_<NAZWA>`. Baza: `ha` (Wi-Fi, WWW, MQTT, dynamiczne strony i HA; bez TLS, OTA, konsoli, zależności i kontrolek). `TEXT_TYPES=0` również przy kontrolce TEXT; jego oddzielny koszt podano wyżej. Pierwsza kontrolka z danej rodziny dołącza wspólne mechanizmy tej rodziny; włączenie kolejnej nie musi powtarzać całego narzutu.

| NAZWA | ESP8266 flash KiB (Δ B) | ESP8266 RAM B (Δ) | ESP32 flash KiB (Δ B) | ESP32 RAM B (Δ) |
|---|---:|---:|---:|---:|
| SLIDER | 401.8 (+3696 B) | 37832 (+720) | 1057.8 (+13616 B) | 52684 (+0) |
| TEXT | 401.2 (+3072 B) | 37784 (+672) | 1056.8 (+12624 B) | 52684 (+0) |
| SELECT | 402.1 (+4000 B) | 37896 (+784) | 1052.4 (+8144 B) | 52684 (+0) |
| EDIT | 400.7 (+2576 B) | 37752 (+640) | 1050.2 (+5904 B) | 52684 (+0) |
| SWITCH | 401.5 (+3392 B) | 37752 (+640) | 1051.1 (+6816 B) | 52684 (+0) |
| CLIMATE | 411.7 (+13888 B) | 38472 (+1360) | 1070.8 (+26944 B) | 52684 (+0) |
| BINARY_SENSOR | 409.2 (+11280 B) | 38152 (+1040) | 1068.9 (+24992 B) | 52684 (+0) |
| DATE | 408.8 (+10848 B) | 38132 (+1020) | 1068.4 (+24464 B) | 52684 (+0) |
| TIME | 408.7 (+10768 B) | 38132 (+1020) | 1068.3 (+24432 B) | 52684 (+0) |
| DATETIME | 409.4 (+11536 B) | 38136 (+1024) | 1069.1 (+25200 B) | 52684 (+0) |
| BUTTON | 410.1 (+12176 B) | 38360 (+1248) | 1070.6 (+26752 B) | 52964 (+280) |
| SCENE | 410.8 (+12944 B) | 38360 (+1248) | 1071.3 (+27504 B) | 52964 (+280) |
| NOTIFY | 410.1 (+12192 B) | 38360 (+1248) | 1070.6 (+26736 B) | 52964 (+280) |
| INFRARED | 410.3 (+12384 B) | 38356 (+1244) | 1070.8 (+26928 B) | 52964 (+280) |
| EVENT | 409.1 (+11152 B) | 38356 (+1244) | 1069.0 (+25104 B) | 52948 (+264) |
| DEVICE_TRIGGER | 408.9 (+10992 B) | 38328 (+1216) | 1068.9 (+24976 B) | 52948 (+264) |
| TAG | 408.7 (+10816 B) | 38328 (+1216) | 1068.6 (+24720 B) | 52948 (+264) |
| DEVICE_TRACKER | 408.3 (+10384 B) | 38136 (+1024) | 1068.0 (+24048 B) | 52684 (+0) |
| LIGHT | 411.6 (+13792 B) | 38148 (+1036) | 1071.6 (+27792 B) | 52684 (+0) |
| FAN | 411.8 (+13952 B) | 38148 (+1036) | 1071.8 (+28032 B) | 52684 (+0) |
| COVER | 412.8 (+14928 B) | 38356 (+1244) | 1073.4 (+29616 B) | 52964 (+280) |
| VALVE | 412.0 (+14208 B) | 38356 (+1244) | 1072.7 (+28896 B) | 52964 (+280) |
| LOCK | 411.3 (+13424 B) | 38356 (+1244) | 1071.7 (+27904 B) | 52964 (+280) |
| ALARM_CONTROL_PANEL | 411.7 (+13872 B) | 38356 (+1244) | 1072.2 (+28352 B) | 52964 (+280) |
| HUMIDIFIER | 417.9 (+20240 B) | 38148 (+1036) | 1078.0 (+34320 B) | 52684 (+0) |
| SIREN | 411.4 (+13520 B) | 38148 (+1036) | 1071.4 (+27600 B) | 52684 (+0) |
| VACUUM | 413.0 (+15168 B) | 38360 (+1248) | 1073.6 (+29792 B) | 52964 (+280) |
| LAWN_MOWER | 411.4 (+13536 B) | 38356 (+1244) | 1071.8 (+28016 B) | 52964 (+280) |
| WATER_HEATER | 416.9 (+19200 B) | 38152 (+1040) | 1076.8 (+33120 B) | 52684 (+0) |
| UPDATE | 410.2 (+12304 B) | 38356 (+1244) | 1070.6 (+26784 B) | 52964 (+280) |

## Zigbee — ESP32-C6

Na ESP8266 i klasycznym ESP32 natywne Zigbee jest niedostępne: `ARDPORTAL_ENABLE_ZIGBEE=1` jest celowo błędem kompilacji, nie profilem o zerowym koszcie. Wyników C6 nie odejmuje się od wyników klasycznego ESP32.

Profile C6 mają Wi-Fi i MQTT wyłączone. `zigbee_controls` włącza dynamiczne strony i wszystkie kontrolki. Router używa trybu SDK ZCZR, pozostałe ED. Pomiar używa partycji `huge_app` wyłącznie do kompilacji; przed uruchomieniem potrzebny jest właściwy układ partycji Zigbee/ArdFS/OTA.

| Profil | Zmieniana flaga / funkcja | Baza | C6 flash KiB (Δ B) | C6 RAM B (Δ) |
|---|---|---|---:|---:|
| c6_minimum | minimum C6 | — | 293.6 | 15420 |
| zigbee | ARDPORTAL_ENABLE_ZIGBEE=1 | c6_minimum | 587.4 (+300800 B) | 33404 (+17984) |
| zigbee_router | ARDPORTAL_ZIGBEE_ROUTER=1 | zigbee | 700.2 (+115504 B) | 36872 (+3468) |
| zigbee_debug | ARDPORTAL_ZIGBEE_DEBUG_SERIAL=1 | zigbee | 587.5 (+64 B) | 33412 (+8) |
| zigbee_controls | ARDPORTAL_ENABLE_DYNAMIC_PAGES=1, ARDPORTAL_ENABLE_CONTROLS=1 | zigbee | 743.1 (+159456 B) | 34356 (+952) |
| zigbee_ota | ARDPORTAL_ENABLE_OTA=1 | zigbee_controls | 763.0 (+20400 B) | 34476 (+120) |
| zigbee_converter | ARDPORTAL_ENABLE_ZIGBEE_CONVERTER=1 | zigbee_controls | 753.3 (+10416 B) | 34420 (+64) |
| zha_quirk | ARDPORTAL_ENABLE_ZHA_QUIRK=1 | zigbee_controls | 751.6 (+8688 B) | 34420 (+64) |
| zigbee_identity_manufacturer | ARDPORTAL_ZIGBEE_OTA_MANUFACTURER=4660 | zigbee_ota | 763.0 (+0 B) | 34476 (+0) |
| zigbee_identity_image_type | ARDPORTAL_ZIGBEE_OTA_IMAGE_TYPE=2 | zigbee_ota | 763.0 (+0 B) | 34476 (+0) |
| zigbee_identity_file_version | ARDPORTAL_ZIGBEE_OTA_FILE_VERSION=2 | zigbee_ota | 763.0 (+0 B) | 34476 (+0) |

## Limity liczbowe

To ograniczenia i progi bezpieczeństwa, a nie rezerwacje buforów tej wielkości. Zmiana progów na 0 służy wyłącznie porównaniu kompilacji; nie jest zaleceniem obniżenia rezerwy RAM.

| Profil | Zmiana | Baza | ESP8266 flash KiB (Δ B) | ESP8266 RAM B (Δ) | ESP32 flash KiB (Δ B) | ESP32 RAM B (Δ) |
|---|---|---|---:|---:|---:|---:|
| definition_limit | ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES=131072 | dynamic | 394.5 (+0 B) | 36716 (+0) | 1037.7 (+0 B) | 52668 (+0) |
| reserve_heap | ARDPORTAL_NETWORK_HEAP_RESERVE=0 | www | 345.9 (+0 B) | 31700 (+0) | 969.6 (+0 B) | 49068 (+0) |
| reserve_dma | ARDPORTAL_NETWORK_DMA_RESERVE=0 | www | 345.9 (+0 B) | 31700 (+0) | 969.6 (+0 B) | 49068 (+0) |
| reserve_block | ARDPORTAL_NETWORK_BLOCK_RESERVE=0 | www | 345.9 (+0 B) | 31700 (+0) | 969.6 (+0 B) | 49068 (+0) |

## Efekty optymalizacji

TX MQTT przechowuje teraz tylko aktualnie oczekujący pakiet zamiast stałego bufora 2048 B. Bufor zostaje zwolniony po transmisji lub rozłączeniu. RX i maksymalny rozmiar pakietu pozostają 2048 B. Podczas oczekiwania na duży pakiet TX oszczędność maleje: pakiet zajmuje stertę i ma narzut alokatora. Nie jest to usunięcie kosztu pamięci transmisji.

Przy wyłączonych Messages część wewnętrznych komunikatów nie jest już formatowana ani tłumaczona. Wyłączone logowanie nie powinno wymuszać tworzenia tymczasowych Stringów.

| Platforma / profil | Flash przed → po (B) | Oszczędność flash B | RAM przed → po (B) | Oszczędność statycznego RAM B |
|---|---:|---:|---:|---:|
| esp8266 / minimum | 309680 → 309456 | +224 | 30452 → 30436 | +16 |
| esp8266 / www | 354752 → 354224 | +528 | 31700 → 31700 | +0 |
| esp8266 / mqtt | 360384 → 360064 | +320 | 36392 → 34360 | +2032 |
| esp8266 / mqtt_tls | 467584 → 467200 | +384 | 36772 → 34740 | +2032 |
| esp8266 / dynamic | 404608 → 403952 | +656 | 38804 → 36716 | +2088 |
| esp8266 / controls | 446752 → 446080 | +672 | 41464 → 39384 | +2080 |
| esp8266 / full | 579472 → 579568 | -96 | 45972 → 43940 | +2032 |
| esp32 / minimum | 312784 → 312432 | +352 | 23604 → 23604 | +0 |
| esp32 / www | 994064 → 992880 | +1184 | 49068 → 49068 | +0 |
| esp32 / mqtt | 1002144 → 1001232 | +912 | 53572 → 51540 | +2032 |
| esp32 / mqtt_tls | 1092160 → 1091232 | +928 | 54084 → 52052 | +2032 |
| esp32 / dynamic | 1063760 → 1062624 | +1136 | 54700 → 52668 | +2032 |
| esp32 / controls | 1130592 → 1129328 | +1264 | 54996 → 52964 | +2032 |
| esp32 / full | 1255712 → 1256016 | -304 | 59004 → 56972 | +2032 |

## Odtwarzanie

Skrypt `tests/measure-features.py` generuje szkice, zapisuje komplet flag, logi i mapy linkera. Uruchom osobno dla `esp8266`, `esp32` i `esp32c6`:

```sh
python3 tests/measure-features.py --platform esp8266 --output /tmp/ardportal-sizes --cli /path/to/arduino-cli
python3 tests/measure-features.py --platform esp32 --output /tmp/ardportal-sizes --cli /path/to/arduino-cli
python3 tests/measure-features.py --platform esp32c6 --output /tmp/ardportal-sizes --cli /path/to/arduino-cli
```

Dane źródłowe: [CSV](feature-sizes.csv), [JSON z flagami](feature-sizes.json). Użyj `--only PROFIL...` do powtórzenia wybranych przypadków albo `--resume` do wznowienia macierzy.
