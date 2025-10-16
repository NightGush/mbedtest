#include "mbed.h"

AnalogIn apps0(PA_4); // first APPS sensor
AnalogIn apps1(PA_3); // second APPS sensor
AnalogIn bse(PA_5); // BSE (brake) sensor
DigitalIn cockpit(PA_9); // Cockpit Switch
DigitalOut rtdbuzz(PA_10); // RTD Buzzer

Timer errortime; // APPS error timer
Timer bsetime; // BSE error timer

int main() {
   printf("Hello, World!\n");

   // define the min and max voltages of each APPS
   // First APPS
   float apps0vmin = 0.25;
   float apps0vmax = 2.25;
   // Second APPS
   float apps1vmin = 0.3;
   float apps1vmax = 2.7;

   // APPS error bools
   bool error_pending = false;
   bool error_confirmed = false;
   // BSE error bools
   bool bse_pending = false;
   bool bse_confirmed = false;

   // Start sequence
   bool break_pressed = false;
   bool switch_flipped = false;

   while (true) {
      // Wait for start sequence
      while (!switch_flipped) {
         // if break has already been pressed, check the switch
         if (break_pressed) {
            // read Cockpit Switch status (0 or 1)
            float cockpitv = cockpit.read();
            if (cockpitv == 1) {
               switch_flipped = true;
               // after the switch is flipped this block never runs again and the code continues
            }
         } else { // else check if break pressed
            // read break voltage
            float bsev = bse.read();

            if (bsev >= 0.8) {
               // Ring RTD Buzzer for 1 second when break pressed
               rtdbuzz.write(1);
               ThisThread::sleep_for(1s); // probably okay to sleep since we're not doing anything else atp
               rtdbuzz.write(0);

               break_pressed = true;
            }
         }
      }

      // Get BSE Voltage
      float bsev = bse.read()*3.3;

      // Check if <0.5V or >4.5V
      // NOTE: This can never be >4.5V on our current microcontroller
      // This is pretty much copy-paste'd from the APPS checker but for BSE
      // TODO: maybe use interrupts for this? Would that be faster?
      if (bsev < 0.5 || bse > 4.5) {
         // BSE just started to error
         if (!bse_pending && !bse_confirmed) {
            bse_pending = true;
            bsetime.reset();
            bsetime.start();
         }

         // difference has been going on for >=100ms, implausability confirmed
         if (bse_pending && bsetime.elapsed_time().count() >= 100000) {
            bse_confirmed = true;
            bse_pending = false;
            bsetime.stop();
            printf("BSE IMPLAUSABILITY DETECTED\n");
         }
      } else {
         if (error_pending || error_confirmed) {
            // implausability recovered
            bse_pending = false;
            bse_confirmed = false;
            bsetime.stop();
            bsetime.reset();
         }
      }

      // keep outputting motor instructions until BSE error confirmed
      if (!bse_confirmed) {
         // read values from sensors
         float v0 = apps0.read()*3.3;
         float v1 = apps1.read()*3.3;

         // calculate percentage of each value
         float v0ptg = round(((v0-apps0vmin) / (apps0vmax-apps0vmin))*100);
         float v1ptg = round(((v1-apps1vmin) / (apps1vmax-apps1vmin))*100);

         // clamp values between 0-100%
         // TODO: make it implausible if <min or >max
         v0ptg = max(0.f, min(v0ptg, 100.f));
         v1ptg = max(0.f, min(v1ptg, 100.f));

         // calculate percentage difference between the two
         float diff = abs(v0ptg - v1ptg);

         if (diff > 10) {
            // the pedals have just started to differ
            if (!error_pending && !error_confirmed) {
               error_pending = true;
               errortime.reset();
               errortime.start();
            }

            // difference has been going on for >=100ms, implausability confirmed
            if (error_pending && errortime.elapsed_time().count() >= 100000) {
               error_confirmed = true;
               error_pending = false;
               errortime.stop();
               printf("IMPLAUSABILITY DETECTED\n");
            }

            if (!error_confirmed) {
               // still output average until confirmed?
               float avg = (v0ptg+v1ptg)/2;
               printf("[possibly implausible] Pedal Position: %f\n", avg);
            } else {
               printf("[IMPLAUSABLE] Pedal Position: 0\n");
            }
         } else {
            if (error_pending || error_confirmed) {
               // implausability recovered
               error_pending = false;
               error_confirmed = false;
               errortime.stop();
               errortime.reset();
            }

            // output average
            float avg = (v0ptg+v1ptg)/2;
            printf("Pedal Position: %f\n", avg);
         }
      }
   }
   return 0;
}
