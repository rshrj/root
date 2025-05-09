/// \file
/// \ingroup tutorial_hist
/// \notebook -js
/// \preview This macro illustrates the use of the time mode on the axis
/// with different time intervals and time formats.
/// Through all this script, the time is expressed in UTC. some
/// information about this format (and others like GPS) may be found at
/// <a href="http://tycho.usno.navy.mil/systime.html">http://tycho.usno.navy.mil/systime.html</a>
///  or
/// <a href="http://www.topology.org/sci/time.html">http://www.topology.org/sci/time.html</a>
///
/// The start time is: almost NOW (the time at which the script is executed)
/// actually, the nearest preceding hour beginning.
/// The time is in general expressed in UTC time with the C time() function
/// This will obviously most of the time not be the time displayed on your watch
/// since it is a universal time. See the C time functions for converting this time
/// into more useful structures.
///
/// \macro_image
/// \macro_code
///
/// \author Damir Buskulic

#include <time.h>

void hist061_TH1_timeonaxis()
{

   time_t script_time;
   script_time = time(0);
   script_time = 3600 * (int)(script_time / 3600);

   // The time offset is the one that will be used by all graphs.
   // If one changes it, it will be changed even on the graphs already defined
   gStyle->SetTimeOffset(script_time);

   auto ct = new TCanvas("ct", "Time on axis", 10, 10, 700, 900);
   ct->Divide(1, 3);

   int i;

   // ### Build a signal: noisy damped sine
   //         Time interval: 30 minutes

   gStyle->SetTitleH(0.08);
   float noise;
   auto ht = new TH1F("ht", "Love at first sight", 3000, 0., 2000.);
   for (i = 1; i < 3000; i++) {
      noise = gRandom->Gaus(0, 120);
      if (i > 700) {
         noise += 1000 * sin((i - 700) * 6.28 / 30) * exp((double)(700 - i) / 300);
      }
      ht->SetBinContent(i, noise);
   }
   ct->cd(1);
   ht->SetLineColor(2);
   ht->GetXaxis()->SetLabelSize(0.05);
   ht->Draw();
   // Sets time on the X axis
   // The time used is the one set as time offset added to the value
   // of the axis. This is converted into day/month/year hour:min:sec and
   // a reasonable tick interval value is chosen.
   ht->GetXaxis()->SetTimeDisplay(1);

   // ### Build a simple graph beginning at a different time
   //         Time interval: 5 seconds

   float x[100], t[100];
   for (i = 0; i < 100; i++) {
      x[i] = sin(i * 4 * 3.1415926 / 50) * exp(-(double)i / 20);
      t[i] = 6000 + (double)i / 20;
   }
   auto gt = new TGraph(100, t, x);
   gt->SetTitle("Politics");
   ct->cd(2);
   gt->SetLineColor(5);
   gt->SetLineWidth(2);
   gt->Draw("AL");
   gt->GetXaxis()->SetLabelSize(0.05);
   // Sets time on the X axis
   gt->GetXaxis()->SetTimeDisplay(1);
   gPad->Modified();

   // ### Build a second simple graph for a very long time interval
   //         Time interval: a few years

   auto gt2 = new TGraph();
   TDatime dateBegin(2000, 1, 1, 0, 0, 0);
   for (i = 0; i < 10; i++) {
      TDatime datePnt(2000 + i, 1, 1, 0, 0, 0);
      gt2->AddPoint(datePnt.Convert() - dateBegin.Convert(), 100 + gRandom->Gaus(500, 100) * i);
   }
   gt2->SetTitle("Number of monkeys on the moon");
   ct->cd(3);
   gt2->SetMarkerColor(4);
   gt2->SetMarkerStyle(29);
   gt2->SetMarkerSize(1.3);
   gt2->Draw("AP");
   gt2->GetXaxis()->SetLabelSize(0.04);
   gt2->GetXaxis()->SetNdivisions(10);
   // Sets time on the X axis
   gt2->GetXaxis()->SetTimeDisplay(1);

   // One can choose a different time format than the one chosen by default
   // The time format is the same as the one of the C strftime() function
   // It's a string containing the following formats :
   //
   //    for date :
   //      %a abbreviated weekday name
   //      %b abbreviated month name
   //      %d day of the month (01-31)
   //      %m month (01-12)
   //      %y year without century
   //      %Y year with century
   //
   //    for time :
   //      %H hour (24-hour clock)
   //      %I hour (12-hour clock)
   //      %p local equivalent of AM or PM
   //      %M minute (00-59)
   //      %S seconds (00-61)
   //      %% %
   // The other characters are output as is.

   gt2->GetXaxis()->SetTimeFormat("%d/%m/%Y %F2000-01-01 00:00:00");
}
