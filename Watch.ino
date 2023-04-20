/*
   Ссылка для установки менеджера плат для MiniCore:
   https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json
*/

//-----------Подключение библиотек-----------------------------------
#include <math.h>  // Подключаем библиотеку Math
#include <RTClib.h>
#include <LowPower.h>     // Подключаем библиотеку глубокого сна
#include <GyverButton.h>  // Подключаем библиотеку обработки нажатий кнопок

//-----------Настройки LED-------------------------------------------
const int row[6] = { A2, A3, 3, 4, 5, 6 };  // Пины для A, B, C, D, E, F строк в LED матрице. Смотри картинку в инструкции
const int col[4] = { 7, 8, 9, 10 };         // Пины для 1, 2, 3, 4 столбцов в LED матрице.
byte rowByte[6] = { 0, 0, 0, 0, 0, 0 };     // Таблица которая хранит и индексирует байты используемые для отображения
int rowCount = 0;                           // Индекс, используемый при циклическом переборе байтов для отображения на матрице
int delayTime = 300;                        // Микросекунд. Время задержки сканирования дисплея между каждой строкой в LED матрице.
bool autoOff = false;                       // Включает автоматическое отключение дисплея после 20 секунд простоя

//-----------Настройки сна-------------------------------------------
unsigned long onTime = 20 * 1000;  // Время работы дисплея перед уходом в глубокий сон (20 сек)
unsigned long sleepTimer = 0;      // Время пробуждения часов
bool awakened = false;             // Переменная для выключения LED матрицы
bool settings = false;             // Переменная для включения настроек
volatile bool woke = false;        // Флаг после включения микроконтроллера для запуска программы

//-----------Настройки кнопок----------------------------------------
#define pushBtn1 2   // Пин для средней кнопки (используемой для прерывания сна)
#define pushBtn2 A0  // Пин для верхней кнопки
#define pushBtn3 A1  // Пин для нижней кнопки
GButton button1(pushBtn1);
GButton button2(pushBtn2);
GButton button3(pushBtn3);

//-----------Настройки времени---------------------------------------
RTC_DS3231 rtc;  // Имя подключенного RTC
DateTime now;
bool timeOrDate = true;  // Указывает какая опция показывается на экране. Время или дата
bool data = false;       // Включает отображении даты по одиночному клику средней кнопкой
int x, y, z;             // Переменные для функции определения количества дней в месяце

//-----------Функции-------------------------------------------------
void WakeFunction();         // Функция пробуждения при нажатии средней кнопки
void DisplayRow(int, byte);  // Функция прорисовки времени на LED матрицу
void Off();

void setup() {
  rtc.begin();  // Запуск RTC
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(2022, 11, 11, 11, 11, 11));
  }                                                // При замене батареи установит время на 11 ноября 2022 года 11:11:11
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Устанавливает в RTC время и дату компьютера
  for (int i = 0; i < 6; i++) {                    // Задаём пинам светодиодов режим выхода
    pinMode(row[i], OUTPUT);
    for (int f = 0; f < 4; f++) {
      pinMode(col[f], OUTPUT);
    }
  }
  delay(40);
  attachInterrupt(0, WakeFunction, RISING);  // Отслеживает прерывание функции пробуждения
  button1.setDebounce(150);                  // Настройка антидребезга (по умолчанию 80 мс)
  button1.setClickTimeout(500);              // настройка таймаута между кликами (по умолчанию 300 мс)
  button1.setStepTimeout(500);               // Установка таймаута между инкрементами (по умолчанию 400 мс)
  button1.setTimeout(1000);                  // Установка таймаута удержания (по умолчанию 300 мс)
  button2.setDebounce(100);                  // Настройка антидребезга (по умолчанию 80 мс)
  button2.setStepTimeout(300);               // Установка таймаута между инкрементами (по умолчанию 400 мс)
  button3.setDebounce(100);                  // Настройка антидребезга (по умолчанию 80 мс)
  button3.setStepTimeout(300);               // Установка таймаута между инкрементами (по умолчанию 400 мс)
  ShutdownNow();                             // И уходит в глубокий сон
}

void loop() {
  ShutdownNow();   // Функция глубокого сна
  button1.tick();  // Функция отслеживания средней кнопки
  button2.tick();  // Функция отслеживания верхней кнопки
  button3.tick();  // Функция отслеживания нижней кнопки
  Woke();
  Settings();  // Функция режима настроек
  Awakened();  // Основная функция
}

//-----------Основная функция----------------------------------------
void Awakened() {
  if (awakened) {
    if (millis() - sleepTimer <= onTime || !autoOff)  // Если дисплей был включен менее чем 20 секунд или функция таймера выключена
    {
      now = rtc.now();
      if (timeOrDate)  // Загружает текущее время
      {
        rowByte[0] = now.hour() / 10;    // Первый символ часа
        rowByte[1] = now.hour() % 10;    // Второй символ часа
        rowByte[2] = now.minute() / 10;  // Первый символ минуты
        rowByte[3] = now.minute() % 10;  // Второй символ минуты
        rowByte[4] = now.second() / 10;  // Первый символ секунды
        rowByte[5] = now.second() % 10;  // Второй символ секунды
      } else                             // Загружает дату
      {
        rowByte[0] = now.day() / 10;            // Первый символ дня
        rowByte[1] = now.day() % 10;            // Второй символ дня
        rowByte[2] = now.month() / 10;          // Первый символ месяца
        rowByte[3] = now.month() % 10;          // Второй символ месяца
        rowByte[4] = (now.year() - 2000) / 10;  // Первый символ года
        rowByte[5] = (now.year() - 2000) % 10;  // Второй символ года
      }
      DisplayRow(rowCount, rowByte[rowCount]);  // Показывает время на LED матрице
      rowCount++;
      if (rowCount > 5)
        rowCount = 0;
    } else if (autoOff) {  // Когда дисплей был включён более 20 секунд и включена функция автовыключения
      Off();
    }
    if (500 <= millis() - sleepTimer <= onTime || !autoOff) {
      if (data == true && !woke && button1.isSingle()) {
        timeOrDate = !timeOrDate;  // Переключение между временем и датой
      }
      if (button1.isHolded()) {
        Off();
      }
      if (button1.isDouble()) {  // Меняет настройку "Автовыключение" на противоположную по двойному клику на среднюю кнопку
        autoOff = !autoOff;
        displaySnake(1);
      }
      if (button1.isTriple()) {  // Меняет настройку "Показывание даты" на противоположную по тройному клику на среднюю кнопку
        data = !data;
        timeOrDate = true;
        displaySnake(2);
      }
    }
  }
}

//-----------Функция срабатывающая после прерывания------------------
void Woke() {
  if (woke) {
    if (!button1.isPress()) {
      settings = true;
      awakened = true;        // Флаг для запуска LED матрицы
      sleepTimer = millis();  // Обратный отсчёт до сна
    }
    woke = false;
  }
}
//-----------Функция отключения и смены настроек---------------------
void Off() {
  settings = false;
  woke = false;
  awakened = false;
}

//-----------Функция режима настроек---------------------------------
void Settings() {
  if (settings) {
    if (timeOrDate) {
      if (button2.isPress()) {  // Нажатие верхней кнопки .. часы
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), (now.hour() == 23 ? 0 : now.hour() + 1), now.minute(), now.second()));
        sleepTimer = millis();        // Обратный отсчёт до сна
      } else if (button2.isStep()) {  // Удержание верхней кнопки .. пролистывание значения часов
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), (now.hour() == 23 ? 0 : now.hour() + 1), now.minute(), now.second()));
        sleepTimer = millis();         // Обратный отсчёт до сна
      } else if (button3.isPress()) {  // Нажатие нижней кнопки .. минуты
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), (now.minute() == 59 ? 0 : now.minute() + 1), now.second()));
        sleepTimer = millis();        // Обратный отсчёт до сна
      } else if (button3.isStep()) {  // Удержание нижней кнопки .. пролистывание значения минут
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), (now.minute() == 59 ? 0 : now.minute() + 1), now.second()));
        sleepTimer = millis();  // Обратный отсчёт до сна
      }
    } else {
      if (button2.isPress()) {       // Нажатие верхней кнопки .. дни
        f(now.month(), now.year());  // Функция определения количества дней в месяце
        rtc.adjust(DateTime(now.year(), now.month(), (now.day() == z ? 1 : now.day() + 1), now.hour(), now.minute(), now.second()));
        sleepTimer = millis();        // Обратный отсчёт до сна
      } else if (button2.isStep()) {  // Удержание верхней кнопки .. пролистывание значения дней
        f(now.month(), now.year());   // Функция определения количества дней в месяце
        rtc.adjust(DateTime(now.year(), now.month(), (now.day() == z ? 1 : now.day() + 1), now.hour(), now.minute(), now.second()));
        sleepTimer = millis();          // Обратный отсчёт до сна
      } else if (button3.isSingle()) {  // Нажатие нижней кнопки .. месяцы
        rtc.adjust(DateTime(now.year(), (now.month() == 12 ? 1 : now.month() + 1), now.day(), now.hour(), now.minute(), now.second()));
        now = rtc.now();
        f(now.month(), now.year());  // Функция определения количества дней в месяце
        rtc.adjust(DateTime(now.year(), now.month(), (now.day() >= z ? z : now.day()), now.hour(), now.minute(), now.second()));
        sleepTimer = millis();        // Обратный отсчёт до сна
      } else if (button3.isStep()) {  // Удержание нижней кнопки .. месяцы
        rtc.adjust(DateTime(now.year(), (now.month() == 12 ? 1 : now.month() + 1), now.day(), now.hour(), now.minute(), now.second()));
        now = rtc.now();
        f(now.month(), now.year());  // Функция определения количества дней в месяце
        rtc.adjust(DateTime(now.year(), now.month(), (now.day() >= z ? z : now.day()), now.hour(), now.minute(), now.second()));
        sleepTimer = millis();          // Обратный отсчёт до сна
      } else if (button3.isDouble()) {  // Двойное нажатие по нижней кнопки .. пролистывание значения года
        rtc.adjust(DateTime((now.year() == 2030 ? 2020 : now.year() + 1), now.month(), now.day(), now.hour(), now.minute(), now.second()));
        sleepTimer = millis();  // Обратный отсчёт до сна
      }
    }
  }
}

//-----------Функция определения количества дней в месяце------------
void f(int x, int y) {
  z = 28 + (x + int(floor(x / 8))) % 2 + 2 % x + int(floor((2 - ((y % 4) * ((y % 100) + (y % 400)) + 2) % ((y % 4) * ((y % 100) + (y % 400)) + 1)) / x)) + int(floor(1 / x)) - int(floor((1 - ((y % 4) * ((y % 100) + (y % 400)) + 2) % ((y % 4) * ((y % 100) + (y % 400)) + 1)) / x));
}

//-----------Функция пробуждения-------------------------------------
void WakeFunction() {
  woke = true;
}

//-----------Функция отрисовки LED матрицы---------------------------
void DisplayRow(int inRow, byte inByte) {
  for (int i = 0; i < 6; i++)  // Выключаем все строчки и столбцы
  {
    digitalWrite(row[i], LOW);
    for (int f = 0; f < 4; f++) {
      digitalWrite(col[f], HIGH);
    }
  }

  digitalWrite(row[inRow], HIGH);  // Включает выбранную строку
  for (int d = 0; d < 4; d++)
    digitalWrite(col[d], !bitRead(inByte, d));  // Включает выбранный столбец
}

//-----------Функция глубокого сна-----------------------------------
void ShutdownNow() {
  if (!woke && !awakened) {
    for (int i = 0; i < 6; i++) {
      digitalWrite(row[i], LOW);  // Выключает строчки
      for (int f = 0; f < 4; f++) {
        digitalWrite(col[f], LOW);  // Выключает столбцы
      }
    }
    timeOrDate = true;  // Устанавливает значение true, так время показывается первым при пробуждении
    delay(30);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);  // Режим максимальной экономии энергии
  }
}

//-----------Отрисовка эффектов--------------------------------------
void displaySnake(int a) {
  // Выключаем все строчки и столбцы
  for (int i = 0; i < 6; i++) {
    digitalWrite(row[i], LOW);
    for (int f = 0; f < 4; f++) {
      digitalWrite(col[f], LOW);
    }
  }
  if (a == 1) {
    for (x = 0; x < 6; x++) {
      digitalWrite(row[x], HIGH);  // Включает выбранную строку
      digitalWrite(col[0], LOW);   // Включает выбранный столбец
      delay(100);
    }
  } else {
    for (x = 5; x > 0; x--) {
      digitalWrite(row[x], HIGH);  // Включает выбранную строку
      digitalWrite(col[0], LOW);   // Включает выбранный столбец
      delay(100);
    }
  }
}
