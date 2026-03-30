# Nakładka na fotel monitorująca postawę użytkownika

### Autor: Łukasz Skowron
### Projekt: Nakładka na fotel monitorująca postawę użytkownika

## Opis Projektu
Projekt składa się z dwóch modułów opartych na układach ESP32, 
których zadaniem jest analiza nacisku wywieranego przez użytkownika na oparcie i siedzisko fotela.
System informuje użytkownika o błędnej postawie za pomocą interfejsu graficznego oraz sygnalizacji świetlnej (LED).

### Kluczowe Funkcje:
* **Pomiar wielopunktowy**: Wykorzystanie 3 czujników FSR oraz 4 czujników tensometrycznych.
* **Komunikacja Bezprzewodowa**: Protokół ESP-NOW - brak potrzeby kabla komunikacyjnego).
* **Inteligentna Kalibracja**: Możliwość zdefiniowania prawidłowej postawy oraz jej zapis do pamięci nieulotnej NVS.
* **Interfejs UI**: Wizualizacja obciążenia poszczególnych sekcji fotela na wyświetlaczu.

### Architektura:
* **Nadajnik (ESP32-C3)**: Zbieranie danych z czujników i transmisja radiowa.
* **Odbiornik (ESP32-S3)**: Analiza danych, obsługa wyświetlacza, diod oraz zarządzanie kalibracją.