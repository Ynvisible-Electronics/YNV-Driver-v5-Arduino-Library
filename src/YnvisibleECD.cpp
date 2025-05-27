/**
 * Ynvisible Electrochromic Displays driving software for Driver v5 board
 * Created by Bruno Fonseca - Ynvisible, May 2025
 */
#include "Arduino.h"
#include "YnvisibleECD.h"


/*********************** PUBLIC FUNCTIONS ************************/

/**
 * @brief Ynvisible's Electrochromic Display driver
 * 
 * @param t_numberOfSegments display's number of segments
 * @param t_segments array of segments' pins
 */
YNV_ECD::YNV_ECD(int t_numberOfSegments, int t_segments[])        // Constructor
{
  m_counterElectrodePin = PIN_CE;
  pinMode(m_counterElectrodePin, OUTPUT);

  m_numberOfSegments = t_numberOfSegments;

  analogReadResolution(ADC_DAC_RESOLUTION);
  analogWriteResolution(ADC_DAC_RESOLUTION);

  for (int i = 0; i < m_numberOfSegments; i++)
  {
    m_segmentPinsList[i] = t_segments[i];

    pinMode(m_segmentPinsList[i], INPUT);

    m_currentState[i] = SEGMENT_STATE_UNDEFINED;
    m_nextState[i] = SEGMENT_STATE_UNDEFINED;
  }
}

/**
 * This method initializes the display by setting all segments to the color state and then bleaching them.
 * It is typically called at the beginning of the program to prepare the display for use.
 */
void YNV_ECD::begin()
{
  //Color all segments
  for(int i = 0; i < m_numberOfSegments; i++){
    setSegmentState(i, true);
  }
  executeDisplay();
  
  //Bleach all segments
  for(int i = 0; i < m_numberOfSegments; i++){
    setSegmentState(i, false);
  }
  executeDisplay();

}


/**
 * Set the State of a Segment before executing the display
 * @param t_segment t_segment to change t_state
 * @param t_state new t_state of the t_segment: SEGMENT_STATE_BLEACH or SEGMENT_STATE_COLOR
*/
void YNV_ECD::setSegmentState(int t_segment, bool t_state){
  m_nextState[t_segment] = t_state;
}

/**
 * Colors and bleaches segments, depending on their state.
 * Change the segments' state with YNV_ECD.setSegmentState() and
 * then call this method to apply the new state.
*/
void YNV_ECD::executeDisplay(){
  bool isDelayRequired = false;

  enableCounterElectrode(m_cfg.bleachingVoltage);

  Serial.println("\n********* BLEACHING DISPLAY *********");
  Serial.print("Counter electrode enabled with bleaching voltage: ");
  Serial.println(m_cfg.bleachingVoltage);
  

  for (int i = 0; i < m_numberOfSegments; i++)
  {
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }

    if(m_nextState[i] != m_currentState[i] && m_nextState[i] == SEGMENT_STATE_BLEACH)
    {
      pinMode(m_segmentPinsList[i], OUTPUT);
      digitalWrite(m_segmentPinsList[i], m_nextState[i]);

      Serial.print("Segment ");
      Serial.print(i);
      Serial.print(" @ pin ");
      Serial.print(m_segmentPinsList[i]);
      Serial.println(" set to BLEACH state.");
      
      m_currentState[i] = m_nextState[i];
      isDelayRequired = true;
    }
  }
  if(isDelayRequired == true){
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }
    Serial.print("Delaying for bleaching time: ");
    Serial.println(m_cfg.bleachingTime);
    delay(m_cfg.bleachingTime);
  }
  
  disableAllSegments();
  Serial.println("All segments disabled after bleaching.");

  enableCounterElectrode(m_supplyVoltage - m_cfg.coloringVoltage);
  Serial.println("\n********* COLORING DISPLAY *********");
  Serial.print("Counter electrode enabled with coloring voltage: ");
  Serial.println(m_cfg.coloringVoltage);
  isDelayRequired = false;

  for (int i = 0; i < m_numberOfSegments; i++)
  {
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }
    if(m_nextState[i] != m_currentState[i] && m_nextState[i] == SEGMENT_STATE_COLOR)
    {
      pinMode(m_segmentPinsList[i], OUTPUT);
      digitalWrite(m_segmentPinsList[i], m_nextState[i]);

      Serial.print("Segment ");
      Serial.print(i);
      Serial.print(" @ pin ");
      Serial.print(m_segmentPinsList[i]);
      Serial.println(" set to COLOR state.");

      m_currentState[i] = m_nextState[i];
      isDelayRequired = true;
    }
  }
  if(isDelayRequired == true){
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }
    Serial.print("Delaying for coloring time: ");
    Serial.println(m_cfg.coloringTime);
    delay(m_cfg.coloringTime);
  }
  disableAllSegments();
  Serial.println("All segments disabled after coloring.");
  disableCounterElectrode();
  Serial.println("Counter electrode disabled.");

  refreshDisplay();
  if(m_stopDrivingFlag == true){
    Serial.println("Stop driving flag set. Stopping execution.");
    return;
  }
  Serial.println("Display refreshed.");
}

/**
 * Refresh the display. Either after a Display Execute or when the CPU wakes up from sleep.
 * @note the method will check if a refresh is required and return if it isn't.
*/
void YNV_ECD::refreshDisplay() //Refreshes the display to maintain the current t_state.
{
  int analog_val = 0;
  bool refresh_color_needed = false, refresh_bleach_needed = false;
  float counterElecVal = 0;
  float currentLimit = 0;
  int retries = 0;

  if(m_stopDrivingFlag == true){
    Serial.println("Stop driving flag set. Stopping execution.");
    return;
  }

  Serial.println("\n********* REFRESH CHECK *********");

  enableCounterElectrode(m_supplyVoltage / 2);
  Serial.print("Counter electrode enabled @ ");
  Serial.println(m_supplyVoltage/2);
  disableAllSegments(); // Put all pins in Input mode
  Serial.println("All segments disabled.");

  Serial.print("Refresh Color Limit Low:   ");
  Serial.print(m_refreshColorLimitL);
  Serial.println(" LSB");
  Serial.print("Refresh Bleach Limit High: ");
  Serial.print(m_refreshBleachLimitH);
  Serial.println(" LSB");

  for (int i = 0; i < m_numberOfSegments; i++) {
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }
    pinMode(m_segmentPinsList[i], INPUT);
    analog_val = analogRead(m_segmentPinsList[i]);
    Serial.print("Segment ");
    Serial.print(i);
    Serial.print(" analog value: ");
    Serial.print(analog_val);
    Serial.print(" LSB");

    if (m_currentState[i] == SEGMENT_STATE_COLOR && (analog_val < m_refreshColorLimitL)) {
      m_refreshSegmentNeeded[i] = true;
      refresh_color_needed = true;
      Serial.print(" <- needs color refresh.");
    } 
    else if (m_currentState[i] == SEGMENT_STATE_BLEACH && (analog_val > m_refreshBleachLimitH)) {
      m_refreshSegmentNeeded[i] = true;
      refresh_bleach_needed = true;
      Serial.print(" <- needs bleach refresh.");
    }
    Serial.println();
  }

  if(m_stopDrivingFlag == true){
    Serial.println("Stop driving flag set. Stopping execution.");
    return;
    }

  if ((refresh_bleach_needed || refresh_color_needed) == false) {
    Serial.println("No refresh needed.");
    return; // Return if Refresh isn't needed.
  }

  Serial.println("\n********* REFRESH BLEACH *********");
  counterElecVal = m_cfg.refreshBleachingVoltage;
  
  enableCounterElectrode(counterElecVal);

  Serial.print("Counter electrode enabled with bleaching voltage for refresh: ");
  Serial.print(counterElecVal);
  Serial.print(" [");
  Serial.print(ADC_DAC_MAX_LSB*(counterElecVal/m_supplyVoltage));
  Serial.println("]");

  Serial.print("Refresh Bleach Pulse: ");
  Serial.print(m_cfg.refreshBleachingVoltage);
  Serial.print(" V | ");
  Serial.print(m_cfg.refreshBleachingVoltage * ADC_DAC_MAX_LSB / m_supplyVoltage);
  Serial.println(" LSB");
  
  Serial.print("Refresh Bleach Pulse Time: ");
  Serial.print(m_cfg.refreshBleachPulseTime);
  Serial.println(" ms");

  Serial.print("Refresh Bleach Limit Low: ");
  Serial.print(m_refreshBleachLimitL * m_supplyVoltage / ADC_DAC_MAX_LSB);
  Serial.print(" V | ");
  Serial.print(m_refreshBleachLimitL);
  Serial.println(" LSB");

  Serial.println("----------------------------------------------------------------------------");
    
  while (refresh_bleach_needed) {
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }
    
    Serial.print("Refresh Bleach Segments: ");
    for (int i = 0; i < m_numberOfSegments; i++) {
      if(m_stopDrivingFlag == true){
        Serial.println("Stop driving flag set. Stopping execution.");
        return;
      }
      if (m_currentState[i] == SEGMENT_STATE_BLEACH && m_refreshSegmentNeeded[i] == true) {
        pinMode(m_segmentPinsList[i], OUTPUT);
        digitalWrite(m_segmentPinsList[i], LOW);
        Serial.print(i);
        Serial.print(" ");
      }
    }
    delay(m_cfg.refreshBleachPulseTime);
    disableAllSegments();
    Serial.println("");
    refresh_bleach_needed = false;

    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }


    // Check which Segments still need bleach refresh
    for (int i = 0; i < m_numberOfSegments; i++) {
      if(m_stopDrivingFlag == true){
        Serial.println("Stop driving flag set. Stopping execution.");
        return;
      }
      if (m_currentState[i] == SEGMENT_STATE_BLEACH && retries < MAX_REFRESH_RETRIES) {
        pinMode(m_segmentPinsList[i], INPUT);
        analog_val = analogRead(m_segmentPinsList[i]);
        Serial.print(" -> Segment ");
        Serial.print(i);
        Serial.print(" analog value: ");
        Serial.print(analog_val);
        if (analog_val > m_refreshBleachLimitL) {
          m_refreshSegmentNeeded[i] = true;
          refresh_bleach_needed = true;
          Serial.print(" <- still needs bleach refresh.");
        }
        Serial.println();
      }
    }
    retries++;
    Serial.print("Bleach Retry: ");
    Serial.print(retries);
    Serial.println("\n----------------------------------------------------------------------------");
    delay(500);
  }


  Serial.println("\n********* REFRESH COLOR *********");

  counterElecVal = m_supplyVoltage - m_cfg.refreshColoringVoltage;
  currentLimit = m_refreshColorLimitH;
  retries = 0;

  enableCounterElectrode(counterElecVal);
  Serial.print("Counter electrode enabled with coloring voltage for refresh: ");
  Serial.print(counterElecVal);
  Serial.print(" [");
  Serial.print(ADC_DAC_MAX_LSB*(counterElecVal/m_supplyVoltage));
  Serial.println("]");
  
  Serial.print("Refresh Color Pulse: ");
  Serial.print(m_cfg.refreshColoringVoltage);
  Serial.print(" V | ");
  Serial.print(m_cfg.refreshColoringVoltage * ADC_DAC_MAX_LSB / m_supplyVoltage);
  Serial.println(" LSB");
  
  Serial.print("Refresh Color Pulse Time: ");
  Serial.print(m_cfg.refreshColorPulseTime);
  Serial.println(" ms");

  Serial.print("Refresh Color Limit High: ");
  Serial.print(m_refreshColorLimitH * m_supplyVoltage / ADC_DAC_MAX_LSB);
  Serial.print(" V | ");
  Serial.print(m_refreshColorLimitH);
  Serial.println(" LSB");

  Serial.println("----------------------------------------------------------------------------");

  while (refresh_color_needed) {
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }

    Serial.print("Refresh Color Segments: ");
    
    for (int i = 0; i < m_numberOfSegments; i++) {
      if(m_stopDrivingFlag == true){
        Serial.println("Stop driving flag set. Stopping execution.");
        return;
      }
      if (m_currentState[i] == SEGMENT_STATE_COLOR && m_refreshSegmentNeeded[i] == true) {
        pinMode(m_segmentPinsList[i], OUTPUT);
        digitalWrite(m_segmentPinsList[i], HIGH);
        Serial.print(i);
        Serial.print(" ");
      }
    }
    Serial.println();

    delay(m_cfg.refreshColorPulseTime);
    if(m_stopDrivingFlag == true){
      Serial.println("Stop driving flag set. Stopping execution.");
      return;
    }
    disableAllSegments();
    refresh_color_needed = false;


    // Check which Segments still need Color refresh
    for (int i = 0; i < m_numberOfSegments; i++) {
      if(m_stopDrivingFlag == true){
        Serial.println("Stop driving flag set. Stopping execution.");
        return;
      }
      if (m_currentState[i] == SEGMENT_STATE_COLOR && retries < MAX_REFRESH_RETRIES) {
        pinMode(m_segmentPinsList[i], INPUT);
        analog_val = analogRead(m_segmentPinsList[i]);
        Serial.print(" -> Segment ");
        Serial.print(i);
        Serial.print(" analog value: ");
        Serial.print(analog_val);
        if (analog_val < currentLimit) {
          m_refreshSegmentNeeded[i] = true;
          refresh_color_needed = true;
          Serial.print(" <- still needs color refresh.");
        }
        Serial.println();
      }
    }
    retries++;
    Serial.print("Color Retry: ");
    Serial.print(retries);
    Serial.println("\n----------------------------------------------------------------------------");
    delay(500);
  }

  disableCounterElectrode();
  Serial.println("Counter electrode disabled.");
}

/**
 * Update the Supply Voltage value.
 * @param t_supplyVoltage new supply voltage value
*/
void YNV_ECD::updateSupplyVoltage(int t_supplyVoltage){
  m_supplyVoltage = t_supplyVoltage;
  updateRefreshLimits();
}

/**
 * @brief Set the stopDrivingFlag to true
 * 
 * Use this method to stop the current driving and return
 * to where the displayExecute() method was called
 */
void YNV_ECD::setStopDrivingFlag(){
  m_stopDrivingFlag = true;
}

/**
 * @brief Set the stopDrivingFlag to false
 * 
 * Clear the stopDrivingFlag so that the display can be driven again
 */
void YNV_ECD::clearStopDriving(){
  m_stopDrivingFlag = false;
}

/** Set all segments to bleach */
void YNV_ECD::setAllSegmentsBleach(){
  for(int i = 0; i < m_numberOfSegments; i++){
    setSegmentState(i, SEGMENT_STATE_BLEACH);
  }
}

/********************* END PUBLIC FUNCTIONS **********************/


/*********************** PRIVATE FUNCTIONS ***********************/

/**
 * Update the Refresh Limits for driving
 * Call this method whenever a parameter that influences the limits changes. e.g. Supply Voltage or Coloring Voltage.
 */
void YNV_ECD::updateRefreshLimits(void){
  m_refreshColorLimitH = ((m_supplyVoltage - m_cfg.refreshColoringVoltage) + m_cfg.refreshColorLimitHVoltage) * ADC_DAC_MAX_LSB / m_supplyVoltage;
  m_refreshColorLimitL = (m_supplyVoltage/2 +  m_cfg.refreshColorLimitLVoltage) * ADC_DAC_MAX_LSB / m_supplyVoltage;

  m_refreshBleachLimitH = (m_supplyVoltage/2 - m_cfg.refreshBleachLimitHVoltage) * ADC_DAC_MAX_LSB / m_supplyVoltage;
  m_refreshBleachLimitL = (m_cfg.refreshBleachingVoltage - m_cfg.refreshBleachLimitLVoltage) * ADC_DAC_MAX_LSB / m_supplyVoltage;
  
  Serial.println(" === Refresh Limits Updated ===");
  Serial.print("Supply Voltage: ");
  Serial.print(m_supplyVoltage);
  Serial.println(" V");
  Serial.print("Refresh Coloring Voltage: ");
  Serial.print(m_cfg.refreshColoringVoltage);
  Serial.println(" V");
  Serial.print("Refresh Bleaching Voltage: ");
  Serial.print(m_cfg.refreshBleachingVoltage);
  Serial.println(" V");

  Serial.print("Color Limit High: ");
  Serial.print(m_cfg.refreshColorLimitHVoltage);
  Serial.print("V [");
  Serial.print(m_refreshColorLimitH);
  Serial.println("]");

  Serial.print("Color Limit Low: ");
  Serial.print(m_cfg.refreshColorLimitLVoltage);
  Serial.print("V [");
  Serial.print(m_refreshColorLimitL);
  Serial.println("]");

  Serial.print("Bleach Limit High: ");
  Serial.print(m_cfg.refreshBleachLimitHVoltage);
  Serial.print("V [");
  Serial.print(m_refreshBleachLimitH);
  Serial.println("]");

  Serial.print("Bleach Limit Low: ");
  Serial.print(m_cfg.refreshBleachLimitLVoltage);
  Serial.print("V [");
  Serial.print(m_refreshBleachLimitL);
  Serial.println("]");
  Serial.println(" ==============================\n");
}

/**
 * @brief Disable all the segments
 * 
 * Sets all the segments' pins to High-Impedance
 * 
 * @note this is not the same as bleaching all the segments
 */
void YNV_ECD::disableAllSegments(){ //Put all work electrodes in High-Z mode.
  for (int i = 0; i < m_numberOfSegments; i++)
  {
    pinMode(m_segmentPinsList[i], INPUT);
  }
}

/**
 * @brief Enable the Counter Electrode's pin
 * 
 * @param t_voltage Voltage with which to drive 
 * the Counter Electrode analog pin.
 */
void YNV_ECD::enableCounterElectrode(float t_voltage) //Enable counter electrode
{
  analogWrite(m_counterElectrodePin, int(ADC_DAC_MAX_LSB*(t_voltage/m_supplyVoltage)));
  delay(50);
}

/**
 * @brief Disable the Counter Electrode's pin
 * 
 * Sets the Counter Electrode's pin to High-Impedance
 */
void YNV_ECD::disableCounterElectrode() //Set counter electrode in High-Z.
{
  analogWrite(m_counterElectrodePin, 0);
}
/********************* END PRIVATE FUNCTIONS **********************/