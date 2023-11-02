/*
 * require libraries:
 * https://github.com/moononournation/Arduino_GFX.git
 * https://github.com/earlephilhower/ESP8266Audio.git
 * https://github.com/bitbank2/JPEGDEC.git
 */
#define FPS 24
#define MJPEG_FILENAME "/280_24fps.mjpeg"
#define MJPEG_BUFFER_SIZE (280 * 240 * 2)
 /*
  * Connect the SD card to the following pins:
  *
  * SD Card | ESP32
  *    D2       12
  *    D3       13
  *    CMD      15
  *    VSS      GND
  *    VDD      3.3V
  *    CLK      14
  *    VSS      GND
  *    D0       2  (add 1K pull up after flashing)
  *    D1       4
  */
#define SDMMC_CMD 15 // SDMMC CMD   / SPI MOSI
#define SDMMC_CLK 14 // SDMMC CLK   / SPI SCK
#define SDMMC_D0 2
#include <WiFi.h>
#include <FS.h>
#include <FFat.h>
#include <SD_MMC.h>

  /* Arduino_GFX */
#include <Arduino_GFX_Library.h>
#define TFT_BL 20
Arduino_DataBus* bus = new Arduino_ESP32SPI(41 /* DC */, 40 /* CS */, 11 /* SCK */, 12/* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_GFX* gfx = new Arduino_ST7789(
    bus, 42 /* RST */, 0 /* rotation */, true /* IPS */,
    240 /* width */, 280 /* height */,
    0 /* col offset 1 */, 20 /* row offset 1 */,
    0 /* col offset 2 */, 20 /* row offset 2 */);


/* MJPEG Video */
#include "MjpegClass.h"
static MjpegClass mjpeg;

/* variables */
static unsigned long next_frame = 0;
static int skipped_frames = 0;
static unsigned long total_play_audio_ms = 0;
static unsigned long total_read_video_ms = 0;
static unsigned long total_show_video_ms = 0;
static unsigned long start_ms, curr_ms, next_frame_ms;

// pixel drawing callback
static int drawMCU(JPEGDRAW* pDraw)
{
    // Serial.printf("Draw pos = (%d, %d), size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
    unsigned long s = millis();
    gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    total_show_video_ms += millis() - s;
    return 1;
} /* drawMCU() */

void setup()
{
    WiFi.mode(WIFI_OFF);
    Serial.begin(115200);

    // Init Video
    gfx->begin(80000000);
    gfx->fillScreen(BLACK);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    // Init FS
    
    //if (!FFat.begin())
        // if ((!SD_MMC.begin()) && (!SD_MMC.begin())) /* 4-bit SD bus mode */
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0);
         if ((!SD_MMC.begin("/sdcard", true)) && (!SD_MMC.begin("/sdcard", true))) /* 1-bit SD bus mode */
    {
        Serial.println(F("ERROR: File system mount failed!"));
        gfx->println(F("ERROR: File system mount failed!"));
    }
    else
    {
        //aFile = new AudioFileSourceFS(FFat, MP3_FILENAME);
        // aFile = new AudioFileSourceFS(SD_MMC, MP3_FILENAME);
        //out = new AudioOutputI2S(0, 0, 64); // Output to builtInDAC
        //out->SetPinout(26, 25, 32);
        //out->SetGain(0.2);
        //mp3 = new AudioGeneratorMP3();

        File vFile = SD_MMC.open(MJPEG_FILENAME);
        // File vFile = SD_MMC.open(MJPEG_FILENAME);
        if (!vFile || vFile.isDirectory())
        {
            Serial.println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
            gfx->println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
        }
        else
        {
            Serial.println(F("PCM audio MJPEG video start"));

            // init Video
            mjpeg.setup(&vFile, MJPEG_BUFFER_SIZE, drawMCU,
                true /* enableDecodeMultiTask */,
                true /* enableDrawMultiTask */,
                true /* useBigEndian */);

            
            start_ms = millis();
            curr_ms = start_ms;
            next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
            while (vFile.available())
            {
                // Read video
                mjpeg.readMjpegBuf();
                total_read_video_ms += millis() - curr_ms;

                if (millis() < next_frame_ms) // check show frame or skip frame
                {
                    // Play video
                    mjpeg.drawJpg();
                }
                else
                {
                    ++skipped_frames;
                    Serial.println(F("Skip frame"));
                }
                curr_ms = millis();

                // Play audio
               /* if ((mp3->isRunning()) && (!mp3->loop()))
                {
                    mp3->stop();
                }*/
                total_play_audio_ms += millis() - curr_ms;

                while (millis() < next_frame_ms)
                {
                    vTaskDelay(1);
                }

                curr_ms = millis();
                next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
                //Serial.println(next_frame);
                /*gfx->setCursor(0, 0);
                gfx->setTextColor(WHITE);
                gfx->printf("Played frames: %d\n", next_frame);*/
            }
            int time_used = millis() - start_ms;
            int total_frames = next_frame - 1;
            Serial.println(F("PCM audio MJPEG video end"));
            vFile.close();
            int played_frames = total_frames - skipped_frames;
            float fps = 1000.0 * played_frames / time_used;
            Serial.printf("Played frames: %d\n", played_frames);
            Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / total_frames);
            Serial.printf("Time used: %d ms\n", time_used);
            Serial.printf("Expected FPS: %d\n", FPS);
            Serial.printf("Actual FPS: %0.1f\n", fps);
            Serial.printf("Play MP3: %lu ms (%0.1f %%)\n", total_play_audio_ms, 100.0 * total_play_audio_ms / time_used);
            Serial.printf("SDMMC read MJPEG: %lu ms (%0.1f %%)\n", total_read_video_ms, 100.0 * total_read_video_ms / time_used);
            Serial.printf("Decode video: %lu ms (%0.1f %%)\n", total_decode_video_ms, 100.0 * total_decode_video_ms / time_used);
            Serial.printf("Show video: %lu ms (%0.1f %%)\n", total_show_video_ms, 100.0 * total_show_video_ms / time_used);

            // wait last frame finished
            delay(200);

#define CHART_MARGIN 24
#define LEGEND_A_COLOR 0xE0C3
#define LEGEND_B_COLOR 0x33F7
#define LEGEND_C_COLOR 0x4D69
#define LEGEND_D_COLOR 0x9A74
#define LEGEND_E_COLOR 0xFBE0
#define LEGEND_F_COLOR 0xFFE6
#define LEGEND_G_COLOR 0xA2A5
            gfx->setCursor(0, 0);
            gfx->setTextColor(WHITE);
            gfx->printf("Played frames: %d\n", played_frames);
            gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / total_frames);
            gfx->printf("Actual FPS: %0.1f\n\n", fps);
            int16_t r1 = ((gfx->height() - CHART_MARGIN - CHART_MARGIN) / 2);
            int16_t r2 = r1 / 2;
            int16_t cx = gfx->width() - gfx->height() + CHART_MARGIN + CHART_MARGIN - 1 + r1;
            int16_t cy = r1 + CHART_MARGIN;

            float arc_start1 = 0;
            float arc_end1 = arc_start1 + max(2.0, 360.0 * total_play_audio_ms / time_used);
            for (int i = arc_start1 + 1; i < arc_end1; i += 2)
            {
                gfx->fillArc(cx, cy, r1, r2, arc_start1 - 90.0, i - 90.0, LEGEND_A_COLOR);
            }
            gfx->fillArc(cx, cy, r1, r2, arc_start1 - 90.0, arc_end1 - 90.0, LEGEND_A_COLOR);
            gfx->setTextColor(LEGEND_A_COLOR);
            gfx->printf("Play MP3:\n%0.1f %%\n", 100.0 * total_play_audio_ms / time_used);

            float arc_start2 = arc_end1;
            float arc_end2 = arc_start2 + max(2.0, 360.0 * total_read_video_ms / time_used);
            for (int i = arc_start2 + 1; i < arc_end2; i += 2)
            {
                gfx->fillArc(cx, cy, r1, r2, arc_start2 - 90.0, i - 90.0, LEGEND_B_COLOR);
            }
            gfx->fillArc(cx, cy, r1, r2, arc_start2 - 90.0, arc_end2 - 90.0, LEGEND_B_COLOR);
            gfx->setTextColor(LEGEND_B_COLOR);
            gfx->printf("Read MJPEG:\n%0.1f %%\n", 100.0 * total_read_video_ms / time_used);

            float arc_start3 = arc_end2;
            float arc_end3 = arc_start3 + max(2.0, 360.0 * total_decode_video_ms / time_used);
            for (int i = arc_start3 + 1; i < arc_end3; i += 2)
            {
                gfx->fillArc(cx, cy, r2, 0, arc_start3 - 90.0, i - 90.0, LEGEND_C_COLOR);
            }
            gfx->fillArc(cx, cy, r2, 0, arc_start3 - 90.0, arc_end3 - 90.0, LEGEND_C_COLOR);
            gfx->setTextColor(LEGEND_C_COLOR);
            gfx->printf("Decode video:\n%0.1f %%\n", 100.0 * total_decode_video_ms / time_used);

            float arc_start4 = arc_end2;
            float arc_end4 = arc_start4 + max(2.0, 360.0 * total_show_video_ms / time_used);
            for (int i = arc_start4 + 1; i < arc_end4; i += 2)
            {
                gfx->fillArc(cx, cy, r1, r2, arc_start4 - 90.0, i - 90.0, LEGEND_D_COLOR);
            }
            gfx->fillArc(cx, cy, r1, r2, arc_start4 - 90.0, arc_end4 - 90.0, LEGEND_D_COLOR);
            gfx->setTextColor(LEGEND_D_COLOR);
            gfx->printf("Play video:\n%0.1f %%\n", 100.0 * total_show_video_ms / time_used);
        }
    }
#ifdef TFT_BL
    delay(60000);
    ledcDetachPin(TFT_BL);
#endif
    //gfx->displayOff();
    //esp_deep_sleep_start();
}

void loop()
{
}
