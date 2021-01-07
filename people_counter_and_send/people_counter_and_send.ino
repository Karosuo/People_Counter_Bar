/**
  Counts people going in and out from a taxi or similar transport

  Program implies that the physical system always distributes the odd sensors to activate first when person is going in, and the even sensors to activate after in the same process

  There will be 4 sensors, but the logic applies for odd sensors as the IN sensors (1, 3, 5...), and even sensors as OUT sensors (2, 4, 6...)
  If the IN sensors activate first, then the OUT sensors and then both are gone, means a person going IN.
  If the OUT sensors activate first, then the IN sensors and then both are gone, means a person going OUT

  If a person stays for 3 seconds or more blocking the sensors, speaker will start beeping (This time can be changed)
  If a persons stayed for another 3 secods while speaker were beeping, then speaker will hold the beep indefinitely until block disapears (This time also can be changed)

  (NOT YET) The program also saves in the eeprom the following:
  - The timestamp when counters started (is written each time the counters are resetted)
  - The total number of people in
  - The total number of people out
  - The total numebr of short blockings (people blocking sensors for 3 seconds or less)
  - The total number of long blockings (when people blocked the sensors for more than 3 seconds)


  Developers
  Rafael Karosuo (r.karosuo@bitebyte.com.mx)
  César Martínez (cesar.martinez18@tectijuana.edu.mx)

*/

/*------------INCLUDE DIRECTIVES---------------------------------*/
#include <SharpDistSensor.h>
#include "k_ring_queue.h" //Ring queue utility
/*---------------------------------------------------------------*/

/*------------PRE-PROCESOR DIRECTIVES----------------------------*/
#define NUM_OF_SENSORS 4
#define SENSOR_WINDOW_SIZE 5 //As in the library "Window size of the median filter (1 = no filtering)" and "should be an odd positive integer", "5" arbitrarily taken from all their examples
#define SENSOR_REF_DISTANCE_MMS 450 //Distance threshold in millimeters, less than this distance means the sensor is blocked, sensor not activated otherwise
#define SPEAKER_MILLIS_BEEPING_THRESHOLD 2000
#define SPEAKER_MILIS_HOLD_BEEP_THRESHOLD 6000
#define SPEAKER_PIN 7
#define BITS_PER_STATE 3
#define TIME_BTW_SAMPLES 0 //Define the time in milliseconds between reading the sensors and the next time it reads them
#define BEEPING_INTERVAL 30 //Speaker Beeping sound interval in millisecs, how long does it stay on and then off
#define EEPROM_SAVING_INTERVAL 500 //How often the eeprom will save the data in milliseconds
#define SEND_HTTP_REQUEST_INTERVAL 6000 //HTTP request sent each this amount of milliseconds
#define DEBUG 1 //If defined (not necessarily with value 1, any value works), then the MCU will send by UART0 the debug prints
/*------------------------------------------------------------*/

/*--------------GLOBAL VARIABLES-------------------------*/

//API URL receiver
String direccion = "POST /voip?datos=%data% HTTP/1.1\r\nHost: app.debitum.net:81\r\n\r\n";

//Analog inputs A0 to AN
/* The instances array index is the equivalent analog pin*/
//The sensor readings in millimeters, measuring distances up to 1500
/*Separate values for each sensor allows to compare who had
  the first reading pair

  Use extern to make it global, but define it in the setup, since SharpDistSensor need to be initialized
*/
//SharpDistSensor * sensor_instances_array[NUM_OF_SENSORS]; //Hold the sensor instances

//Holds the 1 for blocked sensor or 0 otherwise, for each sensor
int sensor_norm_values[NUM_OF_SENSORS];

//The time start point from where the blocked time interval is counted (current_time - start_blocked_millis)
unsigned long start_blocked_millis = 0;

//The time where the last sensor read occured
unsigned long last_sensors_read_millis = 0;


//Indicates if the speaker is in the beeping state
unsigned int speaker_beeping = 0;
unsigned long start_beeping_millis = 0; //Time start point from where count the beeping interval
boolean first_time_speaker_sound = true; //Defines if the sensors were free, and then the first time they're blocked means at least is a short block
//So the speaker should sound, but just need to count as 1 block no matter how many times the speaker on/off intervals
//Therefore it counts the first time it enters into the block state, and it resets when the sensors are freed

//Indicates the last time the data were saved in the eeprom
unsigned long last_eeprom_save = 0;
/**
  For more explanations on state logic, go to the in_or_out function declaration
*/
//Sensor states
/*
  000 S0
  001 S1_IN
  010 S2_IN
  011 S3_IN
  100 S1_OUT
  101 S2_OUT
  110 S3_OUT

  Logic tables take these thre bits as b1, b2 and b3 from left to right
*/
char current_state[BITS_PER_STATE];

//Persons going in
unsigned long persons_in_counter = 0;

//Persons going out
unsigned long persons_out_counter = 0;

//Short blocks counter
unsigned long short_block_counter = 0;

//Long blocks counter
unsigned long long_block_counter = 0;

//Eeprom base address, address with a non zero value, it's where the counters were written
unsigned long eeprom_base_address = 0;
/*------------------------------------------------------------*/

unsigned long http_last_request_millis;  //The last time the HTTP request was sent, starts counte time in setup(), serves as ref with millis to know when to send the next HTTP
char aux_str[70];

/*--------FUNCTION PROTOTYPES----------------------------------*/
/**
  Descriptions are within each function
*/
int is_sensor_blocked(int sensor_index, int threshold);
int is_sensor_blocked_by_value(int sensor_value, int threshold);
int in_or_out(int samples[NUM_OF_SENSORS]);
unsigned long get_eeprom_last_used_address();
void eeprom_write_counters();
void eeprom_reset();
/*------------------------------------------------------------*/

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); //Configura velocidad del puerto serie del Arduino
  Serial1.begin(9600); //Serial1 on pins 19 (RX) and 18 (TX), Configura velocidad del puerto serie para el Serial1
  power_on(); 
  iniciar();
  http_last_request_millis = millis();
  //Configure the speaker
  pinMode(SPEAKER_PIN, OUTPUT);

  memset(current_state, 0, sizeof(current_state)); //Initialize the current state as S0
  Serial.begin(9600); //Setup UART0 at 9600 baud

  /*
    //Initialize the sensor instances
    int sensor_index = 0;
    for(sensor_index = 0; sensor_index < NUM_OF_SENSORS; sensor_index++)
    {
    sensor_instances_array[sensor_index] = & SharpDistSensor((const byte)sensor_index, (const byte) SENSOR_WINDOW_SIZE);
    }*/
}

/**
  TEMPORAL ADD
  to bypass the need of an empty constructor in the SharpDistSensor class
*/
SharpDistSensor sensor1((const byte)A0, (const byte) SENSOR_WINDOW_SIZE);
SharpDistSensor sensor2((const byte)A1, (const byte) SENSOR_WINDOW_SIZE);
SharpDistSensor sensor3((const byte)A2, (const byte) SENSOR_WINDOW_SIZE);
SharpDistSensor sensor4((const byte)A3, (const byte) SENSOR_WINDOW_SIZE);

void loop() {
  // put your main code here, to run repeatedly:  

  int pin_index = 0; //The index pin reference, each sensor's array index is an analog pin
  unsigned long current_time = millis();
  int all_sensors_clear = true; //Indicates that all the sensors are unblocked

  if (current_time - http_last_request_millis >= SEND_HTTP_REQUEST_INTERVAL)  //test whether the period has elapsed
  {
    PeticionHttp("32.5375967,-117.0076339,0," + String(persons_in_counter) + ",1,1," + String(persons_out_counter) + "," + String(short_block_counter) + "," + String(long_block_counter) + ",1,1");
    http_last_request_millis = current_time;
  }


  /**
    TEMPORAL ADD
    to bypass the need of an empty constructor in the SharpDistSensor class
  */

  int temp_sensor_values[NUM_OF_SENSORS];

  temp_sensor_values[0] = sensor1.getDist();
  temp_sensor_values[1] = sensor2.getDist();
  temp_sensor_values[2] = sensor3.getDist();
  temp_sensor_values[3] = sensor4.getDist();


  #ifdef DEBUG
    Serial.print("Sensor1: ");
    Serial.print(sensor1.getDist() < SENSOR_REF_DISTANCE_MMS ? 1 : 0);
    Serial.print(" ");
    Serial.print("Sensor3: ");
    Serial.print(sensor3.getDist() < SENSOR_REF_DISTANCE_MMS ? 1 : 0);
    Serial.print(" ");
    Serial.print("Sensor2: ");
    Serial.print(sensor2.getDist() < SENSOR_REF_DISTANCE_MMS ? 1 : 0);
    Serial.print(" ");
    Serial.print("Sensor4: ");
    Serial.print(sensor4.getDist() < SENSOR_REF_DISTANCE_MMS ? 1 : 0);
    Serial.print(" ");
  #endif


  /*--------------READ SAMPLES AND CHECK IF SENSOR IS BLOCKED, START TIMER IF IT IS-------------------*/

  if (current_time - last_sensors_read_millis > TIME_BTW_SAMPLES) //Check if it's time to read sensors
  {
    for (pin_index = 0; pin_index < NUM_OF_SENSORS; pin_index++)
    {
      sensor_norm_values[pin_index] = is_sensor_blocked_by_value(temp_sensor_values[pin_index], SENSOR_REF_DISTANCE_MMS); //Measure the sensor and check if it's blocked

      //Start the blocking timer only if was not previously started
      if (sensor_norm_values[pin_index] == 1 && start_blocked_millis == 0)
      {
        start_blocked_millis = millis(); //Start blocking time
      }

      //Check if one of the sensors is blocked
      if (sensor_norm_values[pin_index] == 1)
      {
        all_sensors_clear = false; //If there is one blocked, then avoid shutting down the speaker
      }
    }
    last_sensors_read_millis = millis(); //Update the last sensors read time reference
  }


  /*-----------------------------------------------*/

  /*------GO FOR STATE MACHINE TO SEE IF A PERSON WENT IN OR OUT OR NOTHING-----------------------------------------*/
  in_or_out(sensor_norm_values);
  /*-----------------------------------------------------------------------------------------------------------------*/

  /*-----------SEND DEBUG INFORMATION USING UART AND IF NECESSARY STOP SPEAKER AND ITS TIMERS----------------------------------------------------*/

#ifdef DEBUG
  int debug_state_index = 0;
  Serial.print("  ");
  for (debug_state_index = 0; debug_state_index < BITS_PER_STATE; debug_state_index++)
  {
    Serial.print(current_state[debug_state_index]);
  }
  Serial.print(" ");
  Serial.print("IN: ");
  Serial.print(persons_in_counter);
  Serial.print(" ");
  Serial.print("OUT: ");
  Serial.print(persons_out_counter);
  Serial.print(" ");
  Serial.print("SHORT BLOCK: ");
  Serial.print(short_block_counter);
  Serial.print(" ");
  Serial.print("LONG BLOCK: ");
  Serial.print(long_block_counter);
  Serial.print(" ");
  Serial.print("Start block millis interval: ");
  Serial.print(current_time - start_blocked_millis);
  Serial.println();
#endif

  //If sensors are unblocked but were, then turn off speaker and it's timers
  if (all_sensors_clear && start_blocked_millis != 0)
  { //Means was counting blocked time but now all sensors was unblocked, so reset time counting
    digitalWrite(SPEAKER_PIN, LOW); //Turn off the Speaker if it was on, since all sensors unblocked
    start_blocked_millis = 0; //Reset the blocking start point, if zero means no block
  }


  /*---------------------------------------------------------------*/

  /*----CHECK TIMING FOR BLOCKINGS AND TURN SPEAKER ON IF BLOCKED FOR SHORT TIME OR LONG--------*/

  //Also check the difference only when the start_bloked millis ref is set, since if not set means the sensor was not been blocked
  //Ensure it's only within the short block and long block interval
  if ((current_time - start_blocked_millis) >= SPEAKER_MILLIS_BEEPING_THRESHOLD && (current_time - start_blocked_millis) < SPEAKER_MILIS_HOLD_BEEP_THRESHOLD && start_blocked_millis != 0)
  {
    if (current_time - start_beeping_millis >= BEEPING_INTERVAL) //If silence or beeping dured long enough, let's change the state
    {
      if (speaker_beeping) //Toggle the speaker at the BEEPING_INTERVAL
      {
        speaker_beeping = 0; //false
      }
      else
      {
        speaker_beeping = 1; //true
        //if BEEPING_INTERVAL < SPEAKER_MILLIS_BEEPING_THRESHOLD means it will turn on the beeping speaker several times before unblocking or before the long beep start
        //So we need to increment the short_blocked_counter just once, used a flag to identify it
        if (first_time_speaker_sound)
        {
          short_block_counter = short_block_counter + 1; //Increment the short blocks counter
          first_time_speaker_sound = false; //Next time the threshold is met, will be within the same block state situation but just different on/off interval, no need to count as a different short block
        }


      }
      start_beeping_millis = millis(); //Re-Start the reference to know how long a beep last
    }

    digitalWrite(SPEAKER_PIN, speaker_beeping); //Activate or shut down the speaker at BEEPING_INTERVAL
  }

  //If the block stays for long enough, speaker will complain in a holded fashion
  //Ensure it comes from the short block with first_time_speaker_sound, since next time this condition match could be in a long block hold, and not the first time
  if ((current_time - start_blocked_millis) >= SPEAKER_MILIS_HOLD_BEEP_THRESHOLD && start_blocked_millis != 0 && first_time_speaker_sound == false)
  {
    start_beeping_millis = 0; //Reset the 3 second beeping function to disable it
    digitalWrite(SPEAKER_PIN, HIGH); //Indefinitelly start speaker
    long_block_counter = long_block_counter + 1; //Increment the counter of the long_blocks
    first_time_speaker_sound = true; //Next time the speaker sound, it will be a different block situation
  }


  /*-----------------------------------------------------------*/

}//End loop


/*---------------FUNCTION DECLARATIONS------------------------*/
int in_or_out(int samples[NUM_OF_SENSORS])
{
  /**
    Check an array of measures, where there is a measure per sensor, 1 for sensor activated by value above the threshold, 0 for sensor not activated
    Remembering that ODD sensors are fisically first and the EVEN sensors are after.
    Both processes, IN or OUT start in state 0 (S0), so just numbering steps after this state
    For IN process:
    state 1 (S1_IN) = All odd sensors are 1, all the even sensors are 0
    state 2 (S2_IN) = All the sensors are 1 (person is half the way)
    state 3 (S3_IN) = All the odd sensors are 0, all the even sensors are 1
    state 4 (S0) = is the state 0 again

    For out process:
    state 1 (S1_OUT) = All odd sensors are 0, all the even sensors are 1
    state 2 (S2_OUT) = All the sensors are 1 (person is half the way)
    state 3 (S3_OUT) = All the odd sensors are 1, all the even sensors are 0
    state 4 (S0) = is the state 0 again

    State bits reference
    000 S0
    001 S1_IN
    010 S2_IN
    011 S3_IN
    100 S1_OUT
    101 S2_OUT
    110 S3_OUT

    Logit tables take these thre bits as b1, b2 and b3 from left to right

    RETURN
    1 , if it's an in
    0 , if it's an OUT
    -1, if it's a stuck state, nothing is happening
  */

  /*
     All the same group sensors are bitwise ORed and if they follow the state machine, then it's a correct in or out
    This is because if someone jumps some of the sensors or some of the sensors fail or something related
  */


  int all_odd_sensors = 0; //Represent the bitwise "OR" of all the odd sensors
  int all_even_sensors = 0; // Bitwise "OR" of all even sensors
  int returned_value = -1; //If for no IN or OUT, then -1 by default
  int pin_index = 0;
  int state_index = 0;

  //Calculates the current state
  for (pin_index = 1; pin_index <= NUM_OF_SENSORS; pin_index++) //Index starts on 1 to handle the odd/even comparison
  {
    if (pin_index % 2 == 0) //If it's an even sensor
    {
      all_even_sensors =  all_even_sensors | samples[pin_index - 1]; //Index -1 to handle zero position
    }
    else //if it's an odd sensor
    {
      all_odd_sensors =  all_odd_sensors | samples[pin_index - 1];
    }
  }

  //Check which is the next state, from the current plus the inputs (odd or even sensors activation)
  //Equations are as follows:
  /**
     S0 = AE' * AO' + AE * b1'b2'b3' * AO
     S1_IN = AE' * b1b2'b3' * AO + AO * b1' * AE'
     S2_IN = AE * b1'b2 * AO + AE * b1'b3 * AO
     S3_IN = AE * b1'b2 * AO
     S1_OUT = AE * b1b3' * AO' + AE * AO' * b2'
     S2_OUT = AE * b1b3' * AO + AE * b1b2' * AO
     S3_OUT = b1b2b3' * AO * AE' + b1b2'b3 * AO * AE'

     Where b1, b2 and b3 are the index 0, 1 and 2 of the current_state array
  */
  if ((!all_even_sensors && !all_odd_sensors) || (all_even_sensors && all_odd_sensors && (!((int)current_state[0] - 0x30) && !((int)current_state[1] - 0x30) && !((int)current_state[2] - 0x30))))
  {
    //If state zero is activated, let's check if it comes from S3_OUT, S3_IN or just hanging there in S0
    if (strcmp(current_state, "011") == 0 || strcmp(current_state, "010") == 0) //If comes from S3_IN, increment people in
    {
      persons_in_counter = persons_in_counter + 1;
      returned_value = 1; //Return a 1, as it's an IN
    }
    else if (strcmp(current_state, "110") == 0 || strcmp(current_state, "101") == 0) //If comes from S3_OUT, increment people out
    {
      persons_out_counter = persons_out_counter + 1;
      returned_value = 0; //Return a 0, as it's an OUT
    }

    //If just hanging there in S0, do nothing

    strcpy(current_state, "000"); //Go to S0
  }
  else if ((!all_even_sensors && (((int)current_state[0] - 0x30) && !((int)current_state[1] - 0x30) && !((int)current_state[2] - 0x30)) && all_odd_sensors) || (all_odd_sensors && !all_even_sensors && (!((int)current_state[0] - 0x30))))
  {
    strcpy(current_state, "001"); //Go to S1_IN
  }
  else if ((all_even_sensors && (!((int)current_state[0] - 0x30) && ((int)current_state[1] - 0x30)) && all_odd_sensors) || (all_even_sensors && (!((int)current_state[0] - 0x30) && ((int)current_state[2] - 0x30)) && all_odd_sensors))
  {
    strcpy(current_state, "010"); //Go to S2_IN
  }
  else if (all_even_sensors && (!((int)current_state[0] - 0x30) && ((int)current_state[1] - 0x30)) && !all_odd_sensors)
  {
    strcpy(current_state, "011"); //Go to S3_IN
  }
  else if ((all_even_sensors && (((int)current_state[0] - 0x30) && !((int)current_state[2] - 0x30)) && !all_odd_sensors) || (all_even_sensors && !all_odd_sensors && !((int)current_state[1] - 0x30)))
  {
    strcpy(current_state, "100"); //Go to S1_OUT
  }
  else if ((all_even_sensors && (((int)current_state[0] - 0x30) && !((int)current_state[2] - 0x30)) && all_odd_sensors) || (all_even_sensors && (((int)current_state[0] - 0x30) && !((int)current_state[1] - 0x30)) && all_odd_sensors))
  {
    strcpy(current_state, "101"); //Go to S2_OUT
  }
  else if (((((int)current_state[0] - 0x30) && ((int)current_state[1] - 0x30) && !((int)current_state[2] - 0x30)) && all_odd_sensors && !all_even_sensors) || (all_odd_sensors && !all_even_sensors && (((int)current_state[0] - 0x30) && !((int)current_state[1] - 0x30) && ((int)current_state[2] - 0x30))))
  {
    strcpy(current_state, "110"); //GO to S3_OUT
  }


  return returned_value;
}

int is_sensor_blocked(int sensor_index, int threshold)
{ /**
    Determine if the sensor value (pin_number determines which sensor) is lower or not regarding the threshold
    If the sensor value is lower than threshold, means the sensor has been blocked, otherwise is not blocked

    RETURN VALUE
    1 if the sensor is blocked
    0 if not blocked

    Using the Sharp sensor library provided by Julien de la Bruère-Terreault (drgfreeman@tuta.io)
    Found in github
    https://github.com/DrGFreeman/SharpDistSensor

  */
  /*if( (*sensor_instances_array[sensor_index]).getDist() < threshold )
    {
    return 1; //If closer distance, means something got in the way
    }
  */
  return 0; //Otherwise, it's a not activated sensor

}

int is_sensor_blocked_by_value(int sensor_value, int threshold)
{ /**
    Just determines if the sensor_value is above or not regarding the threshold
    If it is above, the sensor is not activated
    If it is bellow, means something got in the way

    RETURN VALUE
    1 if sensor is blocked
    0 if sensor is not
  */

  if (sensor_value < threshold)
  {
    return 1;
  }

  return 0;

}
/*------------------------------------------------------------*/

int enviarAT(String ATcommand, const char* resp_correcta, unsigned int tiempo)
{

  int x = 0;
  bool correcto = 0;
  char respuesta[100];
  unsigned long anterior;
  memset(respuesta, '\0', 100); // Inicializa el string
  delay(100);
  while ( Serial1.available() > 0) Serial1.read(); // Limpia el buffer de entrada
  Serial1.println(ATcommand); // Envia el comando AT
  x = 0;
  anterior = millis();
  // Espera una respuesta
  do {
    // si hay datos el buffer de entrada del UART lee y comprueba la respuesta
    if (Serial1.available() != 0)
    {
      respuesta[x] = Serial1.read();
      x++;
      // Comprueba si la respuesta es correcta
      if (strstr(respuesta, resp_correcta) != NULL)
      {
        correcto = 1;
      }
    }
  }
  // Espera hasta tener una respuesta
  while ((correcto == 0) && ((millis() - anterior) < tiempo));

  return correcto;
}


void power_on()
{
  int respuesta = 0;
  // Comprueba que el modulo Serial1 esta arrancado
  if (enviarAT("AT", "OK", 2000) == 0)
  {
    Serial.println("Encendiendo el GPRS...");
    // Espera la respuesta del modulo Serial1
    while (respuesta == 0) {
      // Envia un comando AT cada 2 segundos y espera la respuesta
      respuesta = enviarAT("AT", "OK", 2000);
      Serial1.println(respuesta);
    }
  }
}

void reiniciar()
{
  Serial.println("Reiniciando...");
  power_on();
}

void iniciar()
{
  //Espera hasta estar conectado a la red movil
  while ( enviarAT("AT+CREG?", "+CREG: 0,1", 1000) == 0 )
  {
  }

  Serial.println("Conectado a la red.");
  enviarAT("AT+CGATT=1\r", "OK", 1000); //Iniciamos la conexión GPRS
  enviarAT("AT+CIFSR", "", 3000); //Activamos el perfil de datos inalámbrico
}

void PeticionHttp(String parametros)
{
  if (enviarAT("AT+CREG?", "+CREG: 0,1", 1000) == 1) //Comprueba la conexion a la red
  {
    enviarAT("AT+CIPSTART=\"TCP\",\"app.debitum.net\",\"81\"", "CONNECT OK", 5000); //Inicia una conexión TCP
    // Envíamos datos a través del TCP
    String temp = direccion;
    temp.replace("%data%", parametros);
    sprintf(aux_str, "AT+CIPSEND=%d", strlen(temp.c_str()));
    if (enviarAT(aux_str, ">", 1000) == 1)
    {
      enviarAT(temp, "OK", 1000);
    }
  }
  else
  {
    reiniciar();
    iniciar();
  }
}
