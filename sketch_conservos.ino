#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <Servo.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pines de botones (Con pull-up interno)
#define subir 7
#define ok 8
#define bajar 9
#define atras 10   // Botón para volver atrás

// Pin del Buzzer Activo
#define buzzerPin 2

// Si el buzzer suena constante al arrancar, cambia esta línea a false:
const bool BUZZER_ACTIVO_LOW = true; 

// Pines para los 4 servos
const int pinesServos[4] = {3, 4, 5, 6};
Servo servos[4];

const char menuPas[6][20] = {
  "Anadir horario",
  "Horarios program",
  "Borrar alarma",
  "Resetear todo",
  "Cargar pastillas",
  "Pastillas dispon"
};
const int totalOpciones = 6;
int indiceActual = 0;
int indiceAnterior = -1;

int pantalla = 0;
int pantallaAnterior = -1;
int subPantalla = 0;

unsigned long ultimoBoton = 0;
const unsigned long tiempoEspera = 10000;

int minutoAnterior = -1;

// Variables temporales para añadir alarma
int hora = 8;
int minuto = 0;
int compartimento = 1;
int numAlarma = 1;      
int cantidadDosis = 1;  
int definiendoPaso = 0; 

// Variables temporales para borrar una alarma
int borrarComp = 1;
int borrarNumAlarma = 1;
int pasoBorrado = 0; 

// Variables para Cargar Pastillas
int cargarComp = 1;
int cargarCant = 10;
int pasoCargar = 0; // 0 = Seleccionar Compartimento, 1 = Seleccionar Cantidad

// Matriz de Alarmas
int horasAlarmas[4][3];
int minutosAlarmas[4][3];
int dosisAlarmas[4][3];        
bool yaDispensado[4][3];        
int stockPastillas[4] = {10, 10, 10, 10}; 

int verCompartimento = 1;
int verAlarma = 1;

// Variables para el Buzzer
bool alarmaSonando = false;
unsigned long ultimoPulsoBuzzer = 0;

DateTime now;

// Funciones
void guardarAlarmaEEPROM(int comp, int slot, int h, int m, int cant);
void borrarAlarmaEEPROM(int comp, int slot);
void resetearTodoEEPROM();
void cargarAlarmasEEPROM();
void mostrarHora();
void mostrarMenu();
void mostrarOpcion();
void configurarAlarma();
void gestionarBorrado();
void gestionarCarga();
void revisarAlarmas();
void dispensarPastilla(int indiceCompartimento, int cantidad);
void actualizarBuzzer();
void encenderBuzzer();
void apagarBuzzer();

void encenderBuzzer() {
  digitalWrite(buzzerPin, BUZZER_ACTIVO_LOW ? LOW : HIGH);
}

void apagarBuzzer() {
  digitalWrite(buzzerPin, BUZZER_ACTIVO_LOW ? HIGH : LOW);
}

void setup() {
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  apagarBuzzer();

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Bienvenido");
  lcd.setCursor(0, 1);
  lcd.print("      PILLEX");

  pinMode(subir, INPUT_PULLUP);
  pinMode(bajar, INPUT_PULLUP);
  pinMode(ok, INPUT_PULLUP);
  pinMode(atras, INPUT_PULLUP);

  if (!rtc.begin()) {
    Serial.println("RTC no encontrado, pero arrancando...");
  }

  cargarAlarmasEEPROM();
  delay(2000);
  lcd.clear();
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
  now = rtc.now();

  revisarAlarmas();
  actualizarBuzzer();

  // --- BOTÓN ATRÁS ---
  if (digitalRead(atras) == LOW) {
    if (pantalla == 1) {
      if (subPantalla == 0) {
        pantalla = 0;
      } else if (subPantalla == 1) {
        if (definiendoPaso > 0) definiendoPaso--;
        else subPantalla = 0;
      } else if (subPantalla == 3) {
        if (pasoBorrado > 0) pasoBorrado--;
        else subPantalla = 0;
      } else if (subPantalla == 5) {
        if (pasoCargar > 0) pasoCargar--;
        else subPantalla = 0;
      } else {
        subPantalla = 0;
      }
      lcd.clear();
      indiceAnterior = -1;
    }
    ultimoBoton = millis();
    delay(250);
  }

  // --- NAVEGACIÓN MENÚ ---
  if (digitalRead(subir) == LOW && pantalla == 1 && subPantalla == 0) {
    indiceActual++;
    if (indiceActual >= totalOpciones) indiceActual = 0;
    ultimoBoton = millis();
    delay(200);
  }

  if (digitalRead(bajar) == LOW && pantalla == 1 && subPantalla == 0) {
    indiceActual--;
    if (indiceActual < 0) indiceActual = totalOpciones - 1;
    ultimoBoton = millis();
    delay(200);
  }

  // --- BOTÓN OK ---
  if (digitalRead(ok) == LOW) {
    if (alarmaSonando) {
      alarmaSonando = false;
      apagarBuzzer();
      lcd.clear();
      pantallaAnterior = -1;
      delay(300);
    } else {
      if (pantalla == 0) {
        pantalla = 1;
        subPantalla = 0;
      } else if (pantalla == 1 && subPantalla == 0) {
        subPantalla = indiceActual + 1;
        definiendoPaso = 0;
        pasoBorrado = 0;
        pasoCargar = 0;
        lcd.clear();
      } else if (pantalla == 1 && subPantalla == 1) { // Añadir Alarma
        definiendoPaso++;
        lcd.clear();
        if (definiendoPaso > 4) {
          int cIdx = compartimento - 1;
          int aIdx = numAlarma - 1;

          horasAlarmas[cIdx][aIdx] = hora;
          minutosAlarmas[cIdx][aIdx] = minuto;
          dosisAlarmas[cIdx][aIdx] = cantidadDosis;

          guardarAlarmaEEPROM(cIdx, aIdx, hora, minuto, cantidadDosis);

          lcd.setCursor(0, 0);
          lcd.print("Alarma Guardada!");
          lcd.setCursor(0, 1);
          lcd.print("C"); lcd.print(compartimento);
          lcd.print(" A"); lcd.print(numAlarma);
          lcd.print(" Dosis:"); lcd.print(cantidadDosis);
          delay(2000);

          definiendoPaso = 0;
          subPantalla = 0;
          indiceAnterior = -1;
          lcd.clear();
        }
      } else if (pantalla == 1 && subPantalla == 3) { // Borrar Alarma
        pasoBorrado++;
        lcd.clear();
        if (pasoBorrado > 1) {
          int cIdx = borrarComp - 1;
          int aIdx = borrarNumAlarma - 1;

          borrarAlarmaEEPROM(cIdx, aIdx);

          lcd.setCursor(0, 0);
          lcd.print("Alarma Borrada! ");
          lcd.setCursor(0, 1);
          lcd.print("C"); lcd.print(borrarComp);
          lcd.print(" Alarma "); lcd.print(borrarNumAlarma);
          delay(2000);

          pasoBorrado = 0;
          subPantalla = 0;
          indiceAnterior = -1;
          lcd.clear();
        }
      } else if (pantalla == 1 && subPantalla == 4) { // Resetear Todo
        resetearTodoEEPROM();

        lcd.setCursor(0, 0);
        lcd.print("   MEMORIA CERO  ");
        lcd.setCursor(0, 1);
        lcd.print("Alarmas borradas");
        delay(2000);

        subPantalla = 0;
        indiceAnterior = -1;
        lcd.clear();
      } else if (pantalla == 1 && subPantalla == 5) { // Cargar Pastillas
        pasoCargar++;
        lcd.clear();
        if (pasoCargar > 1) {
          int cIdx = cargarComp - 1;
          stockPastillas[cIdx] += cargarCant; // Se agrega la cantidad al stock existente

          lcd.setCursor(0, 0);
          lcd.print("Stock Cargado!  ");
          lcd.setCursor(0, 1);
          lcd.print("Comp "); lcd.print(cargarComp);
          lcd.print(" Total:"); lcd.print(stockPastillas[cIdx]);
          delay(2000);

          pasoCargar = 0;
          subPantalla = 0;
          indiceAnterior = -1;
          lcd.clear();
        }
      } else if (pantalla == 1 && (subPantalla == 2 || subPantalla == 6)) {
        subPantalla = 0;
        indiceAnterior = -1;
        lcd.clear();
      }
    }

    ultimoBoton = millis();
    delay(250);
  }

  // Regreso por inactividad
  if ((millis() - ultimoBoton) >= tiempoEspera && subPantalla == 0) {
    pantalla = 0;
  }

  // --- VISTAS DISPLAY ---
  if (alarmaSonando) {
    lcd.setCursor(0, 0);
    lcd.print(" TOME PASTILLA  ");
    lcd.setCursor(0, 1);
    lcd.print("Presione OK...  ");
  } else if (pantalla == 0) {
    if (pantallaAnterior != 0) {
      lcd.clear();
      pantallaAnterior = 0;
      minutoAnterior = -1;
    }
    mostrarHora();
  } else if (pantalla == 1) {
    pantallaAnterior = 1;
    if (subPantalla == 0) {
      if (indiceActual != indiceAnterior) {
        mostrarMenu();
        indiceAnterior = indiceActual;
      }
    } else {
      mostrarOpcion();
    }
  }

  // --- CONFIGURACIÓN DE PARÁMETROS ---
  if (subPantalla == 1) { // Añadir Alarma
    if (digitalRead(subir) == LOW) {
      if (definiendoPaso == 0) { compartimento++; if (compartimento > 4) compartimento = 1; }
      else if (definiendoPaso == 1) { numAlarma++; if (numAlarma > 3) numAlarma = 1; }
      else if (definiendoPaso == 2) { hora++; if (hora > 23) hora = 0; }
      else if (definiendoPaso == 3) { minuto++; if (minuto > 59) minuto = 0; }
      else if (definiendoPaso == 4) { cantidadDosis++; if (cantidadDosis > 5) cantidadDosis = 1; }
      delay(180);
    }
    if (digitalRead(bajar) == LOW) {
      if (definiendoPaso == 0) { compartimento--; if (compartimento < 1) compartimento = 4; }
      else if (definiendoPaso == 1) { numAlarma--; if (numAlarma < 1) numAlarma = 3; }
      else if (definiendoPaso == 2) { hora--; if (hora < 0) hora = 23; }
      else if (definiendoPaso == 3) { minuto--; if (minuto < 0) minuto = 59; }
      else if (definiendoPaso == 4) { cantidadDosis--; if (cantidadDosis < 1) cantidadDosis = 5; }
      delay(180);
    }
  }

  if (subPantalla == 2) { // Ver Programados
    if (digitalRead(subir) == LOW) {
      verAlarma++;
      if (verAlarma > 3) { verAlarma = 1; verCompartimento++; if (verCompartimento > 4) verCompartimento = 1; }
      delay(200);
    }
    if (digitalRead(bajar) == LOW) {
      verAlarma--;
      if (verAlarma < 1) { verAlarma = 3; verCompartimento--; if (verCompartimento < 1) verCompartimento = 4; }
      delay(200);
    }
  }

  if (subPantalla == 3) { // Borrar Alarma
    if (digitalRead(subir) == LOW) {
      if (pasoBorrado == 0) { borrarComp++; if (borrarComp > 4) borrarComp = 1; }
      else if (pasoBorrado == 1) { borrarNumAlarma++; if (borrarNumAlarma > 3) borrarNumAlarma = 1; }
      delay(180);
    }
    if (digitalRead(bajar) == LOW) {
      if (pasoBorrado == 0) { borrarComp--; if (borrarComp < 1) borrarComp = 4; }
      else if (pasoBorrado == 1) { borrarNumAlarma--; if (borrarNumAlarma < 1) borrarNumAlarma = 3; }
      delay(180);
    }
  }

  if (subPantalla == 5) { // Cargar Pastillas
    if (digitalRead(subir) == LOW) {
      if (pasoCargar == 0) { cargarComp++; if (cargarComp > 4) cargarComp = 1; }
      else if (pasoCargar == 1) { cargarCant++; if (cargarCant > 20) cargarCant = 1; }
      delay(180);
    }
    if (digitalRead(bajar) == LOW) {
      if (pasoCargar == 0) { cargarComp--; if (cargarComp < 1) cargarComp = 4; }
      else if (pasoCargar == 1) { cargarCant--; if (cargarCant < 1) cargarCant = 20; }
      delay(180);
    }
  }
}

void actualizarBuzzer() {
  if (!alarmaSonando) {
    apagarBuzzer();
    return;
  }

  unsigned long milisActuales = millis();

  if (milisActuales - ultimoPulsoBuzzer >= 2000) {
    ultimoPulsoBuzzer = milisActuales;
    encenderBuzzer();
    delay(80);
    apagarBuzzer();
  }
}

void guardarAlarmaEEPROM(int comp, int slot, int h, int m, int cant) {
  int direccionBase = (comp * 3 + slot) * 3;
  EEPROM.update(direccionBase, h);
  EEPROM.update(direccionBase + 1, m);
  EEPROM.update(direccionBase + 2, cant);
}

void borrarAlarmaEEPROM(int comp, int slot) {
  int direccionBase = (comp * 3 + slot) * 3;
  EEPROM.update(direccionBase, 255);
  EEPROM.update(direccionBase + 1, 255);
  EEPROM.update(direccionBase + 2, 255);

  horasAlarmas[comp][slot] = -1;
  minutosAlarmas[comp][slot] = -1;
  dosisAlarmas[comp][slot] = 0;
}

void resetearTodoEEPROM() {
  for (int c = 0; c < 4; c++) {
    for (int a = 0; a < 3; a++) {
      borrarAlarmaEEPROM(c, a);
    }
  }
}

void cargarAlarmasEEPROM() {
  for (int c = 0; c < 4; c++) {
    for (int a = 0; a < 3; a++) {
      int direccionBase = (c * 3 + a) * 3;
      int h = EEPROM.read(direccionBase);
      int m = EEPROM.read(direccionBase + 1);
      int cant = EEPROM.read(direccionBase + 2);

      if (h < 0 || h > 23 || m < 0 || m > 59 || cant < 1 || cant > 5) {
        horasAlarmas[c][a] = -1;
        minutosAlarmas[c][a] = -1;
        dosisAlarmas[c][a] = 0;
      } else {
        horasAlarmas[c][a] = h;
        minutosAlarmas[c][a] = m;
        dosisAlarmas[c][a] = cant;
      }
      yaDispensado[c][a] = false;
    }
  }
}

void dispensarPastilla(int indiceCompartimento, int cantidad) {
  servos[indiceCompartimento].attach(pinesServos[indiceCompartimento]);

  for (int p = 0; p < cantidad; p++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DISPENSANDO...");
    lcd.setCursor(0, 1);
    lcd.print("Pastilla ");
    lcd.print(p + 1);
    lcd.print(" de ");
    lcd.print(cantidad);

    servos[indiceCompartimento].write(0);
    delay(300);
    servos[indiceCompartimento].write(180);
    delay(1200);
    servos[indiceCompartimento].write(0);
    delay(800);
  }

  servos[indiceCompartimento].detach();

  // --- DESCUENTO DE STOCK ---
  if (stockPastillas[indiceCompartimento] >= cantidad) {
    stockPastillas[indiceCompartimento] -= cantidad;
  } else {
    stockPastillas[indiceCompartimento] = 0;
  }

  // --- AVISO SI QUEDA POCO STOCK (1 o 0 pastillas) ---
  if (stockPastillas[indiceCompartimento] <= 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("!ALERTA STOCK!");
    lcd.setCursor(0, 1);
    lcd.print("C"); lcd.print(indiceCompartimento + 1);
    lcd.print(" Queda(n): "); lcd.print(stockPastillas[indiceCompartimento]);
    delay(2500);
  }

  alarmaSonando = true;
  ultimoPulsoBuzzer = millis() - 2000;
}

void revisarAlarmas() {
  static int ultimoDiaReseteo = -1;

  if (now.hour() == 0 && now.minute() == 0 && now.second() == 0 && ultimoDiaReseteo != now.day()) {
    for (int c = 0; c < 4; c++) {
      for (int a = 0; a < 3; a++) {
        yaDispensado[c][a] = false;
      }
    }
    ultimoDiaReseteo = now.day();
  }

  for (int c = 0; c < 4; c++) {
    for (int a = 0; a < 3; a++) {
      if (horasAlarmas[c][a] != -1 &&
          horasAlarmas[c][a] == now.hour() &&
          minutosAlarmas[c][a] == now.minute() &&
          !yaDispensado[c][a]) {

        dispensarPastilla(c, dosisAlarmas[c][a]);

        yaDispensado[c][a] = true;

        delay(1000);
        lcd.clear();
        pantallaAnterior = -1;
      }
    }
  }
}

void mostrarHora() {
  if (minutoAnterior != now.minute()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FECHA: ");
    if (now.day() < 10) lcd.print("0");
    lcd.print(now.day());
    lcd.print("/");
    if (now.month() < 10) lcd.print("0");
    lcd.print(now.month());
    lcd.print("/");
    lcd.print(now.year() % 100);

    lcd.setCursor(0, 1);
    lcd.print("HORA:  ");
    if (now.hour() < 10) lcd.print("0");
    lcd.print(now.hour());
    lcd.print(":");
    if (now.minute() < 10) lcd.print("0");
    lcd.print(now.minute());
    minutoAnterior = now.minute();
  }
}

void mostrarMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(">");
  lcd.print(menuPas[indiceActual]);
}

void configurarAlarma() {
  lcd.setCursor(0, 0);
  if (definiendoPaso == 0) {
    lcd.print("1.Compartimento:");
    lcd.setCursor(0, 1);
    lcd.print("Seccion: "); lcd.print(compartimento); lcd.print(" (1-4)");
  } else if (definiendoPaso == 1) {
    lcd.print("2.Num de Alarma:");
    lcd.setCursor(0, 1);
    lcd.print("Alarma: "); lcd.print(numAlarma); lcd.print(" de 3");
  } else if (definiendoPaso == 2) {
    lcd.print("3.Ajustar Hora: ");
    lcd.setCursor(0, 1);
    if (hora < 10) lcd.print("0");
    lcd.print(hora); lcd.print(":"); if (minuto < 10) lcd.print("0"); lcd.print(minuto);
  } else if (definiendoPaso == 3) {
    lcd.print("4.Ajustar Minuto");
    lcd.setCursor(0, 1);
    if (hora < 10) lcd.print("0");
    lcd.print(hora); lcd.print(":"); if (minuto < 10) lcd.print("0"); lcd.print(minuto);
  } else if (definiendoPaso == 4) {
    lcd.print("5.Cant Pastillas");
    lcd.setCursor(0, 1);
    lcd.print("Dosis: "); lcd.print(cantidadDosis); lcd.print(" pastilla(s)");
  }
}

void gestionarBorrado() {
  lcd.setCursor(0, 0);
  if (pasoBorrado == 0) {
    lcd.print("Borrar Compart:");
    lcd.setCursor(0, 1);
    lcd.print("Seccion: "); lcd.print(borrarComp); lcd.print(" (1-4)");
  } else if (pasoBorrado == 1) {
    lcd.print("Borrar Alarma: ");
    lcd.setCursor(0, 1);
    lcd.print("Alarma: "); lcd.print(borrarNumAlarma); lcd.print(" de 3");
  }
}

void gestionarCarga() {
  lcd.setCursor(0, 0);
  if (pasoCargar == 0) {
    lcd.print("1.Compartimento:");
    lcd.setCursor(0, 1);
    lcd.print("Seccion: "); lcd.print(cargarComp); lcd.print(" (1-4)");
  } else if (pasoCargar == 1) {
    lcd.print("2.Cant a Cargar:");
    lcd.setCursor(0, 1);
    lcd.print("Agregar: "); lcd.print(cargarCant); lcd.print(" past.");
  }
}

void mostrarProgramado() {
  int cIdx = verCompartimento - 1;
  int aIdx = verAlarma - 1;

  lcd.setCursor(0, 0);
  lcd.print("C"); lcd.print(verCompartimento);
  lcd.print(" Alarma "); lcd.print(verAlarma); lcd.print("/3");

  lcd.setCursor(0, 1);
  if (horasAlarmas[cIdx][aIdx] != -1) {
    if (horasAlarmas[cIdx][aIdx] < 10) lcd.print("0");
    lcd.print(horasAlarmas[cIdx][aIdx]);
    lcd.print(":");
    if (minutosAlarmas[cIdx][aIdx] < 10) lcd.print("0");
    lcd.print(minutosAlarmas[cIdx][aIdx]);
    lcd.print("hs Cant:");
    lcd.print(dosisAlarmas[cIdx][aIdx]);
  } else {
    lcd.print("Sin programar   ");
  }
}

void mostrarOpcion() {
  switch (subPantalla) {
    case 1:
      configurarAlarma();
      break;
    case 2:
      mostrarProgramado();
      break;
    case 3:
      gestionarBorrado();
      break;
    case 4:
      lcd.setCursor(0, 0);
      lcd.print("Resetear TODO?");
      lcd.setCursor(0, 1);
      lcd.print("OK:Si  Atras:No ");
      break;
    case 5:
      gestionarCarga();
      break;
    case 6:
      lcd.setCursor(0, 0);
      lcd.print("Disp C1:"); lcd.print(stockPastillas[0]);
      lcd.print(" C2:"); lcd.print(stockPastillas[1]);
      lcd.setCursor(0, 1);
      lcd.print("Disp C3:"); lcd.print(stockPastillas[2]);
      lcd.print(" C4:"); lcd.print(stockPastillas[3]);
      break;
  }
}