#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#define PIN_LED_ESTADO 4
#define PIN_LED_NODOS 2
#define PIN_LED_POTENCIA 33
#define PIN_BTN_INICIO 32
#define PIN_BTN_NODO 5
#define PIN_POT 15
#define PIN_SDA 26
#define PIN_SCL 27

#define TRIG_IZQ 25
#define ECHO_IZQ 14
#define TRIG_CEN 23
#define ECHO_CEN 22
#define TRIG_DER 19
#define ECHO_DER 18

#define CANAL_PWM 0
#define DEBOUNCE 300UL

Adafruit_MPU6050 mpu;
bool mpuOK = false;
bool viajeActivo = false;
int nodos = 0;

unsigned long tBtn1 = 0;
unsigned long tBtn2 = 0;

bool estadoBtn1 = HIGH;
bool estadoBtn2 = HIGH;

bool fIzq = false, fDer = false, fCen = false, fFreno = false;

float medir(int trig, int echo)
{
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long us = pulseIn(echo, HIGH, 12000);
  if (us == 0)
    return 400.0f;
  float cm = us * 0.034f / 2.0f;
  return (cm > 400.0f) ? 400.0f : cm;
}

void terminarViaje(const char *motivo)
{
  viajeActivo = false;
  digitalWrite(PIN_LED_ESTADO, LOW);
  digitalWrite(PIN_LED_NODOS, LOW);
  ledcWrite(CANAL_PWM, 0);
  fIzq = fDer = fCen = fFreno = false;
  Serial.println("\n========================================");
  Serial.println("           FIN DEL VIAJE");
  Serial.printf("  Motivo          : %s\n", motivo);
  Serial.printf("  Nodos recorridos: %d\n", nodos);
  Serial.println("========================================");
  Serial.println("Presiona BTN INICIO para nuevo viaje.\n");
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_ESTADO, OUTPUT);
  digitalWrite(PIN_LED_ESTADO, LOW);
  pinMode(PIN_LED_NODOS, OUTPUT);
  digitalWrite(PIN_LED_NODOS, LOW);

  ledcSetup(CANAL_PWM, 5000, 8);
  ledcAttachPin(PIN_LED_POTENCIA, CANAL_PWM);
  ledcWrite(CANAL_PWM, 0);

  pinMode(PIN_BTN_INICIO, INPUT_PULLUP);
  pinMode(PIN_BTN_NODO, INPUT_PULLUP);

  pinMode(TRIG_IZQ, OUTPUT);
  pinMode(ECHO_IZQ, INPUT);
  pinMode(TRIG_CEN, OUTPUT);
  pinMode(ECHO_CEN, INPUT);
  pinMode(TRIG_DER, OUTPUT);
  pinMode(ECHO_DER, INPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  if (mpu.begin())
  {
    mpuOK = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  estadoBtn1 = digitalRead(PIN_BTN_INICIO);
  estadoBtn2 = digitalRead(PIN_BTN_NODO);
  tBtn1 = millis();
  tBtn2 = millis();

  Serial.println("========================================");
  Serial.println("   SCOOTER GEN-5 - SISTEMA LISTO");
  Serial.println("   Presiona BTN INICIO para comenzar");
  Serial.println("========================================\n");
}

void loop()
{
  bool lecturaBtn1 = digitalRead(PIN_BTN_INICIO);
  bool lecturaBtn2 = digitalRead(PIN_BTN_NODO);

  if (estadoBtn1 == HIGH && lecturaBtn1 == LOW && millis() - tBtn1 > DEBOUNCE)
  {
    tBtn1 = millis();
    if (!viajeActivo)
    {
      viajeActivo = true;
      nodos = 0;
      fIzq = fDer = fCen = fFreno = false;
      digitalWrite(PIN_LED_ESTADO, HIGH);
      digitalWrite(PIN_LED_NODOS, LOW);
      Serial.println(">>> MOVIMIENTO INICIADO\n");
    }
    else
    {
      terminarViaje("BOTON INICIO");
    }
  }
  estadoBtn1 = lecturaBtn1;

  if (!viajeActivo)
  {
    ledcWrite(CANAL_PWM, 0);
    estadoBtn2 = lecturaBtn2;
    return;
  }

  if (estadoBtn2 == HIGH && lecturaBtn2 == LOW && millis() - tBtn2 > DEBOUNCE)
  {
    tBtn2 = millis();
    nodos++;
    digitalWrite(PIN_LED_NODOS, HIGH);
    delay(80);
    digitalWrite(PIN_LED_NODOS, LOW);
    Serial.printf(">>> NODO ALCANZADO. Total: %d\n", nodos);
  }
  estadoBtn2 = lecturaBtn2;

  if (mpuOK)
  {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    if (abs(a.acceleration.x) > 4.0f || abs(a.acceleration.y) > 4.0f)
    {
      Serial.println(">>> SCOOTER CAIDO");
      terminarViaje("CAIDA DETECTADA");
      return;
    }
  }

  float dIzq = medir(TRIG_IZQ, ECHO_IZQ);
  float dCen = medir(TRIG_CEN, ECHO_CEN);
  float dDer = medir(TRIG_DER, ECHO_DER);

  float angActual = 90.0f;

  if (dCen < 200.0f)
  {
    if (!fCen)
    {
      Serial.printf(">>> Objeto en el centro a %.0f cm\n", dCen);
      fCen = true;
      fIzq = fDer = fFreno = false;
    }
    if (dCen < 100.0f && !fFreno)
    {
      Serial.println(">>> FRENANDO");
      fFreno = true;
    }
  }
  else if (dIzq < 200.0f)
  {
    angActual = (float)map((long)constrain((int)dIzq, 5, 200), 200L, 5L, 90L, 180L);
    if (!fIzq)
    {
      Serial.printf(">>> Objeto se acerca por la izquierda a %.0f cm - voltear a la derecha\n", dIzq);
      fIzq = true;
      fDer = fCen = fFreno = false;
    }
  }
  else if (dDer < 200.0f)
  {
    angActual = (float)map((long)constrain((int)dDer, 5, 200), 200L, 5L, 90L, 0L);
    if (!fDer)
    {
      Serial.printf(">>> Objeto se acerca por la derecha a %.0f cm - girando a la izquierda\n", dDer);
      fDer = true;
      fIzq = fCen = fFreno = false;
    }
  }
  else
  {
    if (fIzq || fDer || fCen)
    {
      Serial.println(">>> Objeto alejado - volviendo al centro");
      fIzq = fDer = fCen = fFreno = false;
    }
  }

  int potMax = map(analogRead(PIN_POT), 0, 4095, 0, 255);
  float desv = abs(angActual - 90.0f);
  int brillo = constrain((int)((desv / 90.0f) * potMax), 0, 255);
  ledcWrite(CANAL_PWM, brillo);

  delay(50);
}