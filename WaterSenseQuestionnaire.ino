/*
  Water Sense questionnaire sketch

  Hardware (Grove Shield for Arduino Nano v1.3, switch set to 3.3 V):
    - Grove 1.2-inch IPS Display: digital port D2 (SCK=D2, SDA=D3)
    - Grove Button (P):           digital port D4
    - Grove Rotary Angle Sensor:  analog port A0

  Libraries:
    - Adafruit GFX Library
    - Arduino_ST7789_Fast (Seeed's Grove 1.2-inch IPS display library)
*/

#include <Adafruit_GFX.h>
#include <Arduino_ST7789_Fast.h>

// Keep hardware assignments and artificial timings together for easy changes.
constexpr uint8_t DISPLAY_SCK_PIN = 2;
constexpr uint8_t DISPLAY_DATA_PIN = 3;
constexpr uint8_t CONFIRM_BUTTON_PIN = 4;
constexpr uint8_t ROTARY_PIN = A0;
constexpr uint16_t ORANGE = 0xFD20;

constexpr unsigned long BUTTON_DEBOUNCE_MS = 40;
constexpr unsigned long MEASURING_DURATION_MS = 9000;
constexpr unsigned long UPLOAD_DURATION_MS = 7500;
constexpr unsigned long LOADING_STATUS_INTERVAL_MS = 500;
constexpr unsigned long ROTARY_SAMPLE_INTERVAL_MS = 60;
constexpr int ROTARY_HYSTERESIS = 20;

Arduino_ST7789 lcd(DISPLAY_SCK_PIN, DISPLAY_DATA_PIN);

enum Screen {
  WELCOME,
  COLLECT_SAMPLE,
  OBSERVE_GUIDANCE,
  QUALITY_RATING,
  SENSE,
  MEASURING,
  BATHING_RESULTS,
  ECOSYSTEM_RESULTS,
  DETAIL_RESULTS,
  UPLOADING,
  COMPLETE
};

struct Observation {
  uint8_t quality;
  bool suitableForBathing;
  uint8_t confidencePercent;
  const char *ecosystemHealth;
  uint16_t ecosystemColor;
  uint8_t ecosystemConfidencePercent;
  int temperatureC;
  float ph;
  const char *turbidity;
  float dissolvedOxygen;
  int electricalConductivity;
};

Screen currentScreen = WELCOME;
Observation observation = {0, false, 0, "", GREEN, 0, 0, 0.0f, "", 0.0f, 0};
uint8_t selectedRating = 1;
uint8_t displayedRating = 0;
unsigned long screenStartedAt = 0;
unsigned long lastLoadingStatusAt = 0;
unsigned long lastRotarySampleAt = 0;
uint8_t loadingStatusPhase = 0;
int filteredRotaryValue = -1;

bool lastButtonReading = LOW;
bool stableButtonState = LOW;
unsigned long buttonChangedAt = 0;

void drawText(int16_t x, int16_t y, const char *value, uint8_t size, uint16_t color) {
  lcd.setTextSize(size);
  lcd.setTextColor(color, BLACK);
  lcd.setCursor(x, y);
  lcd.print(value);
}

void drawTemperatureLine(int16_t x, int16_t y, int temperatureC) {
  char value[24];
  snprintf(value, sizeof(value), "Temperature: %d", temperatureC);
  drawText(x, y, value, 2, WHITE);
  const int16_t degreeX = x + strlen(value) * 12 + 2;
  lcd.drawCircle(degreeX + 3, y + 3, 3, WHITE);
  drawText(degreeX + 9, y, "C", 2, WHITE);
}

void drawLoadingLabel(const char *label, int16_t y) {
  const int16_t labelWidth = strlen(label) * 12;
  drawText((240 - labelWidth) / 2, y, label, 2, WHITE);
}

void drawLoadingDots(const char *label, int16_t y) {
  static const char *const dots[] = {"", ".", "..", "..."};
  const int16_t labelWidth = strlen(label) * 12;  // built-in font at size 2
  const int16_t labelX = (240 - labelWidth) / 2;
  const int16_t dotsX = labelX + labelWidth;

  // The label is stable; only clear and redraw the small ellipsis area.
  lcd.fillRect(dotsX, y, 36, 18, BLACK);
  drawText(dotsX, y, dots[loadingStatusPhase], 2, WHITE);
}

const char *const DROP_FINAL[] = {
  "       /\\       ",
  "      /  \\      ",
  "     /    \\     ",
  "    /      \\    ",
  "   /        \\   ",
  "  /          \\  ",
  " |   ^    ^   | ",
  " |            | ",
  " |    \\__/    | ",
  "  \\          /  ",
  "   \\        /   ",
  "    \\______/    "
};

const char *const DROP_STATIC[] = {
  "       /\\       ",
  "      /  \\      ",
  "     /    \\     ",
  "    /      \\    ",
  "   /        \\   ",
  "  /          \\  ",
  " |            | ",
  " |            | ",
  " |            | ",
  "  \\          /  ",
  "   \\        /   ",
  "    \\______/    "
};

void drawAsciiArt(const char *const *lines, uint8_t lineCount, int16_t x, int16_t y, uint8_t textSize, uint16_t color) {
  lcd.setTextSize(textSize);
  lcd.setTextColor(color, BLACK);
  for (uint8_t line = 0; line < lineCount; ++line) {
    lcd.setCursor(x, y + line * (8 * textSize));
    lcd.print(lines[line]);
  }
}

void drawFooter(const char *message) {
  lcd.drawFastHLine(12, 199, 216, DGREY);
  if (strcmp(message, "Turn the knob, then press OK") == 0) {
    drawText(18, 204, "Turn the knob,", 2, WHITE);
    drawText(18, 220, "then press OK", 2, WHITE);
  } else if (strcmp(message, "Press OK to continue") == 0) {
    drawText(18, 204, "Press to continue", 2, WHITE);
  } else {
    drawText(18, 212, message, 2, WHITE);
  }
}

void drawRatingBox(uint8_t index, bool selected) {
  const int16_t boxWidth = 34;
  const int16_t boxHeight = 30;
  const int16_t gap = 8;
  const int16_t startX = 18;
  const int16_t y = 145;
  const int16_t x = startX + (index - 1) * (boxWidth + gap);
  const uint16_t fill = selected ? CYAN : BLACK;
  const uint16_t outline = selected ? CYAN : WHITE;
  const uint16_t textColor = selected ? BLACK : WHITE;
  char label[2] = {static_cast<char>('0' + index), '\0'};

  lcd.fillRoundRect(x, y, boxWidth, boxHeight, 5, fill);
  lcd.drawRoundRect(x, y, boxWidth, boxHeight, 5, outline);
  lcd.setTextSize(2);
  lcd.setTextColor(textColor, fill);
  lcd.setCursor(x + 11, y + 7);
  lcd.print(label);
}

void drawRatingBoxes(uint8_t rating) {
  for (uint8_t index = 1; index <= 5; ++index) {
    drawRatingBox(index, index == rating);
  }
}

void drawRatingScreen(const char *title, const char *instruction, const char *question) {
  lcd.fillScreen(BLACK);
  drawText(12, 14, title, 2, CYAN);
  drawText(12, 62, instruction, 1, WHITE);
  drawText(12, 84, question, 2, YELLOW);
  drawText(12, 120, "1 - 5", 2, WHITE);
  drawRatingBoxes(selectedRating);
  drawFooter("Turn knob, then press OK");
}

void drawQualityRatingScreen() {
  lcd.fillScreen(BLACK);
  drawText(12, 24, "Based on what you", 2, WHITE);
  drawText(12, 40, "observed and what", 2, WHITE);
  drawText(12, 56, "you know about the", 2, WHITE);
  drawText(12, 72, "place:", 2, WHITE);
  drawText(12, 96, "How would you rate", 2, YELLOW);
  drawText(12, 112, "the water quality?", 2, YELLOW);
  drawRatingBoxes(selectedRating);
  drawFooter("Turn the knob, then press OK");
}

void drawObservationGuidance() {
  lcd.fillScreen(BLACK);
  drawText(12, 14, "Observe", 2, CYAN);
  drawText(12, 52, "Look calmly at", 2, WHITE);
  drawText(12, 68, "the water.", 2, WHITE);
  drawText(12, 96, "Spend time with it;", 2, WHITE);
  drawText(12, 112, "smell, look, touch.", 2, WHITE);
  drawText(12, 144, "What do you notice?", 2, YELLOW);
  drawText(12, 160, "How does it feel?", 2, YELLOW);
  drawFooter("Press OK to continue");
}

void drawBathingResults() {
  lcd.fillScreen(BLACK);
  drawText(12, 14, "Results", 2, CYAN);
  drawText(12, 66, observation.suitableForBathing ? "Bathing: YES" : "Bathing: NO", 2,
           observation.suitableForBathing ? GREEN : RED);
  char confidenceLine[24];
  snprintf(confidenceLine, sizeof(confidenceLine), "Confidence: %d%%", observation.confidencePercent);
  drawText(12, 102, confidenceLine, 2, WHITE);
  drawText(12, 140, "Estimate from a", 2, DGREY);
  drawText(12, 156, "mathematical model;", 2, DGREY);
  drawText(12, 172, "reality may differ.", 2, DGREY);
  drawFooter("Press OK for more");
}

void drawDetailedResults() {
  char phLine[16];
  char turbidityLine[24];
  char oxygenLine[20];
  char conductivityLine[20];
  const int phTenths = static_cast<int>(observation.ph * 10.0f + 0.5f);
  const int oxygenTenths = static_cast<int>(observation.dissolvedOxygen * 10.0f + 0.5f);

  snprintf(phLine, sizeof(phLine), "pH: %d.%d", phTenths / 10, phTenths % 10);
  snprintf(turbidityLine, sizeof(turbidityLine), "Turbidity: %s", observation.turbidity);
  snprintf(oxygenLine, sizeof(oxygenLine), "DO: %d.%d mg/L", oxygenTenths / 10, oxygenTenths % 10);
  snprintf(conductivityLine, sizeof(conductivityLine), "EC: %d uS/cm", observation.electricalConductivity);

  lcd.fillScreen(BLACK);
  drawText(12, 14, "Results", 2, CYAN);
  drawTemperatureLine(12, 50, observation.temperatureC);
  drawText(12, 78, phLine, 2, WHITE);
  drawText(12, 106, turbidityLine, 2, WHITE);
  drawText(12, 134, oxygenLine, 2, WHITE);
  drawText(12, 162, conductivityLine, 2, WHITE);
  drawFooter("Press OK to upload");
}

void drawEcosystemResults() {
  lcd.fillScreen(BLACK);
  drawText(12, 14, "Results", 2, CYAN);
  drawText(12, 66, "Ecosystem health:", 2, WHITE);
  drawText(12, 102, observation.ecosystemHealth, 2, observation.ecosystemColor);

  char confidenceLine[30];
  snprintf(confidenceLine, sizeof(confidenceLine), "Confidence: %d%%", observation.ecosystemConfidencePercent);
  drawText(12, 138, confidenceLine, 2, WHITE);
  drawText(12, 164, "Model estimate;", 2, DGREY);
  drawText(12, 180, "reality may differ.", 2, DGREY);
  drawFooter("Press OK for more");
}

void renderCurrentScreen() {
  switch (currentScreen) {
    case WELCOME:
      lcd.fillScreen(BLACK);
      drawText(12, 35, "Welcome", 2, WHITE);
      drawText(12, 85, "WATER SENSE", 2, CYAN);
      drawFooter("Press to start");
      break;

    case COLLECT_SAMPLE:
      lcd.fillScreen(BLACK);
      drawText(12, 18, "Collect sample", 2, CYAN);
      drawText(12, 68, "Take a sample", 2, WHITE);
      drawText(12, 96, "of water", 2, WHITE);
      drawFooter("Press OK to continue");
      break;

    case OBSERVE_GUIDANCE:
      drawObservationGuidance();
      break;

    case QUALITY_RATING:
      drawQualityRatingScreen();
      break;

    case SENSE:
      lcd.fillScreen(BLACK);
      drawText(12, 18, "Sense", 2, CYAN);
      drawText(12, 58, "Place the", 2, WHITE);
      drawText(12, 74, "smart lid.", 2, WHITE);
      drawText(12, 104, "Make sure the", 2, WHITE);
      drawText(12, 120, "sensors are", 2, WHITE);
      drawText(12, 136, "touching the water.", 2, WHITE);
      drawFooter("Press OK to continue");
      break;

    case MEASURING:
      lcd.fillScreen(BLACK);
      drawLoadingLabel("Measuring", 108);
      drawLoadingDots("Measuring", 108);
      break;

    case BATHING_RESULTS:
      drawBathingResults();
      break;

    case ECOSYSTEM_RESULTS:
      drawEcosystemResults();
      break;

    case DETAIL_RESULTS:
      drawDetailedResults();
      break;

    case UPLOADING:
      lcd.fillScreen(BLACK);
      drawLoadingLabel("Saving", 108);
      drawLoadingDots("Saving", 108);
      break;

    case COMPLETE:
      lcd.fillScreen(BLACK);
      drawText(12, 25, "Observation saved", 2, CYAN);
      drawText(58, 65, "Thank you!", 2, WHITE);
      drawAsciiArt(DROP_FINAL, sizeof(DROP_FINAL) / sizeof(DROP_FINAL[0]), 72, 105, 1, CYAN);
      break;
  }
}

void generateSimulatedResults() {
  // Placeholder model: the user's overall impression influences the result,
  // with a little random variation until real measurements are available.
  const int modelScore = static_cast<int>(observation.quality) + random(-1, 2);
  observation.suitableForBathing = modelScore >= 3;
  observation.confidencePercent = random(68, 96);  // 68% to 95%
  const int ecosystemScore = static_cast<int>(observation.quality) + random(-1, 2);
  observation.ecosystemConfidencePercent = random(68, 96);
  if (ecosystemScore >= 4) {
    observation.ecosystemHealth = "Healthy";
    observation.ecosystemColor = GREEN;
  } else if (ecosystemScore >= 3) {
    observation.ecosystemHealth = "Stressed";
    observation.ecosystemColor = ORANGE;
  } else {
    observation.ecosystemHealth = "Degraded";
    observation.ecosystemColor = RED;
  }
  observation.temperatureC = random(5, 21);
  observation.ph = random(65, 86) / 10.0f;
  observation.dissolvedOxygen = random(45, 121) / 10.0f;
  observation.electricalConductivity = random(80, 1001);
  const int turbidityChoice = random(0, 3);
  observation.turbidity = turbidityChoice == 0 ? "Low" : turbidityChoice == 1 ? "Medium" : "High";
}

bool isRatingScreen(Screen screen) {
  return screen == QUALITY_RATING;
}

int readFilteredRotaryValue() {
  const int rawValue = constrain(analogRead(ROTARY_PIN), 0, 1023);

  // A small moving average removes normal ADC noise without making the knob feel slow.
  if (filteredRotaryValue < 0) {
    filteredRotaryValue = rawValue;
  } else {
    filteredRotaryValue = (filteredRotaryValue * 3 + rawValue) / 4;
  }

  return filteredRotaryValue;
}

uint8_t ratingFromRotaryValue(int rotaryValue) {
  return static_cast<uint8_t>(constrain((rotaryValue * 5) / 1024 + 1, 1, 5));
}

uint8_t ratingWithHysteresis(int rotaryValue, uint8_t currentRating) {
  uint8_t rating = currentRating;

  // Require the knob to move a little beyond a boundary before changing value.
  // This prevents a noisy value at a boundary from alternating between two boxes.
  while (rating < 5) {
    const int boundary = (1024 * rating) / 5;
    if (rotaryValue <= boundary + ROTARY_HYSTERESIS) {
      break;
    }
    ++rating;
  }

  while (rating > 1) {
    const int boundary = (1024 * (rating - 1)) / 5;
    if (rotaryValue >= boundary - ROTARY_HYSTERESIS) {
      break;
    }
    --rating;
  }

  return rating;
}

void enterScreen(Screen nextScreen) {
  currentScreen = nextScreen;
  screenStartedAt = millis();
  lastLoadingStatusAt = screenStartedAt;
  loadingStatusPhase = 0;

  if (isRatingScreen(currentScreen)) {
    filteredRotaryValue = -1;
    selectedRating = ratingFromRotaryValue(readFilteredRotaryValue());
    displayedRating = selectedRating;
  } else {
    displayedRating = 0;
  }

  if (currentScreen == MEASURING) {
    generateSimulatedResults();
  }

  renderCurrentScreen();
}

bool wasConfirmButtonPressed() {
  const bool reading = digitalRead(CONFIRM_BUTTON_PIN);
  const unsigned long now = millis();

  if (reading != lastButtonReading) {
    buttonChangedAt = now;
    lastButtonReading = reading;
  }

  if (now - buttonChangedAt >= BUTTON_DEBOUNCE_MS && reading != stableButtonState) {
    stableButtonState = reading;
    return stableButtonState == HIGH;
  }

  return false;
}

void handleConfirmPress() {
  switch (currentScreen) {
    case WELCOME:
      enterScreen(COLLECT_SAMPLE);
      break;
    case COLLECT_SAMPLE:
      enterScreen(OBSERVE_GUIDANCE);
      break;
    case OBSERVE_GUIDANCE:
      enterScreen(QUALITY_RATING);
      break;
    case QUALITY_RATING:
      observation.quality = selectedRating;
      enterScreen(SENSE);
      break;
    case SENSE:
      enterScreen(MEASURING);
      break;
    case BATHING_RESULTS:
      enterScreen(ECOSYSTEM_RESULTS);
      break;
    case ECOSYSTEM_RESULTS:
      enterScreen(DETAIL_RESULTS);
      break;
    case DETAIL_RESULTS:
      enterScreen(UPLOADING);
      break;
    case COMPLETE:
      // To start another observation later, uncomment the next line:
      enterScreen(WELCOME);
      break;
    default:
      // Measuring and Uploading advance automatically.
      break;
  }
}

void updateRatingSelection() {
  if (!isRatingScreen(currentScreen)) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastRotarySampleAt < ROTARY_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastRotarySampleAt = now;

  const uint8_t newRating = ratingWithHysteresis(readFilteredRotaryValue(), selectedRating);
  if (newRating != selectedRating) {
    const uint8_t previousRating = selectedRating;
    selectedRating = newRating;
    displayedRating = newRating;
    drawRatingBox(previousRating, false);
    drawRatingBox(newRating, true);
  }
}

void updateAutomaticScreens() {
  const unsigned long now = millis();

  if (currentScreen == MEASURING) {
    if (now - lastLoadingStatusAt >= LOADING_STATUS_INTERVAL_MS) {
      lastLoadingStatusAt = now;
      loadingStatusPhase = (loadingStatusPhase + 1) % 4;
      drawLoadingDots("Measuring", 108);
    }
    if (now - screenStartedAt >= MEASURING_DURATION_MS) {
      enterScreen(BATHING_RESULTS);
    }
  } else if (currentScreen == UPLOADING && now - screenStartedAt >= UPLOAD_DURATION_MS) {
    enterScreen(COMPLETE);
  } else if (currentScreen == UPLOADING && now - lastLoadingStatusAt >= LOADING_STATUS_INTERVAL_MS) {
    lastLoadingStatusAt = now;
    loadingStatusPhase = (loadingStatusPhase + 1) % 4;
    drawLoadingDots("Saving", 108);
  }
}

void setup() {
  pinMode(CONFIRM_BUTTON_PIN, INPUT);
  pinMode(ROTARY_PIN, INPUT);
  analogReadResolution(10);
  randomSeed(analogRead(A6) ^ micros());

  lcd.init();
  enterScreen(WELCOME);
}

void loop() {
  updateRatingSelection();

  if (wasConfirmButtonPressed()) {
    handleConfirmPress();
  }

  updateAutomaticScreens();
}
