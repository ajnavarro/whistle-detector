// Code based on https://github.com/nicolacimmino/WhistleControl/blob/master/WhistleDetect/WhistleDetect.ino

#include <Arduino.h>
#include <arduinoFFT.h>

// This is the amount of peaks detected, in frequency domain, from
// the original melody sample.
#define KEY_LEN 200

// This is the amount of times the user should repeat the original
// whistled melody.
#define VERIFICATION_COUNT 2

// Peaks in frequency domain from the original melody.
double key[KEY_LEN];

// Estimated during the verificatin phase keeps into account
//  the ability of the user to repeat the melody consistently.
int expectedError = 0;

// How main bins a detected peak can be off from the expected
// and still be considered valid.
#define DETECT_ERROR_HYSTERESIS 5

#define SAMPLES 128             //Must be a power of 2
#define SAMPLING_FREQUENCY 1000 //Hz, must be less than 10000 due to ADC

unsigned int sampling_period_us;
unsigned long currentMicros, previousMicros;

double vReal[SAMPLES];
double vImag[SAMPLES];

arduinoFFT FFT = arduinoFFT();

int getPeakLocation()
{
    /*SAMPLING*/
    for (int i = 0; i < SAMPLES; i++)
    {
        currentMicros = micros() - previousMicros;
        previousMicros = currentMicros;

        vReal[i] = analogRead(0);
        vImag[i] = 0;

        while (micros() < (currentMicros + sampling_period_us))
        {
        }
    }

    /*FFT*/
    FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

    // Peak detector with threshold at 0.1
    // Detects the FFT bin with the highest
    // energy above threshold.
    float peak = 1500;
    int peakLocation = -1;
    for (int i = 5; i < (SAMPLES / 2); i++)
    {
        if (vReal[i] > peak)
        {
            peak = vReal[i];
            peakLocation = i;
        }
    }

    return peakLocation;
}

/*
 * Here we sample the incoming sound and estimate how much it maches
 * with the original one. Sampling, FFT and peak detection is done
 * as in the learning phase. Cumulative error is then calculated after
 * after passing it through a non linear function to allow for a small
 * error of few FFT bins. 
 */
int detectMelody()
{
    while (getPeakLocation() < 0)
    {
        yield();
    };

    int cumulativeError = 0;
    for (int ix = 0; ix < KEY_LEN; ix++)
    {
        int peakLocation = getPeakLocation();
        int error = abs(key[ix] - peakLocation);
        cumulativeError += (error > DETECT_ERROR_HYSTERESIS) ? error : 0;
        yield();
    }

    return cumulativeError;
}

int train()
{
    Serial.println("START TRAINING");
    while (getPeakLocation() < 0)
    {
        yield();
    };

    for (int ix = 0; ix < KEY_LEN; ix++)
    {
        key[ix] = getPeakLocation();
        yield();
    }

    int averageError = 0;
    for (int ix = 0; ix < VERIFICATION_COUNT; ix++)
    {
        delay(1000);
        Serial.println("Verify training");
        int error = detectMelody();
        if (error > 3000)
        {
            Serial.println("Training fail");
            Serial.print("ERROR: ");
            Serial.println(error);
            ix--;
            continue;
        }
        else
        {
            Serial.println("Training ok");
            Serial.print("Error val: ");
            Serial.println(error);
        }
        averageError += error;
    }
    averageError = averageError / VERIFICATION_COUNT;
    return averageError;
}

void setup()
{
    Serial.begin(115200);

    sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));

    // Do the training once at startup and estimate the
    // expected max error as 10% more of the average
    // error detected in training.
    expectedError = train() * 1.1;
}

void loop()
{

    Serial.println("NOW LISTENING");
    // Detect the melody and report success if the
    //  error was less than the maximum.
    int error = detectMelody();

    if (error < expectedError)
    {
        Serial.println("CORRECT AUDIO");
        Serial.println("LISTENING AGAIN");
    }
    else
    {
        Serial.println("WRONG AUDIO");
    }

    yield();
}
