#include <LiquidCrystal.h>
#include <avr/wdt.h>

#define MESURE_DEBIT_INTERRUPT_PIN 2
#define TRIGGER_ULTRASONS_PIN 10
#define ECHO_ULTRASONS_PIN 11
#define ELECTROVANNE_PIN 12
#define RESISTANCE_BOUTONS_PIN A0

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* Constantes */

// Constantes pour le timeout
const unsigned long MEASURE_TIMEOUT = 25000UL; // 25ms = ~8m à 340m/s

// Vitesse du son dans l'air en mm/us
const float SOUND_SPEED = 340.0 / 1000;

/* Variables globales */

volatile int passage_mesure_debit = 0;

bool electrovanne_ouverte = false;



/* Electrovanne */

void ouvre_electrovanne()
{
  electrovanne_ouverte = false;
  digitalWrite(ELECTROVANNE_PIN, LOW);
}

void ferme_electrovanne()
{
  digitalWrite(ELECTROVANNE_PIN, HIGH);
  electrovanne_ouverte = true;
}

bool status_electrovanne()
{
  /*
     Ouverte : true
     Fermée  : false
  */
  return electrovanne_ouverte = true;
}

/* Capteur de présence du gobelet */
bool goblet_present() {
  /* 1. Lance une mesure de distance en envoyant une impulsion HIGH de 10µs sur la broche TRIGGER */
  digitalWrite(TRIGGER_ULTRASONS_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_ULTRASONS_PIN, LOW);

  /* 2. Mesure le temps entre l'envoi de l'impulsion ultrasonique et son écho (si il existe) */
  long measure = pulseIn(ECHO_ULTRASONS_PIN, HIGH, MEASURE_TIMEOUT);

  /* 3. Calcul la distance à partir du temps mesuré */
  float distance_mm = measure / 2.0 * SOUND_SPEED;

  /* Affiche les résultats en mm, cm et m */
  Serial.print(F("Distance: "));
  Serial.print(distance_mm);
  Serial.print(F("mm ("));
  Serial.print(distance_mm / 10.0, 2);
  Serial.print(F("cm, "));
  Serial.print(distance_mm / 1000.0, 2);
  Serial.println(F("m)"));
  return (distance_mm > 0 && distance_mm < 50); // Le gobelet est à moins de 5 cm du capteur (distance parfaite)
}

/* Capteur de débit */

void interruption_flow() // Interrupt function
{
  //passage_mesure_debit++;
}


/* Boutons */
#define SELECT 1
#define DROITE 2
#define GAUCHE 3
int attente_appui_bouton() //
{
  /*
     Valeurs de retour :

     SELECT : Bouton select
     DROITE : Bouton droite
     GAUCHE : Bouton gauche

  */
  while (true) { // On attends "a jamais"
    int valeur_bouton = analogRead(RESISTANCE_BOUTONS_PIN);
    if (valeur_bouton > 610 && valeur_bouton < 620) {
      return SELECT;
    }
    else if (valeur_bouton > 850 && valeur_bouton < 860) {
      return GAUCHE;
    }
    else if (valeur_bouton > 810 && valeur_bouton < 820) {
      return DROITE;
    }
    delay(10); // On évite la surchauffe lul
  }
}

/* Ecran */
void affiche_et_log(String message) {
  lcd.clear();
  lcd.print(message);
  Serial.println(message);
}

/* Interface utilisateur */
void affiche_select_quantite(int centilitres) {
  lcd.clear();
  lcd.setCursor ( 1, 0 );
  lcd.print("Select Quantite");
  lcd.setCursor ( 4, 1 );
  lcd.print((String) centilitres + " CL");
}

int menu_selection_quantite() {
  // De base, nous avons:
  int centilitres = 15;
  int bouton_appuye;
  while (true) {
    affiche_select_quantite(centilitres);
    delay(300); // On donne le temps à l'utilisateur de relacher le bouton.
    bouton_appuye = attente_appui_bouton();

    if (bouton_appuye == SELECT) {
      return centilitres;
    }
    else if (bouton_appuye == DROITE) {
      if (centilitres < 95) {
        centilitres += 5;
      }
    }
    else if (bouton_appuye == GAUCHE) {
      if (centilitres > 10) {
        centilitres -= 5;
      }
    }
  }
}

void affichage_attente_presence_gobelet() {
  while (!goblet_present()) {
    affiche_et_log("Attente gobelet...");
    delay(500);
  }
  affiche_et_log("Gobelet detecte...");
}

void affichage_attente_absence_gobelet() {
  while (goblet_present()) {
    affiche_et_log("Fin distribution...");
    delay(500);
  }
  affiche_et_log("Merci!");
}

void affichage_distribution(double mlitres_actuels) {
  lcd.clear();
  String tosend = (String) "Total " + round(mlitres_actuels) + " mL";
  lcd.print(("Dist en cours"));
  lcd.setCursor ( 0, 1 );
  lcd.print(tosend);
}

double interruptions_to_centilitres(int interruptions) {
  // 6.3 correspond à une constante de calibration (Florian)
  return ((double) interruptions) / 0.6 / 6.3;
}

int centilitres_to_interruptions(int centilitres) {
  return round(189 * centilitres / 50);
}

bool livrer_boisson(int centilitres_voulus) {
  passage_mesure_debit = 0; // Rien dans le verre au début!
  int interruptions_voulues = centilitres_to_interruptions(centilitres_voulus);
  Serial.println("DEBUG: interruptions_voulues=" + (String) interruptions_voulues);
  ouvre_electrovanne();

  while (goblet_present()) {
    lcd.clear();

    if (passage_mesure_debit >= interruptions_voulues) {
      ferme_electrovanne();
      affiche_et_log("Dist. terminee");
      return true;
    }

    double mlitres_actuels = interruptions_to_centilitres(passage_mesure_debit) * 10;
    affichage_distribution(mlitres_actuels);
    delay(300);
  }
  ferme_electrovanne();
  Serial.println("Erreur: Le gobelet à été retiré pendant la distribution!!");
  affiche_et_log("Erreur");
  delay(500);

  return false;
}


void software_reset() {
  wdt_enable(WDTO_15MS);
  while (true)
  {
  }
}

void setup() {
  // put your setup code here, to run once:
  lcd.begin(16, 2);
  Serial.begin(9600);
  Serial.println("Starting...");

  MCUSR = 0;  // clear out any flags of prior resets.

  affiche_et_log("Configuring pins");
  // inputs
  pinMode(MESURE_DEBIT_INTERRUPT_PIN, INPUT);
  pinMode(ECHO_ULTRASONS_PIN, INPUT);

  // Outputs
  pinMode(ELECTROVANNE_PIN, OUTPUT);
  ferme_electrovanne();

  pinMode(TRIGGER_ULTRASONS_PIN, OUTPUT);
  digitalWrite(TRIGGER_ULTRASONS_PIN, LOW); // La broche TRIGGER doit être à LOW au repos

  digitalWrite(MESURE_DEBIT_INTERRUPT_PIN, HIGH); // Optional Internal Pull-Up

  affiche_et_log("Starting interrupts...");
  attachInterrupt(digitalPinToInterrupt(MESURE_DEBIT_INTERRUPT_PIN), interruption_flow, RISING); // Setup Interrupt
  sei(); // Enable interrupts

  affiche_et_log("Start complete...");
}


void loop() {
  // Alors, cette loop va etre longue... C'est parti

  // 1ere étape, déterminer la quantité d'eau à verser (à demander à l'utilisateur)
  int centilitres = menu_selection_quantite();

  // 2nde étape, attendre que le goblet soit positionné
  affichage_attente_presence_gobelet();

  // 3eme etape, verser la boisson
  bool resultat = livrer_boisson(centilitres);

  if (!resultat) {
    // Erreur :(
    affiche_et_log("SELF-RESET");
    software_reset();
  }

  // 4eme étape, attendre que l'utilateur retire le gobelet
  if (resultat) {
    affichage_attente_absence_gobelet();
  }


}
