#!/usr/bin/env python3
"""Render the measured feature matrix as Markdown, CSV and JSON."""
import argparse
import csv
import datetime
import json
import runpy
from pathlib import Path

MODULES = {
    'minimum': 'Minimum (wszystkie moduły opcjonalne wyłączone)',
    'wifi': 'ARDPORTAL_ENABLE_WIFI', 'www': 'ARDPORTAL_ENABLE_WWW',
    'ota': 'ARDPORTAL_ENABLE_OTA', 'mqtt': 'ARDPORTAL_ENABLE_MQTT',
    'mqtt_tls': 'ARDPORTAL_ENABLE_MQTT_TLS', 'console': 'ARDPORTAL_ENABLE_CONSOLE',
    'messages': 'ARDPORTAL_ENABLE_CONSOLE_MESSAGES',
    'mqtt_debug': 'ARDPORTAL_MQTT_DEBUG_SERIAL',
    'dynamic': 'ARDPORTAL_ENABLE_DYNAMIC_PAGES',
    'dependencies': 'ARDPORTAL_ENABLE_DEPENDENCIES', 'ha': 'ARDPORTAL_ENABLE_HA',
    'ha_dependencies': 'ARDPORTAL_ENABLE_DEPENDENCIES przy HA=1',
    'text_types': 'ARDPORTAL_ENABLE_TEXT_TYPES', 'controls': 'ARDPORTAL_ENABLE_CONTROLS',
    'default': 'Domyślne moduły IP i wszystkie kontrolki (Messages=0)',
    'full': 'Wszystkie moduły IP i kontrolki + Messages',
}


def load(directory):
    return {platform: {row['name']: row for path in sorted((directory / platform).glob('*.json'))
                      if path.name != 'profiles.json' for row in [json.loads(path.read_text())]}
            for platform in ['esp8266', 'esp32', 'esp32c6']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--date', default=datetime.date.today().isoformat())
    parser.add_argument('--allow-incomplete', action='store_true', help='Preview only; mark pending/failed profiles explicitly')
    args = parser.parse_args()
    before, after = load(args.before), load(args.after)
    definitions = runpy.run_path(str(Path(__file__).with_name('measure-features.py')), run_name='measure_definitions')
    if not args.allow_incomplete:
        for platform in ['esp8266', 'esp32', 'esp32c6']:
            expected = definitions['zigbee_profiles']() if platform == 'esp32c6' else definitions['profiles']()
            invalid = [row['name'] for row in expected if not after[platform].get(row['name'], {}).get('ok')]
            if invalid:
                parser.error(platform + ': missing or failed profiles: ' + ', '.join(invalid))
    args.output.mkdir(parents=True, exist_ok=True)
    all_rows = []
    for phase, records in [('before', before), ('after', after)]:
        for platform, profiles in records.items():
            for name, row in profiles.items():
                parent = profiles.get(row.get('parent'))
                clean = {key: value for key, value in row.items() if key not in ['command', 'seconds']}
                if row['ok']:
                    for metric in ['flash_bin', 'ram_static']:
                        clean['delta_' + metric] = row[metric] - parent[metric] if parent and parent['ok'] else None
                all_rows.append(dict(phase=phase, **clean))
    (args.output / 'feature-sizes.json').write_text(json.dumps(all_rows, indent=2) + '\n')
    columns = ['phase', 'platform', 'name', 'parent', 'ok', 'flash_bin', 'delta_flash_bin', 'ram_static', 'delta_ram_static', 'iram_used', 'fqbn']
    with (args.output / 'feature-sizes.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, columns, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(all_rows)

    def cell(platform, name):
        profiles = after[platform]
        row = profiles.get(name)
        if not row:
            return ['BRAK POMIARU', 'BRAK POMIARU']
        if not row['ok']:
            return ['BŁĄD KOMPILACJI', 'BŁĄD KOMPILACJI']
        parent = profiles.get(row.get('parent'))
        flash = f"{row['flash_bin'] / 1024:.1f}"
        ram = str(row['ram_static'])
        if parent and parent['ok']:
            flash += f" ({row['flash_bin'] - parent['flash_bin']:+d} B)"
            ram += f" ({row['ram_static'] - parent['ram_static']:+d})"
        return [flash, ram]

    measured_flags = {key[1:] if key.startswith('@') else 'ARDPORTAL_ENABLE_' + key
                      for profiles in after.values() for row in profiles.values() for key in row['flags']}
    measured_profiles = sum(len(profiles) for profiles in after.values())
    lines = ['# ArdPortal — rozmiary według flag kompilacji', '',
        f'Pomiar: {args.date}, ArdPortal 0.8.0, Arduino CLI 1.5.1, optymalizacja `-Os`.', '',
        f'Zakres: **{len(measured_flags)} publicznych flag, {measured_profiles} profili po optymalizacji**, '
        'w tym każdy z 30 typów kontrolek oddzielnie.', '',
        'ESP8266: `esp8266:esp8266:generic`, core 3.1.2. ESP32: klasyczny '
        '`esp32:esp32:esp32`, core 3.3.12. Zigbee dodatkowo na ESP32-C6, core 3.3.12.', '',
        '## Jak czytać wyniki', '',
        '- Flash: rozmiar gotowego pliku aplikacji `.bin`. Na ESP32 nie obejmuje oddzielnego '
        'bootloadera ani tablicy partycji; na ESP8266 format core zawiera także eboot. '
        'Nie obejmuje obszaru ArdFS ani drugiego slotu OTA. Jednostka główna to KiB (1024 B); '
        'nawias zawiera dokładny przyrost w bajtach.',
        '- Część kompilacji ESP32 używa `--application-only`: pomija tylko pakowanie bootloadera '
        'i obrazu merged. Kompilacja, linkowanie oraz generowanie aplikacji `.bin` pozostają bez zmian.',
        '- RAM: statyczne `.data`/`.bss` oraz obszary zaliczane do RAM przez raport danego core, w bajtach. '
        'Na ESP8266 raport obejmuje również stałe w RAM. Nie jest to maksymalne zużycie sterty ani stosu.',
        '- ESP8266 ma również osobny budżet IRAM na kod. Jego zużycie (z rezerwą 32768 B cache) '
        'jest zachowane w CSV/JSON jako `iram_used`; główne tabele RAM dotyczą pamięci danych.',
        '- Każdy profil zmienia wskazaną flagę z 0 na 1 względem podanej bazy, z wyjątkiem '
        'limitów liczbowych, profilu pełnego i Routera. Przyrostów nie należy sumować: kod i zasoby są współdzielone.',
        '- Wszystkie flagi opcjonalne są jawnie wyłączone w `minimum`. Skrypt zachowuje pełną listę '
        'ustawień każdego profilu w JSON. Kontrolki mierzone pojedynczo z WWW, MQTT bez TLS i HA; pozostałe typy są wyłączone.',
        '- Szkic wywołuje `begin()`/`loop()`, a z dynamicznymi stronami rejestruje pustą stronę. '
        'Eksportery Zigbee są jawnie wywoływane. Wyniki mierzą dostępny kod, bez danych czujników i bez uruchamiania na sprzęcie.',
        '- Pomiary „przed” dotyczą stanu po wcześniejszych poprawkach odcięcia Wi-Fi i RAM, '
        'a „po” dodatkowo optymalizacji TX MQTT i wyłączonej diagnostyki.', '',
        '## Moduły — wyniki po optymalizacji', '',
        '| Flaga / profil | Baza | ESP8266 flash KiB (Δ B) | ESP8266 RAM B (Δ) | ESP32 flash KiB (Δ B) | ESP32 RAM B (Δ) |',
        '|---|---|---:|---:|---:|---:|']
    for name, label in MODULES.items():
        parent = after['esp8266'].get(name, {}).get('parent') or '—'
        lines.append('| ' + ' | '.join([label, parent, *cell('esp8266', name), *cell('esp32', name)]) + ' |')
    lines += ['', '## Największe koszty i miejsca oszczędności', '',
              '| Platforma | Flaga / profil | Baza | Przyrost flash B | Przyrost RAM B |',
              '|---|---|---|---:|---:|']
    for platform in ['esp8266', 'esp32']:
        profiles = after[platform]
        candidates = []
        for name in MODULES:
            if name in ['minimum', 'full', 'default', 'controls']:
                continue
            row = profiles.get(name)
            parent = profiles.get(row.get('parent')) if row else None
            if row and parent and row['ok'] and parent['ok']:
                candidates.append((row['flash_bin'] - parent['flash_bin'], name,
                                   row['ram_static'] - parent['ram_static'], row['parent']))
        for flash, name, ram, parent in sorted(candidates, reverse=True)[:5]:
            lines.append(f'| {platform} | {MODULES[name]} | {parent} | {flash:+d} | {ram:+d} |')
    lines += ['',
              '- Wi-Fi na ESP32 dołącza sterownik i stos IP dostawcy. Wariant bez sieci '
              'odcina tę zależność; zachowanie działającego Wi-Fi nadal wymaga tego kodu.',
              '- TLS ma duży koszt flash, a pamięć sesji jest dodatkowo alokowana na stercie. '
              'Wyłączaj tę funkcję tylko w profilu świadomie korzystającym z połączeń bez TLS; '
              'optymalizacja nie zmienia walidacji certyfikatów.',
              '- MQTT był głównym kandydatem do redukcji statycznego RAM: stały bufor TX '
              'zastąpiono pamięcią zależną od oczekującego pakietu. To poprawia stan bezczynny, '
              'a nie gwarantuje niższego szczytu podczas dużej transmisji.',
              '- Konsola ma osobne historie i bufory WebSocket. Zależności z HA wymagają '
              'dodatkowych masek widoczności; dlatego pokazano także profil `ha_dependencies`.',
              '- Dalsze większe zmiany wymagają osobnych pomiarów sterty przy konkretnych '
              'formularzach, TLS i zapisach ArdFS. Limity pól, pojemność pakietu RX i mechanizmy '
              'odzyskiwania danych nie zostały zmniejszone.']
    lines += ['', '## Poszczególne kontrolki', '',
              'Nazwy oznaczają flagi `ARDPORTAL_ENABLE_CONTROL_<NAZWA>`. Baza: `ha` '
              '(Wi-Fi, WWW, MQTT, dynamiczne strony i HA; bez TLS, OTA, konsoli, zależności i kontrolek). '
              '`TEXT_TYPES=0` również przy kontrolce TEXT; jego oddzielny koszt podano wyżej. '
              'Pierwsza kontrolka z danej rodziny dołącza wspólne mechanizmy tej rodziny; '
              'włączenie kolejnej nie musi powtarzać całego narzutu.', '',
              '| NAZWA | ESP8266 flash KiB (Δ B) | ESP8266 RAM B (Δ) | ESP32 flash KiB (Δ B) | ESP32 RAM B (Δ) |',
              '|---|---:|---:|---:|---:|']
    for control in definitions['CONTROLS']:
        name = 'control_' + control.lower()
        lines.append('| ' + ' | '.join([control, *cell('esp8266', name), *cell('esp32', name)]) + ' |')
    lines += ['', '## Zigbee — ESP32-C6', '',
              'Na ESP8266 i klasycznym ESP32 natywne Zigbee jest niedostępne: '
              '`ARDPORTAL_ENABLE_ZIGBEE=1` jest celowo błędem kompilacji, nie profilem o zerowym koszcie. '
              'Wyników C6 nie odejmuje się od wyników klasycznego ESP32.', '',
              'Profile C6 mają Wi-Fi i MQTT wyłączone. `zigbee_controls` włącza dynamiczne '
              'strony i wszystkie kontrolki. Router używa trybu SDK ZCZR, pozostałe ED. '
              'Pomiar używa partycji `huge_app` wyłącznie do kompilacji; przed uruchomieniem '
              'potrzebny jest właściwy układ partycji Zigbee/ArdFS/OTA.', '',
              '| Profil | Zmieniana flaga / funkcja | Baza | C6 flash KiB (Δ B) | C6 RAM B (Δ) |',
              '|---|---|---|---:|---:|']
    for profile in definitions['zigbee_profiles']():
        name = profile['name']
        row = after['esp32c6'].get(name)
        if not row:
            continue
        parent = after['esp32c6'].get(row.get('parent'))
        changes = ', '.join((key[1:] if key.startswith('@') else 'ARDPORTAL_ENABLE_' + key) + '=' + str(value)
                            for key, value in row['flags'].items() if not parent or parent['flags'].get(key) != value)
        if not parent:
            changes = 'minimum C6'
        lines.append('| ' + ' | '.join([name, changes, row.get('parent') or '—', *cell('esp32c6', name)]) + ' |')
    lines += ['', '## Limity liczbowe', '',
              'To ograniczenia i progi bezpieczeństwa, a nie rezerwacje buforów tej wielkości. '
              'Zmiana progów na 0 służy wyłącznie porównaniu kompilacji; nie jest zaleceniem obniżenia rezerwy RAM.', '',
              '| Profil | Zmiana | Baza | ESP8266 flash KiB (Δ B) | ESP8266 RAM B (Δ) | ESP32 flash KiB (Δ B) | ESP32 RAM B (Δ) |',
              '|---|---|---|---:|---:|---:|---:|']
    for name in ['definition_limit', 'reserve_heap', 'reserve_dma', 'reserve_block']:
        row = after['esp8266'].get(name, {})
        change = 'ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES=131072' if name == 'definition_limit' else 'ARDPORTAL_NETWORK_' + name.removeprefix('reserve_').upper() + '_RESERVE=0'
        lines.append('| ' + ' | '.join([name, change, row.get('parent') or '—', *cell('esp8266', name), *cell('esp32', name)]) + ' |')
    lines += ['', '## Efekty optymalizacji', '',
              'TX MQTT przechowuje teraz tylko aktualnie oczekujący pakiet zamiast stałego bufora 2048 B. '
              'Bufor zostaje zwolniony po transmisji lub rozłączeniu. RX i maksymalny rozmiar pakietu '
              'pozostają 2048 B. Podczas oczekiwania na duży pakiet TX oszczędność maleje: '
              'pakiet zajmuje stertę i ma narzut alokatora. Nie jest to usunięcie kosztu pamięci transmisji.', '',
              'Przy wyłączonych Messages część wewnętrznych komunikatów nie jest już formatowana '
              'ani tłumaczona. Wyłączone logowanie nie powinno wymuszać tworzenia tymczasowych Stringów.', '',
              '| Platforma / profil | Flash przed → po (B) | Oszczędność flash B | RAM przed → po (B) | Oszczędność statycznego RAM B |',
              '|---|---:|---:|---:|---:|']
    for platform in ['esp8266', 'esp32']:
        for name in ['minimum', 'www', 'mqtt', 'mqtt_tls', 'dynamic', 'controls', 'full']:
            old, new = before[platform].get(name), after[platform].get(name)
            if old and new and old['ok'] and new['ok']:
                lines.append(f"| {platform} / {name} | {old['flash_bin']} → {new['flash_bin']} | {old['flash_bin'] - new['flash_bin']:+d} | {old['ram_static']} → {new['ram_static']} | {old['ram_static'] - new['ram_static']:+d} |")
    lines += ['', '## Odtwarzanie', '',
              'Skrypt `tests/measure-features.py` generuje szkice, zapisuje komplet flag, '
              'logi i mapy linkera. Uruchom osobno dla `esp8266`, `esp32` i `esp32c6`:', '',
              '```sh', 'python3 tests/measure-features.py --platform esp8266 --output /tmp/ardportal-sizes --cli /path/to/arduino-cli',
              'python3 tests/measure-features.py --platform esp32 --output /tmp/ardportal-sizes --cli /path/to/arduino-cli',
              'python3 tests/measure-features.py --platform esp32c6 --output /tmp/ardportal-sizes --cli /path/to/arduino-cli', '```', '',
              'Dane źródłowe: [CSV](feature-sizes.csv), [JSON z flagami](feature-sizes.json). '
              'Użyj `--only PROFIL...` do powtórzenia wybranych przypadków albo `--resume` do wznowienia macierzy.', '']
    (args.output / 'Feature-size-report.md').write_text('\n'.join(lines))
    print(args.output / 'Feature-size-report.md')


if __name__ == '__main__':
    main()
