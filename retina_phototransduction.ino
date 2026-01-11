// =============================================================================
// RETINA PHOTOTRANSDUCTION SIMULATOR - Arduino Implementation
// Based on Hamer et al. (2005) Equations A8-A12
// CALIBRATED for realistic rod/cone responses
// =============================================================================
//
// LED OUTPUTS:
//   PURPLE (pin 3):  Rod photocurrent response (brighter = more suppression)
//   ORANGE (pin 5):  Cone photocurrent response
//   GREEN  (pin 10): Combined (scotopic/photopic weighted)
//
// INPUT:
//   Potentiometer (A0): Light intensity [R*/s] (log scale: 0.1 - 10000)
//
// UNITS:
//   I(t)  : R*/s    dt: s    R*, E*: a.u.    cGMP, Ca: μM    J: pA
//
// =============================================================================

#define POT_PIN A0

// === TIMING ===
const float dt = 0.001;        // [s] 1 ms timestep
const int STEPS_PER_LOOP = 20; // 20 ms simulated per loop
unsigned long lastLoopTime = 0;
const unsigned long LOOP_INTERVAL_MS = 20;

// =============================================================================
// PHOTORECEPTOR STRUCTURE
// =============================================================================
struct Photoreceptor {
    float tauR, tauE, gain;                    // Front-end
    float alphaMax, Kc, m, betaDark, betaSub;  // cGMP (Eq.3)
    float gDark, ncg;
    float cDark, c0, gammaCa;                  // Calcium (Eq.4)
    float Jdark, fCa;                          // Output (Eq.5)
    float Rstar, Estar, cGMP, Ca, J;           // State
};

// =============================================================================
// ROD - Human/Primate (Hamer 2005)
// Slow, sensitive, saturates at ~1000 R*/s
// 
// Steady-state: α(0.6μM) / 1.2/s = 4.0 μM ✓
// Response: 50% at ~500 R*/s, saturates by ~5000 R*/s
// =============================================================================
Photoreceptor rod = {
    .tauR = 0.080,      // [s] 80 ms
    .tauE = 0.200,      // [s] 200 ms (dominant)
    .gain = 8.0,
    
    .alphaMax = 117.13, // [μM/s]
    .Kc = 0.17,         // [μM]
    .m = 2.5,
    .betaDark = 1.2,    // [1/s]
    .betaSub = 0.1,     // [1/(s·a.u.)]
    .gDark = 4.0,       // [μM]
    .ncg = 3.0,
    
    .cDark = 0.6,       // [μM]
    .c0 = 0.01,         // [μM]
    .gammaCa = 50.0,    // [1/s]
    
    .Jdark = 20.0,      // [pA]
    .fCa = 0.17,
    
    .Rstar = 0, .Estar = 0, .cGMP = 4.0, .Ca = 0.6, .J = -20.0
};

// =============================================================================
// CONE - Human/Primate (CALIBRATED for literature-consistent operating range)
// Fast, robust, operates at high intensities without saturation
//
// From literature (Chapter 20, Table 20.1):
//   - Cone half-saturation: ~100,000-240,000 R*/s (vs rod ~500 R*/s)
//   - 20× faster τ_R and 20× faster τ_E → 400× shift in operating range
//
// Calibration:
//   α_max = 217.6 μM/s (ensures g_dark = 4.0 μM at steady state)
//   β_sub = 2.0 /s per E* (gives ~50% response at ~100,000 R*/s)
// =============================================================================
Photoreceptor cone = {
    .tauR = 0.004,      // [s] 4 ms
    .tauE = 0.010,      // [s] 10 ms
    .gain = 2.5,
    
    .alphaMax = 217.6,  // [μM/s] - calibrated for g_dark = 4.0
    .Kc = 0.20,         // [μM]
    .m = 2.5,
    .betaDark = 5.0,    // [1/s]
    .betaSub = 2.0,     // [1/(s·a.u.)] - gives ~50% at ~100,000 R*/s
    .gDark = 4.0,       // [μM]
    .ncg = 3.0,
    
    .cDark = 0.5,       // [μM]
    .c0 = 0.01,         // [μM]
    .gammaCa = 300.0,   // [1/s] τ_Ca ≈ 3 ms
    
    .Jdark = 24.0,      // [pA]
    .fCa = 0.35,
    
    .Rstar = 0, .Estar = 0, .cGMP = 4.0, .Ca = 0.5, .J = -24.0
};

// =============================================================================
// PHOTOTRANSDUCTION EQUATIONS
// =============================================================================
void stepCell(Photoreceptor &cell, float I) {
    // [Eq.1] dR*/dt = I - R*/τ_R
    float dRdt = I - cell.Rstar / cell.tauR;
    cell.Rstar += dRdt * dt;
    if (cell.Rstar < 0) cell.Rstar = 0;
    
    // [Eq.2] dE*/dt = gain × R* - E*/τ_E
    float dEdt = cell.gain * cell.Rstar - cell.Estar / cell.tauE;
    cell.Estar += dEdt * dt;
    if (cell.Estar < 0) cell.Estar = 0;
    
    // [Eq.3] dg/dt = α(c) - β(E*) × g
    float alpha = cell.alphaMax / (1.0 + pow(cell.Ca / cell.Kc, cell.m));
    float beta = cell.betaDark + cell.betaSub * cell.Estar;
    float dgdt = alpha - beta * cell.cGMP;
    cell.cGMP += dgdt * dt;
    cell.cGMP = constrain(cell.cGMP, 0.01, 20.0);
    
    // [Eq.4] dc/dt = Influx - Efflux
    float chanOpen = pow(cell.cGMP / cell.gDark, cell.ncg);
    float influx = cell.gammaCa * (cell.cDark - cell.c0) * chanOpen;
    float efflux = cell.gammaCa * (cell.Ca - cell.c0);
    cell.Ca += (influx - efflux) * dt;
    cell.Ca = constrain(cell.Ca, cell.c0, 5.0);
    
    // [Eq.5] J = -J_dark × [w_CNG × chanOpen + w_ex × caRatio] (ALGEBRAIC)
    // With correct parameters, chanOpen ∈ [0,1] and caRatio ∈ [0,1] naturally
    // No clamping needed - the physics ensures J ∈ [-Jdark, 0]
    float wCNG = 2.0 / (cell.fCa + 2.0);
    float wEx = cell.fCa / (cell.fCa + 2.0);
    float caRatio = (cell.Ca - cell.c0) / (cell.cDark - cell.c0);
    cell.J = -cell.Jdark * (wCNG * chanOpen + wEx * caRatio);
}

// =============================================================================
// I/O CONVERSION
// =============================================================================
float potToIntensity(int pot) {
    float norm = pot / 1023.0;
    return pow(10.0, -1.0 + norm * 5.0);  // 0.1 to 10000 R*/s
}

int currentToLED(float J, float Jdark) {
    float norm = constrain(1.0 + J / Jdark, 0.0, 1.0);
    return (int)(norm * 255);
}

// =============================================================================
// SETUP & LOOP
// =============================================================================
void setup() {
    pinMode(3, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(10, OUTPUT);
    
    Serial.begin(115200);
    Serial.println(F("=== Phototransduction Simulator (Hamer 2005) ==="));
    Serial.println(F("CALIBRATED: Rod saturates ~1000 R*/s, Cone ~50% at 5000 R*/s"));
    Serial.println(F(""));
    lastLoopTime = millis();
}

void loop() {
    unsigned long now = millis();
    if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
    lastLoopTime = now;
    
    float I = potToIntensity(analogRead(POT_PIN));
    
    for (int i = 0; i < STEPS_PER_LOOP; i++) {
        stepCell(rod, I);
        stepCell(cone, I);
    }
    
    int rodLED = currentToLED(rod.J, rod.Jdark);
    int coneLED = currentToLED(cone.J, cone.Jdark);
    
    // Combined: scotopic (rod) → photopic (cone) transition
    float logI = log10(I);
    float coneWt = constrain((logI + 1.0) / 4.0, 0.0, 1.0);
    float rodWt = 1.0 - coneWt;
    float rodN = constrain(1.0 + rod.J / rod.Jdark, 0.0, 1.0);
    float coneN = constrain(1.0 + cone.J / cone.Jdark, 0.0, 1.0);
    int combLED = (int)(constrain(rodWt * rodN + coneWt * coneN, 0.0, 1.0) * 255);
    
    analogWrite(3, rodLED);
    analogWrite(5, coneLED);
    analogWrite(10, combLED);
    
    // Debug output
    static unsigned long lastPrint = 0;
    if (now - lastPrint > 500) {
        lastPrint = now;
        Serial.print(F("I=")); Serial.print(I, 0);
        Serial.print(F(" R*/s | Rod: g=")); Serial.print(rod.cGMP, 3);
        Serial.print(F(" J=")); Serial.print(rod.J, 2);
        Serial.print(F("pA (")); Serial.print(100*(1+rod.J/rod.Jdark), 1);
        Serial.print(F("%) | Cone: g=")); Serial.print(cone.cGMP, 3);
        Serial.print(F(" J=")); Serial.print(cone.J, 2);
        Serial.print(F("pA (")); Serial.print(100*(1+cone.J/cone.Jdark), 1);
        Serial.print(F("%) | LED: ")); Serial.print(rodLED);
        Serial.print(F("/")); Serial.print(coneLED);
        Serial.print(F("/")); Serial.println(combLED);
    }
}
