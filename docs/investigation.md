# Sonoff DW2-Wi-Fi → integrare locală cu ESP32 (fără eWeLink)

*Rezumat sesiune de investigație — 12 noiembrie 2026*

## 1. Obiectiv

Senzorul de ușă/geam Sonoff DW2-Wi-Fi trebuie să comunice **direct cu un ESP32 WROOM**, fără eWeLink și fără cloud, pentru proiectul personal **smart-heating** (controller de încălzire multi-zonă pe ESP32, repo `github.com/RoDTOX/smart-home-heating`). Rolul senzorului în proiect: detectează geam/ușă deschisă și oprește încălzirea în zona respectivă.

## 2. Ce am aflat despre dispozitiv

### 2.1 Specificații (manual + pagina oficială)

| Parametru | Valoare |
|---|---|
| Alimentare | 2 × AAA (3 V) |
| Curent în repaus (sleep) | ≤ 40 µA |
| Curent la transmisie | ≤ 15 mA |
| Wi-Fi | doar 2.4 GHz, 802.11b |
| BLE | 2402–2480 MHz, doar pentru configurare |
| Declanșare | magnet îndepărtat > ~20 mm |
| Distanță de montaj | < 5 mm când e închis |
| FCC ID | 2APN5DW2-WIFI (două certificări: mai 2020 și aprilie 2021) |

### 2.2 Cum funcționează cu firmware-ul original

- **Bluetooth-ul e folosit doar la pairing.** Prin el, aplicația îi trimite numele și parola Wi-Fi.
- **Evenimentele ușii merg doar pe Wi-Fi, la cloud-ul eWeLink/CoolKit.** Nu le trimite nici prin BLE, nici pe rețeaua locală.
- **Ciclul de funcționare:** doarme → magnetul se mișcă și îl trezește → se conectează la Wi-Fi → trimite la cloud → adoarme la loc.
- **Probleme cunoscute:** pairing-ul eșuează (eroarea 6315) dacă routerul nu are activat 802.11b pe 2.4 GHz. Unii utilizatori raportează baterii descărcate repede și reconectare ratată după o cădere a Wi-Fi-ului.

### 2.3 Test BLE făcut de pe laptop (verificat)

Cu senzorul în modul pairing (buton apăsat 5 s), scanarea BLE l-a găsit:

| Câmp | Valoare |
|---|---|
| Adresă MAC | `88:4A:18:EA:CD:A0` |
| Nume | `ck_1002873d3e` |
| Serviciu BLE | `0xAAAA` (nestandard, propriu) |
| Putere emisie | 0 dBm |
| Semnal lângă laptop | −51 … −74 dBm |
| Interval anunțuri | ~2–4 s |
| Date de producător | niciuna (nu trimite starea ușii) |

- Prefixul `ck_` vine probabil de la CoolKit, firma din spatele eWeLink. `1002873d3e` pare să fie ID-ul dispozitivului în eWeLink. *Ambele sunt deducții, neconfirmate.*
- Primul test a ratat senzorul. Cauza probabilă: scriptul l-a pus între dispozitivele deja existente din primele 8 s. Am corectat scriptul să afișeze orice dispozitiv apropiat (semnal peste −75), indiferent când apare.

**Concluzie:** cu firmware-ul original, **nu se poate** comunica direct cu un ESP32. Configurarea prin BLE e nedocumentată, iar după ea senzorul tot la cloud trimite.

## 3. Hardware: placa DW2-WIFI V1.2 (2021.01.13)

| Componentă | Identificare | Rol |
|---|---|---|
| **U1** | **Opulinks OPL1000 A2** (`OPL1000 A2KA2205`) | Cip Wi-Fi + BLE. Aplicația rulează pe un nucleu ARM Cortex-M3 |
| **U2** | BoyaMicro 25Q80 | Memorie flash SPI de **1 MB**, cu firmware-ul |
| **J2** | `3V3 · GND · RXD · TXD` | **Port serial (UART), cu pinii marcați**. Pe aici se scrie firmware-ul |
| Puncte de test | `RST`, `ADC`, `IO8`, `IO9`, `GND` | Reset + pini liberi |
| **H1** | componentă cu 3 pini lângă margine | **Probabil senzor Hall** (detectează magnetul). *De confirmat.* |
| X1 | 32.768 kHz | Ceas pentru sleep / trezire |
| X2 | 22 MHz | Cristal principal |
| S1 / LED1 | buton / LED | Pairing / stare |
| Q1, U3 | tranzistor, regulator/protecție | Alimentare |

### Descoperirea cheie

Opulinks publică pe GitHub:

- **SDK-ul complet pentru OPL1000 A2**, cu programul de scris firmware-ul, programul de configurare a pinilor, 36 de documente (pornire, moduri de boot, memorie flash, consum, API), 30 de exemple (Wi-Fi, BLE, MQTT, OTA) și compilare cu GCC (Makefile) sau Keil.
  - <https://github.com/Opulinks-Tech/OPL1000A2-SDK>
- **Un proiect de referință chiar pentru un senzor magnetic de ușă conectat la CoolKit**: configurare Wi-Fi prin BLE, mod smart sleep, cod sursă inclus și un firmware gata compilat.
  - <https://github.com/Opulinks-Tech/OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS>

Firmware-ul Sonoff e aproape sigur pornit de la acest proiect de referință, deci **avem codul sursă de la care să plecăm**.

### Ce NU merge

- **Tasmota / OpenBeken:** din ce știu, nu suportă OPL1000. *Neverificat exhaustiv.*
- **ESP-NOW:** e un protocol doar pentru cipurile Espressif. OPL1000 nu îl are.
- **Să păcălim aplicația** (ESP32 care se dă drept serverul eWeLink): comunicație criptată și nedocumentată. Nu merită.

## 4. Arhitectura aleasă

### Varianta A (recomandată): firmware propriu pe OPL1000 + mesaje BLE

```text
[DW2 cu firmware propriu]                [ESP32 WROOM]
  doarme (~µA)                             ascultă BLE continuu
  magnetul se mișcă → senzorul Hall        filtrează după MAC
  → trezire                                → stare ușă/geam
  trimite rafală de anunțuri BLE           → logica smart-heating
  (ușă, baterie, contor) ~1–2 s
  → adoarme la loc
```

**Conținutul anunțului BLE:** câmp `Manufacturer Data` cu ID propriu, care conține:

- versiune;
- stare (deschis/închis);
- **contor de evenimente**, ca ESP32 să ignore duplicatele;
- tensiunea bateriei (prin ADC);
- opțional, o semnătură scurtă (HMAC) cu cheie comună, ca alte dispozitive să nu poată falsifica mesajele.

**Avantaje:**

- Wi-Fi nu pornește deloc → consum minim, baterii mai durabile.
- Nu depinde de router sau de cloud.
- ESP32 WROOM are BLE.

**De reținut:**

- Dacă ESP32 WROOM folosește și Wi-Fi, BLE-ul și Wi-Fi-ul împart același radio, deci unele anunțuri se pot pierde. De aceea senzorul trimite o **rafală de anunțuri timp de 1–2 s**, iar contorul elimină duplicatele.
- Un mesaj periodic (de exemplu la câteva ore) cu bateria arată că senzorul încă funcționează.

### Varianta B (de rezervă): firmware propriu + UDP/MQTT pe Wi-Fi

Senzorul se conectează la router și trimite un pachet UDP sau MQTT la ESP32 / broker.

- Consumă mai mult: conectarea la Wi-Fi durează 1–3 s.
- Avantaj: SDK-ul are deja un exemplu de MQTT pe rețeaua locală.

### Varianta C (dacă A și B eșuează): altă placă în carcasa Sonoff

Păstrezi carcasa, magnetul și bateriile și pui un ESP32-C3 mic, legat la ieșirea senzorului Hall H1. Apoi poți folosi ESP-NOW.

- Problemă de alimentare: ESP32-C3 cere ≥ 3.0 V, iar 2 baterii AAA scad sub 3 V. Soluții: o celulă LiFePO4 de 3.2 V sau un ridicător de tensiune.

## 5. Ce e de făcut mai departe

### Pasul 0: materiale

- [ ] Adaptor USB-serial de **3.3 V** (CP2102 / CH340 / FTDI), **sau** o placă ESP32 DevKit cu EN legat la GND (cipul ei USB-serial merge ca adaptor).
- [ ] Pini / fire Dupont lipite pe J2 (sunt găuri standard la 2.54 mm).
- [ ] Opțional: clemă SOIC-8 pentru citirea directă a memoriei 25Q80.

### Pasul 1: ascultăm portul serial (fără risc)

- Legi `GND–GND`, `TXD(placă)→RX(adaptor)`, `RXD(placă)→TX(adaptor)`.
- Alimentarea: **ori** cu bateriile și fără 3V3 de la adaptor, **ori** prin 3V3 și fără baterii. Niciodată ambele.
- Citim ce afișează firmware-ul la pornire. Viteza e necunoscută. Încercăm întâi 115200, apoi alte viteze uzuale.
- Ce putem afla: versiunea SDK, log-uri, poate pinii folosiți.

### Pasul 2: copie de siguranță a firmware-ului original (obligatoriu)

- **Metoda 1:** programul Opulinks din SDK (vezi ghidul „patch download tool” și documentele despre modurile de boot).
- **Metoda 2:** citire directă a memoriei 25Q80 cu un ESP32 folosit ca cititor SPI, prin clemă SOIC-8, cu OPL1000 ținut în reset.
- Păstrăm imaginea de 1 MB. Fără ea nu ne mai putem întoarce la eWeLink.

### Pasul 3: aflăm pinii

- Pe ce pin intră semnalul de la **H1**, pe ce pin e **S1** și pe ce pin e **LED1**.
- Pornim de la configurația din proiectul de referință Opulinks, apoi verificăm pe placă cu multimetrul (continuitate) și urmărind traseele.
- Confirmăm că H1 e senzor Hall: magnet aproape/departe și măsurăm ieșirea.

### Pasul 4: mediul de compilare

- Clonăm `OPL1000A2-SDK` și `OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS`.
- Instalăm compilatorul GNU Arm GCC (după capitolul 4 din ghidul de dezvoltare SDK) sau folosim Keil.
- Compilăm proiectul de referință neschimbat, ca să verificăm mediul.
- Testăm întâi pe o **placă de dezvoltare OPL1000**, dacă există, ca să nu riscăm senzorul.

### Pasul 5: firmware-ul nou

- Scoatem partea de cloud (HTTPS / CoolKit) și configurarea Wi-Fi prin BLE.
- Păstrăm: trezirea din sleep pe întreruperea de la H1 și citirea bateriei.
- Adăugăm: rafala de anunțuri BLE (format la secțiunea 4), contorul de evenimente salvat în flash și mesajul periodic.
- Măsurăm consumul (SDK-ul are un exemplu pentru asta).

### Pasul 6: receptorul pe ESP32 WROOM (PlatformIO)

- Ascultă BLE pasiv (NimBLE-Arduino sau ESP-IDF), cu filtru după MAC sau ID de producător.
- Decodează starea, bateria și contorul și elimină duplicatele.
- Expune evenimentul în logica smart-heating: geam deschis → încălzirea oprită în zona respectivă.
- Dacă nu primește mesajul periodic → alarmă „senzor offline”.

### Pasul 7: integrare și teste

- Zile întregi de deschideri și închideri: niciun eveniment pierdut, latența.
- Consum real și estimarea duratei bateriilor.
- Comportament cu ESP32 conectat simultan la Wi-Fi.

## 6. Riscuri

| Risc | Măsură |
|---|---|
| Senzorul rămâne inutilizabil | Copie de siguranță a memoriei înainte de orice scriere |
| Tensiune greșită | Doar 3.3 V. Niciodată 5 V pe J2 |
| Bateriile și 3V3 legate simultan | Ori una, ori alta |
| SDK vechi, documentație incompletă | Proiectul de referință pentru senzor de ușă reduce mult riscul |
| Pinii de pe placă diferă de proiectul de referință | Verificare cu multimetrul (pasul 3) |
| Anunțuri BLE pierdute | Rafală de 1–2 s + contor + mesaj periodic |

## 7. Fișiere pe laptop (BSECELEANU-NB)

Folder: `C:\Users\BSeceleanu\dw2_test\`

| Fișier | Rol |
|---|---|
| `ble_scan.py` | Scanare BLE: dispozitive existente vs. noi + orice dispozitiv apropiat (semnal > −75). Rulare: `C:\Python314\python.exe -u ble_scan.py 90` |
| `lan_watch.py` | Urmărește tabela ARP + anunțurile eWeLink în rețea (mDNS), ca să vezi trezirea senzorului pe Wi-Fi. Relevant doar pentru firmware-ul original |
| `ble_log.csv` | Log-ul scanărilor BLE |
| `_patch.py` | Script de patch temporar (se poate șterge) |

- Biblioteca Python `bleak` 3.1.0 e instalată pentru `C:\Python314\python.exe`, doar pentru utilizatorul curent.
- **Notă tehnică:** pe acest laptop, Python lansat din consolă se blochează dacă nu primește `< NUL` la intrare.

## 8. Linkuri utile

- Pagina produsului: <https://sonoff.tech/en-us/products/sonoff-dw2-wi-fi-wireless-door-window-sensor>
- Manual: <https://manuals.plus/sonoff/dw2-wi-fi-wi-fi-wireless-door-window-sensor-manual>
- Dosar FCC (are poze interne cu placa): <https://fccid.io/2APN5DW2-WIFI>
- SDK OPL1000 A2: <https://github.com/Opulinks-Tech/OPL1000A2-SDK>
- Referință senzor de ușă OPL1000: <https://github.com/Opulinks-Tech/OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS>
- Hardware development kit OPL1000: <https://github.com/Opulinks-Tech/OPL1000-HDK>
- Organizația Opulinks pe GitHub: <https://github.com/Opulinks-Tech>

## 9. Întrebări deschise

1. Ce viteză are portul serial și ce afișează firmware-ul original la pornire?
2. Ce pin al OPL1000 primește semnalul de la H1, de la S1 și de la LED1?
3. Cum intră OPL1000 în modul de scriere: e nevoie de un pin anume la pornire sau ajunge doar portul serial + RST?
4. Memoria flash e protejată la citire sau scriere?
5. Cât consumă real în sleep cu firmware-ul nostru, față de ≤ 40 µA cu cel original?
