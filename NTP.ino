/*
 Звонилка 
*/
 
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Wire.h>
#include <DS3231.h>
#include "TM1637.h"
//#include "LowPower.h"

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x07};
IPAddress ip(192, 168, 0, 177);
IPAddress subnet(192, 168, 0, 177);
IPAddress gateway(192, 168, 0, 177);

int syncTime[] = {10, 0, 3}; // время обновления времени(wtf?) 10:00;  1 - mon, 2 - tue, 3 - wed, 4 - thu, 5 - fri, 6 - sat, 7 - sun

int timeTable[28][2] = { // обычное расписание
  {23, 25}, {23, 26}, {23, 27}, {23, 28}, {10, 20}, {11, 5}, {11, 10}, 
  {11, 55}, {12, 25}, {13, 10}, {13, 15}, {14, 0}, {14, 15}, {15, 0},
  {15, 5}, {15, 50}, {16, 5}, {16, 50}, {16, 55}, {17, 40}, {17, 50},
  {18, 35}, {18, 40}, {19, 25}, {19, 30}, {20, 15}, {20, 20}, {21, 5}
};

int shortTimeTable[28][2] = { // субботнее расписание
  {8, 30}, {9, 15}, {9, 20}, {10, 5}, {10, 15}, {11, 0}, {11, 5}, 
  {11, 50}, {12, 20}, {13, 5}, {13, 10}, {13, 55}, {14, 5}, {14, 50},
  {14, 55}, {15, 40}, {15, 50}, {16, 35}, {16, 40}, {17, 25}, {17, 35},
  {18, 20}, {18, 25}, {19, 10}, {19, 15}, {20, 0}, {20, 5}, {20, 50}
};

int celebrationTimeTable[14][2] = { // расписание с часовыми парами
  {8, 30}, {9, 30}, {9, 40}, {10, 40}, {10, 50}, {11, 50}, {12, 0}, 
  {13, 0}, {13, 10}, {14, 10}, {14, 20}, {15, 20}, {15, 30}, {16, 30}
};

char weekDay[][4] = {"Not", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

unsigned int localPort = 8911; // порт для прослушивания входящих udp-пакетов
char timeServer[] = {132,63,97,4}; // адрес NTP-сервера {192, 168, 0, 1} или домен "time.nist.gov"
const int NTP_PACKET_SIZE = 48;
const long timeZone = 18000L; // timeZone = (+5) * 3600

byte packetBuffer[NTP_PACKET_SIZE]; // буфер для хранения входящих и исходящих пакетов
boolean button; // состояние кнопки

EthernetUDP Udp;
TM1637 tm1637(6, 7);
DS3231 clock;
RTCDateTime dt;

void rasp3() {
    for(int i=0; i<=13; i++) {
        if(dt.hour == celebrationTimeTable[i][0] && dt.minute == celebrationTimeTable[0][i]) {
            relay();
        }
    }
}

void rasp2() {
    for(int i=0; i<=27; i++) {
        if(dt.hour == shortTimeTable[i][0] && dt.minute == shortTimeTable[0][i]) {
            relay();
        }
    }
}

void rasp1() {
  for(int i=0; i<=27; i++) {
    if(dt.hour == timeTable[i][0] && dt.minute == timeTable[0][i]) {
        relay();
    }
  }
}

void setup() {
      tm1637.init();
      tm1637.set(5); // яркость дисплея
      clock.begin(); // инициализация часов ds3231
      Serial.begin(9600); // инициализация последовательного порта на скорости 9600бод. Включен для отладки
      Ethernet.begin(mac); // инициализация ethernet-модуля(ip, mac, subnet, gateway)
      Udp.begin(localPort); // инициализация порта для прослушивания
      attachInterrupt(1, fl, FALLING); // включение прерывания для кнопки
      pinMode(4, INPUT); // подключение переключателя режимов стандартные пары/часовые пары
      pinMode(5, INPUT);
      pinMode(8, OUTPUT);
      digitalWrite(8, 1);
      pinMode(3, INPUT); // подключение кнопки
}

void fl() {
    button = 1;
}

void relay() {
      tm1637.point(POINT_OFF);
      tm1637.display(0, 11); // b
      tm1637.display(1, 14); // E
      tm1637.display(2, 27); // L
      tm1637.display(3, 27); // L
      digitalWrite(8, LOW);
      delay(5000);
      digitalWrite(8, HIGH);
      tm1637.clearDisplay();
      tm1637.point(POINT_ON);
      tm1637.display(0, dt.hour/10);
      tm1637.display(1, dt.hour%10);
      tm1637.display(2, dt.minute/10);
      tm1637.display(3, dt.minute%10);
      delay(60000 - (dt.second * 1000));
}

void clockT() {
      tm1637.point(POINT_ON);
      // часы
      tm1637.display(0, dt.hour/10);
      tm1637.display(1, dt.hour%10);
      // секунды
      tm1637.display(2, dt.minute/10);
      tm1637.display(3, dt.minute%10);
}

void loop() {
    dt = clock.getDateTime();
    clockT(); // отрисовка времени
    bool button3 = digitalRead(5); // чтение состояния переключателя(звонки в воскресенье)
    bool button2 = digitalRead(4); // чтение состояния переключателя(стандартные пары/часовые пары)
    if(dt.dayOfWeek < 6 && button2 == 0) {
      rasp1();
    }
    if(dt.dayOfWeek == 6 && button2 == 0){
      rasp2();
    }
    if(dt.dayOfWeek != 7 && button2 == 1) {
      rasp3();
    }
    if (dt.dayOfWeek == 7 && button3 == 1) {
      rasp2();
    }
    ntpu();
}

void ntpu() {
    if(dt.hour == syncTime[0] && dt.minute == syncTime[1] && dt.dayOfWeek == syncTime[2] || button ) { // отправляем запрос на обновление в среду в 10:00 или по нажатию на кнопку
        button = 0;
        ntps(timeServer); // отправка NTP-запроса на сервер
        delay(1000);// подождём ответа
        if (Udp.parsePacket()) { // Получили пакет, читаем данные
            // индикация синхронизации
            tm1637.point(POINT_OFF);
            tm1637.display(0, 5); // S
            tm1637.display(1,25); // Y
            tm1637.display(2,26); // N
            tm1637.display(3,12); // C
            Udp.read(packetBuffer, NTP_PACKET_SIZE);
            // отметка времени начинается в 40-ом байте полученного пакета
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            const unsigned long seventyYears = 2208988800UL;
            unsigned long epoch = secsSince1900 - seventyYears + timeZone;
            clock.setDateTime(epoch-3600); // записываем время в часы
        }
        else {
            tm1637.clearDisplay();
            tm1637.point(POINT_OFF);
            tm1637.display(0, 14); // E
            tm1637.display(1, 19); // r
            tm1637.display(2, 19); // r
        }
        delay(10000);
    }
}

void ntps(char* address) {
      memset(packetBuffer, 0, NTP_PACKET_SIZE);
      packetBuffer[0] = 0b11100011;   // индикатор перехода(Alarm condition, clock not synchronized)- 2 бита, версия NTP- 3 бита, режим работы(Server)- 3 бита
      packetBuffer[1] = 0;     // Stratum, or type of clock
      packetBuffer[2] = 6;     // Интервал опроса
      packetBuffer[3] = 0xEC;  // Точность
      // 8 bytes of zero for Root Delay & Root Dispersion
      packetBuffer[12]  = 49;
      packetBuffer[13]  = 0x4E;
      packetBuffer[14]  = 49;
      packetBuffer[15]  = 52;
      Udp.beginPacket(address, 123);
      Udp.write(packetBuffer, NTP_PACKET_SIZE);
      Udp.endPacket();
}


