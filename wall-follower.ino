#include <Servo.h>
#include "Adafruit_VL53L0X.h"


Adafruit_VL53L0X lox = Adafruit_VL53L0X();


/**
 * Define PIN
 */
// Motor pin
#define RIGHT_SERVO_PWM 5
#define RIGHT_SERVO_IN_2 A0
#define RIGHT_SERVO_IN_1 7
#define LEFT_SERVO_PWM 3
#define LEFT_SERVO_IN_1 9
#define LEFT_SERVO_IN_2 8

// Contact sensor pin
#define SENSOR_LEFT 2
#define SENSOR_RIGHT 12
#define SENSOR_FRONT 4

// Servo for bee
#define BEE_RIGHT_PIN 10
#define BEE_LEFT_PIN 11

// Jumper pin
#define JUMPER 13 // Jumper pin

// Flag pin
#define FLAG_PIN 6

// Bee position
#define BEE_LEFT_CLOSE 175
#define BEE_RIGHT_CLOSE 10
#define BEE_LEFT_OPEN 30
#define BEE_RIGHT_OPEN 170

// Flag position
#define HIDE 130
#define VISIBLE 40


// Direction of robot
#define LEFT 0
#define RIGHT 1
int direction = LEFT;

// State
#define IDLE 0 // Wait jumper insertion
#define WAIT 1 // Wait match start (eject jumper)
#define STARTED 2
#define OBSTACLE 3
#define BEE 4
#define END 5

#define COEF_DIRECTION 0.6

#define MATCH_DURATION 90000

Servo pushBeeLeft;
Servo pushBeeRight;
Servo flag;

// Read jumper return 1 if present, 0 else
int readJumper()
{
    return digitalRead(JUMPER);
}
void move();
void stop();
int detect();
void bee();
void installTouillette();

void setup()
{
    pinMode(RIGHT_SERVO_PWM, OUTPUT);
    pinMode(RIGHT_SERVO_IN_2, OUTPUT);
    pinMode(RIGHT_SERVO_IN_1, OUTPUT);
    pinMode(LEFT_SERVO_PWM, OUTPUT);
    pinMode(LEFT_SERVO_IN_1, OUTPUT);
    pinMode(LEFT_SERVO_IN_2, OUTPUT);

    pinMode(SENSOR_LEFT, INPUT_PULLUP);
    pinMode(SENSOR_RIGHT, INPUT_PULLUP);
    pinMode(SENSOR_FRONT, INPUT_PULLUP);

    pinMode(JUMPER, INPUT);

    pinMode(FLAG_PIN, OUTPUT);

    // Set direction : ALWAYS FORWARD
    digitalWrite(RIGHT_SERVO_IN_2, LOW);
    digitalWrite(RIGHT_SERVO_IN_1, HIGH);
    digitalWrite(LEFT_SERVO_IN_2, HIGH);
    digitalWrite(LEFT_SERVO_IN_1, LOW);

    pushBeeLeft.attach(BEE_LEFT_PIN);
    pushBeeRight.attach(BEE_RIGHT_PIN);
    flag.attach(FLAG_PIN);
    flag.write(HIDE);

    pushBeeLeft.write(BEE_LEFT_CLOSE);
    pushBeeRight.write(BEE_RIGHT_CLOSE);

    Serial.begin(115200);
    Serial.println("IDLE");

    lox.begin();
}

void loop()
{
    static unsigned int state = IDLE;
    static unsigned long started_time = 0;
    static unsigned int bee_break = 0;

    switch(state)
    {
        case IDLE: // Wait jumper

            // If jumper is inserted go to next state
            if (readJumper()) {
                if (!digitalRead(SENSOR_RIGHT))
                    direction = RIGHT;
                state = WAIT;
                Serial.println("GO TO WAIT START");
            }

            // Move arm to show position detected by robot
            if (!digitalRead(SENSOR_RIGHT))
            {
                pushBeeRight.write((BEE_RIGHT_OPEN + BEE_RIGHT_CLOSE) / 2);
                pushBeeLeft.write(BEE_LEFT_CLOSE);
            }
            else if (!digitalRead(SENSOR_LEFT))
            {
                pushBeeRight.write(BEE_RIGHT_CLOSE);
                pushBeeLeft.write((BEE_LEFT_OPEN + BEE_LEFT_CLOSE) / 2);
            }
            else
            {
                pushBeeRight.write(BEE_RIGHT_CLOSE);
                pushBeeLeft.write(BEE_LEFT_CLOSE);
            }

            break;


        case WAIT: // Wait begin of match

            // Set small perimeter (close arms)
            pushBeeRight.write(BEE_RIGHT_CLOSE);
            pushBeeLeft.write(BEE_LEFT_CLOSE);

            // Jumper is removed
            if (!readJumper()) {
                started_time = millis(); // Register begin time of match
                installTouillette(); // Deploy robot (to push bee)
                state = STARTED; // Set state to start
                Serial.println("STARTED");
            }
            break;


        case STARTED: // Wait end of match or obstacle

            // End of match ?
            if (millis() - started_time >= MATCH_DURATION) {
                bee();
                Serial.println("END_OF_MATCH");
                state = END;
            }

            // Obstacle ?
            if (detect()) {
                Serial.println("OBSTACLE OBSTACLE OBSTACLE");
                state = OBSTACLE;
            }

            // End of the table
            if (!digitalRead(SENSOR_FRONT)) {
                Serial.println("Go to BEE");
                state = BEE;
            }
            move();
            break;


        case OBSTACLE: // Wait end of match or no obstacle

            // End of match
            if (millis() - started_time >= MATCH_DURATION) {
                bee();
                Serial.println("END_OF_MATCH");
                state = END;
            }

            // No obstacle
            if (! detect()) {
                Serial.println("No more obstacle");
                state = STARTED;
            }

            // End of the table
            if (!digitalRead(SENSOR_FRONT)) {
                Serial.println("Go to BEE from OBSTACLE");
                state = BEE;
            }
            stop();
            break;


        case BEE: // Push bee and stop program
            stop();
            bee_break = 1;
            bee();
            state = END;
            break;


        case END: // End of program
            stop();
            if (bee_break) { // Security : push the bee
                delay(2000);
                flag.write(VISIBLE); // Display number of points
                bee_break = 0;
            }
            break;
    }
}

// Set good position of arm to push bee
void installTouillette()
{
    if (direction == RIGHT) {
        pushBeeRight.write((BEE_RIGHT_OPEN + BEE_RIGHT_CLOSE) / 2);
        pushBeeLeft.write(BEE_LEFT_CLOSE);
    } else {
        pushBeeRight.write(BEE_RIGHT_CLOSE);
        pushBeeLeft.write((BEE_LEFT_OPEN + BEE_LEFT_CLOSE) / 2);
    }
}

// Push the bee
void bee()
{
    if (direction == RIGHT)
    {
        pushBeeRight.write(BEE_RIGHT_CLOSE);
        delay(1000);
        pushBeeLeft.write(BEE_LEFT_OPEN);
    }
    else
    {
        pushBeeLeft.write(BEE_LEFT_CLOSE);
        delay(1000);
        pushBeeRight.write(BEE_RIGHT_OPEN);
    }
}

// Read Time of flight to know if there is a robot
int detect()
{
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4 && measure.RangeMilliMeter <= 150) {
        return 1;
    } else {
        return 0;
    }
}

// Move the robot
void move()
{
    long left = 250, right = 250;

    // If don't touch the wall, turn to touch the wall ...
    if (direction == LEFT && digitalRead(SENSOR_LEFT))
        left *= COEF_DIRECTION;
    if (direction == RIGHT && digitalRead(SENSOR_RIGHT))
        right *= COEF_DIRECTION;

    analogWrite(LEFT_SERVO_PWM, left);
    analogWrite(RIGHT_SERVO_PWM, right);
}

// STOP the robot
void stop()
{
    analogWrite(LEFT_SERVO_PWM, 0);
    analogWrite(RIGHT_SERVO_PWM, 0);
}
